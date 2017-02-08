#include <stdio.h>        //printf etc
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>         //duration_cast system_clock
#include <iterator>       //next
#include <iomanip>        //setfill setw
#include <unordered_map>
#include <boost/filesystem.hpp>

int main(int argc, char const *argv[])
{
  using namespace std;
  try{
    //
    // Input rules
    //
    if(argc!=5){
      throw "Require 4 arguments.";
    }
    boost::filesystem::path workingDir(boost::filesystem::current_path());
    std::ifstream attributesTable(argv[1]);
    std::ifstream readStream(argv[2]);
    boost::filesystem::path verifierPath(argv[3]);
    boost::filesystem::path outputPath(argv[4]);

    if(attributesTable.fail()){
      throw "Failed to load AttributesTable";
    }
    if(readStream.fail()){
      throw "Failed to load InitialStates";
    }
    auto start = std::chrono::system_clock::now();

    std::ofstream statesHeader(outputPath.string()+"/InitialStates-header.csv",std::ios::trunc);
    std::ofstream rulesHeader(outputPath.string()+"/rules-header.csv",std::ios::trunc);

    string str;
    string header;
    for(auto lineCount=0;getline(attributesTable,str);lineCount++){
      header+="val"+to_string(lineCount)+",";
    }
    header.pop_back();
    statesHeader<<":ID,"<<header;
    rulesHeader<<":START_ID,rule,:END_ID";

    unordered_map<string,string> stateTable;
    for(string str;getline(readStream,str) && str.size()>0;){
      auto commaPosition = str.find_first_of(",");
      stateTable.emplace(str.substr(commaPosition,*str.rbegin()),str.substr(0,commaPosition));
    }
    const auto stateMaxNum=stateTable.size();

    // 上から処理済み状態数、生成された遷移数、処理が完了したスレッド数、検証器を再実行した回数
    // 進捗状況確認/終了判定に用いる
    atomic<int> stateCount(0);
    atomic<int> relationCount(0);
    atomic<int> finishedThread(0);
    atomic<int> retryCount(0);

    //
    // プログレスバー
    //
    auto progressRate=0.0;
    string progressGauge="";
    std::thread progressBar([&]{
      auto alreadyProcessingNum=0;
      while(progressRate<1.0){
        progressRate=static_cast<double>(stateCount)/stateMaxNum;
        progressGauge=string(static_cast<int>(progressRate*100/4.0),'|')+string(25-static_cast<int>(progressRate*100/4.0),'-');
        printf("\r[%s]%.3f%%",progressGauge.c_str(),progressRate*100);
        printf(" %d states/s",(stateCount-alreadyProcessingNum)/3);
        alreadyProcessingNum=stateCount;
        fflush(stdout);
        std::this_thread::sleep_for(std::chrono::microseconds(3000000));
      }
    });

    progressBar.detach();

    //
    // ワーカースレッド
    //
    auto processingData = [&](const int startNum,const int endNum,const int number){
      int ruleNum;
      char resultStr[200];
      string preID,postID;
      string rulesCSVPath=outputPath.string()+"/rules"+to_string(number)+".csv";
      FILE* rulesCSV = fopen(rulesCSVPath.c_str(),"w");
      FILE* verifyResult;
      std::string command;
      auto endIter=std::next(stateTable.begin(),endNum);
      for(auto readStr = std::next(stateTable.begin(),startNum);readStr!=endIter;++readStr){
        command="echo "+readStr->first+"|"+verifierPath.string()+"/a.out 2>&1";
        verifyResult = popen(command.c_str(),"r");
        char resultBuf[200];
        for(;;){
          if(std::fgets(resultBuf,sizeof(resultBuf),verifyResult)==NULL){
            if(feof(verifyResult)){
              pclose(verifyResult);
              retryCount++;
              verifyResult = popen(command.c_str(),"r");
              continue;
            }
            printf("Unreachable\n");
          }
          if(resultBuf[0]!='@'){
            break;
          }
          sscanf(resultBuf,"@%d%s",&ruleNum,resultStr);
          fprintf(rulesCSV,"%s,%d,%s\n",readStr->second.c_str(),ruleNum,stateTable.at(resultStr).c_str());
          ++relationCount;
        }
        pclose(verifyResult);
        stateCount++;
      }

      fclose(rulesCSV);
      finishedThread++;
    };

    // 4は現状決め打ち
    const auto threadNum=4;
    const auto interval = static_cast<int>(stateMaxNum/threadNum);
    vector<thread> threads;
    for(auto i=0;i<threadNum;i++){
      // 均等に割り振れない場合用 最後のスレッドが処理する状態の個数で帳尻を合わせる
      // ex) 1000状態を3スレッド→[0-333(333状態)][333-666(333状態)][666-1000(334状態)]
      //     [0-333]をthreadに渡したとき、そのスレッドはstateTableの0-332番目の要素を参照し処理(≒333は'番兵')
      if(i+1==threadNum){
        threads.emplace_back(processingData,interval*i,stateMaxNum,i);
        break;
      }
      threads.emplace_back(processingData,interval*i,interval*(i+1),i);
    }

    while (finishedThread!=threads.size()) {
      std::this_thread::yield();
    }

    for(auto& i : threads){
      i.join();
    }

    //
    // result
    //
    auto end = std::chrono::system_clock::now();
    auto diff = end - start;
    progressGauge=string(25,'|');
    cout<<"\r["<<progressGauge.c_str()<<"]100%   "<<endl;
    cout<<endl<<"🍺\a\a\a\a\a"<<endl;
    std::cout << stateCount << " States / " << relationCount << " Relationships" <<" (Retry: "<<retryCount<<")"<<endl;
    std::cout << "elapsed time = "
              << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()
              << " msec("
              << std::setfill ('0') << std::setw (2)
              << std::chrono::duration_cast<std::chrono::hours>(diff).count()<<"h "
              << std::setfill ('0') << std::setw (2)
              << std::chrono::duration_cast<std::chrono::minutes>(diff).count()%60<<"m "
              << std::setfill ('0') << std::setw (2)
              << std::chrono::duration_cast<std::chrono::seconds>(diff).count()%60<<"s"
              << ")" << std::endl;

  }catch(const char* str){
    std::cerr<<"ERROR: "<<str<<std::endl;
    return -1;
  }
  return 0;
}