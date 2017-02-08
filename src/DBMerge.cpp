#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <string>
#include <random>
#include <iterator>

#include <algorithm>
#include <functional>
#include <iomanip>

#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

// 改名:InitStateConverter

int main(int argc, char const *argv[])
{
  try{
    if(argc!=2){
      throw "は？";
    }
    boost::filesystem::path dirPath(argv[1]);

    if(!boost::filesystem::exists(dirPath)){
      throw "ディレクトリがない";
    }
    if(!boost::filesystem::is_directory(dirPath)){
      throw "ディレクトリじゃない";
    }
    std::vector<std::string> partNames{"ruleCuts_A_result","ruleCuts_B_result"};
    std::vector<std::string> identifier{"A","B"};
    if(!boost::filesystem::is_directory(dirPath/partNames[0])
       || !boost::filesystem::is_directory(dirPath/partNames[1])
      ){
      throw "ルールカットが足りない";
    }

    int counter=0;
    int ruleCounter=0;
    boost::filesystem::path outputDirPath(dirPath/"ruleCuts_result_merged");

    boost::system::error_code error;
    const bool result = boost::filesystem::create_directory(outputDirPath, error);
    if (!result || error) {
      throw "ディレクトリの作成に失敗";
    }
    for(auto i=0;i<partNames.size();i++){
      std::string targetPath=dirPath.string()+"/"+partNames[i];

      std::ifstream initState(targetPath+"/InitialStates");
      std::ifstream decodeTable(dirPath.string()+"/decodeTable_"+identifier[counter]);
      // ｜
      // ｜ 変換・格納
      // ↓
      std::ofstream fixedInitState(outputDirPath.string()+"/InitialStates_"+identifier[counter],std::ios::trunc);
      std::ofstream fixedStatesHeader(outputDirPath.string()+"/StatesHeader_"+identifier[counter],std::ios::trunc);
      // InitialStates
      for(std::string str;getline(initState,str) && str.size()>0;){
        fixedInitState<<identifier[counter]<<str.substr(0,str.find_first_of(","))<<","<<str<<'\n';
      }

      // InitialStates-header
      std::vector<std::string> vec;
      std::vector<std::string> decodeVector;
      std::string header,attrDecodeString,ruleDecodeString;
      std::getline(decodeTable,ruleDecodeString);
      std::getline(decodeTable,attrDecodeString);
      boost::split(vec,attrDecodeString,boost::is_any_of(","));
      boost::split(decodeVector,ruleDecodeString,boost::is_any_of(","));

      fixedStatesHeader<<":ID,localID:Int,";
      for(auto str:vec){
        header+="val"+str+",";
      }
      header.pop_back();
      fixedStatesHeader<<header;

      for(auto j=0;boost::filesystem::exists(targetPath+"/rules"+std::to_string(j)+".csv");j++){
        std::ifstream rulesCSV(targetPath+"/rules"+std::to_string(j)+".csv");
        std::ofstream fixedRules(outputDirPath.string()+"/rules"+std::to_string(ruleCounter++)+".csv",std::ios::trunc);
        for(std::string str;getline(rulesCSV,str) && str.size()>0;){
          boost::split(vec,str,boost::is_any_of(","));
          fixedRules<<identifier[counter]<<vec[0]<<","<<decodeVector.at(std::stoi(vec[1]))<<","<<identifier[counter]<<vec[2]<<'\n';
        }
      }
      counter++;
    }
    std::ofstream rulesHeader(outputDirPath.string()+"/rules-header.csv",std::ios::trunc);
    rulesHeader<<":START_ID,rule,:END_ID";
    return 0;
  }catch(const char* str){
    std::cout<<str<<std::endl;
    return -1;
  }
}