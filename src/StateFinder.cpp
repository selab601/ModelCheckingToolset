#include <neo4j-client.h>
#include <iostream>
#include <fstream>
#include <errno.h>
#include <stdio.h>
#include <vector>
#include <memory>
#include <list>
#include <forward_list>
#include <stack>
#include <tuple>
#include <string>
#include <map>
#include <chrono>
#include <iomanip>
#include <functional>
#include <iterator>
#include <unordered_set>
#include <unordered_map>
#include <fstream>
#include <algorithm>

#include <boost/algorithm/string.hpp>
#include <boost/bimap/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/filesystem.hpp>
#include <boost/iterator/counting_iterator.hpp>

int main(int argc, char const *argv[])
{
  using std::string;
  using std::vector;
  using std::forward_list;
  using std::pair;
  using std::tuple;
  using std::cout;
  using std::endl;
  using std::getline;
  using std::get;
  using std::function;
  using std::make_tuple;
  using std::to_string;
  using std::unordered_set;
  using std::stack;
  using std::unordered_map;
  using std::advance;
  using std::make_pair;
  using boost::algorithm::split;
  using boost::algorithm::join;
  using namespace boost::bimaps;
  using namespace boost::filesystem;
  try{
    if(argc!=4){
      throw "Usage: [MergedInfoDir] [StartState] [EndState]";
    }
    path workingDirPath(argv[1]);
    if(!exists(workingDirPath)){
      throw "Failed to open file.";
    }
    boost::filesystem::ifstream initialStateAStream(workingDirPath/"ruleCuts_A_result/InitialStates");
    boost::filesystem::ifstream initialStateBStream(workingDirPath/"ruleCuts_B_result/InitialStates");
    boost::filesystem::ifstream decodeTableAStream(workingDirPath/"decodeTable_A");
    boost::filesystem::ifstream decodeTableBStream(workingDirPath/"decodeTable_B");

    if(initialStateAStream.fail()){
      throw "Failed to load "+workingDirPath.string()+"ruleCuts_A_result/InitialStates";
    }
    if(initialStateBStream.fail()){
      throw "Failed to load "+workingDirPath.string()+"ruleCuts_B_result/InitialStates";
    }
    neo4j_client_init();
    neo4j_connection_t *connection = neo4j_connect("neo4j://neo4j:neo4J@localhost:7687", NULL, NEO4J_INSECURE);
    if (connection == NULL)
    {
        neo4j_perror(stderr, errno, "Connection failed");
        return EXIT_FAILURE;
    }

    neo4j_session_t *session = neo4j_new_session(connection);
    if (session == NULL)
    {
        neo4j_perror(stderr, errno, "Failed to start session");
        return EXIT_FAILURE;
    }
    auto start = std::chrono::system_clock::now();
    // string -> 状態ID string -> カンマから始まる属性値列
    typedef bimap<string,string>::value_type StateID;

    // ID <-> 属性値列 対応参照用
    typedef bimap<unordered_set_of<string>,unordered_set_of<string>> StateIDTable;

    string startState(argv[2]);
    string endState(argv[3]);
    // 到達可能パスの候補
    // vector<pair<vector<int>,string>>
    // 状態と、バックトラックに使われた遷移 ( ) -> 
    // なお、目標状態のsecondはなし
    stack<pair<string,int>> path;
    unordered_set<string> alreadyVisitedGlobalState;

    // 部分状態集合 first:A second:B
    pair<StateIDTable,StateIDTable> localTable;

    // 強連結成分対応参照用 <状態ID,<強連結成分ID,強連結から脱出した先のID>>
    //  unordered_map<string,pair<string,string>> sccTable;

    for(string str;getline(initialStateAStream,str) && str.size()>0;){
      auto commaPosition = str.find_first_of(",");
      string id=str.substr(0,commaPosition);
      string state=str.substr(commaPosition,*str.rbegin());
      boost::trim_if(state,boost::is_any_of(","));
      localTable.first.insert(StateID(id,state));
    }
    for(string str;getline(initialStateBStream,str) && str.size()>0;){
      auto commaPosition = str.find_first_of(",");
      string id=str.substr(0,commaPosition);
      string state=str.substr(commaPosition,*str.rbegin());
      boost::trim_if(state,boost::is_any_of(","));
      localTable.second.insert(StateID(id,state));
    }

    string decodeStrA;
    string decodeStrB;
    vector<string> decodeTableA;
    vector<string> decodeTableB;

    getline(decodeTableAStream,decodeStrA);
    getline(decodeTableAStream,decodeStrA);
    getline(decodeTableBStream,decodeStrB);
    getline(decodeTableBStream,decodeStrB);

    split(decodeTableA,decodeStrA,boost::is_any_of(","));
    split(decodeTableB,decodeStrB,boost::is_any_of(","));

    auto decode=[](const vector<string>& globalState,const vector<string>& decodeTable)->string{
      vector<string> localState;
      auto decodeIter=decodeTable.begin();
      for(auto i=0;i<globalState.size();i++){
        if(to_string(i)==*decodeIter){
          localState.emplace_back(globalState[i]);
          advance(decodeIter,1);
        }
      }
      return join(localState,",");
    };

    // localStateの変更をglobalStateに反映する
    auto reflect=[](vector<string> globalState,const vector<string>& localState,const vector<string>& decodeTable)->string{
      auto decodeIter=decodeTable.begin();
      for(auto iter=localState.begin();iter!=localState.end();iter++,decodeIter++){
        globalState[stoi(*decodeIter)]=*iter;
      }
      return join(globalState,",");
    };

    auto backTrack=[&](vector<pair<int,int>>& resultVec,string partName,const int id){
      string cql="match p=shortestPath((n:"+partName+")-[r:Rule*..1]->(m:"+partName+")) where m.localID="+to_string(id)+" with n, m, reduce(s = '', rel in r | s + rel.rule) as rels RETURN n.localID,toInt(rels)";
      neo4j_result_stream_t *results = neo4j_run(session,cql.c_str(),neo4j_null);
      if(results==NULL){
        neo4j_perror(stderr, errno, "Failed to run statement");
      }
      neo4j_result_t *result;
      while((result=neo4j_fetch_next(results))!=NULL){
        if(neo4j_is_null(neo4j_result_field(result, 0)))break;
          resultVec.emplace_back(make_pair(neo4j_int_value(neo4j_result_field(result, 0)),neo4j_int_value(neo4j_result_field(result, 1))));
      }
    };

    path.emplace(make_pair(endState,-1));
    int count=0;
    system("date");

    while(!path.empty()){
      // スタックの一番上を参照する
      auto currentNode=path.top();
      // 現在地をすでに訪れたことにする
      alreadyVisitedGlobalState.emplace(currentNode.first);

      // 全体状態の属性をvectorにする
      vector<string> globalStateVec;
      split(globalStateVec,currentNode.first,boost::is_any_of(","));

      // 部分化する
      auto decodedA=decode(globalStateVec,decodeTableA);
      auto decodedB=decode(globalStateVec,decodeTableB);
      vector<pair<int,int>> btResultsA;
      vector<pair<int,int>> btResultsB;
          // cout<<"参照1A";
      // 各部分遷移にバックトラックをかけた後の結果を取得する
      backTrack(btResultsA,"A",stoi(localTable.first.right.at(decodedA)));
          // cout<<"参照1B";

      backTrack(btResultsB,"B",stoi(localTable.second.right.at(decodedB)));


      // 得られた半状態について、片っぱしから全体化してゆき、行ってないノードを探す
      vector<tuple<vector<pair<int,int>>,vector<string>,StateIDTable>> searchTarget{{btResultsA,decodeTableA,localTable.first},{btResultsB,decodeTableB,localTable.second}};
      string nextGlobalState="";
      for(auto btResults:searchTarget){
        for(auto& btResult:get<0>(btResults)){
          vector<string> localStateVec;
          // cout<<"btResult.first:"<<btResult.first;
          split(localStateVec,get<2>(btResults).left.at(to_string(btResult.first)),boost::is_any_of(","));
          string resultGlobalstr=reflect(globalStateVec,localStateVec,get<1>(btResults));
          // そのようなノードがあったら、そっちに向かう
          if(alreadyVisitedGlobalState.count(resultGlobalstr)==0){
            nextGlobalState=resultGlobalstr;
            path.emplace(make_pair(resultGlobalstr,btResult.second));
            vector<string> tmp;
            split(tmp,resultGlobalstr,boost::is_any_of(","));
            break; 
          }
        }
        if(!nextGlobalState.empty()){
          break;
        }
      }
      if(nextGlobalState==startState){
        break;
      }
      // 行ける場所がなければ、pop
      if(nextGlobalState.empty()){
        path.pop();
      }
      auto diff = std::chrono::system_clock::now() - start;
      std::cerr<<"\rVisited: "<<alreadyVisitedGlobalState.size()
                << " / elapsed time = "
                << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()
                << " msec("
                << std::setfill ('0') << std::setw (2)
                << std::chrono::duration_cast<std::chrono::hours>(diff).count()<<"h "
                << std::setfill ('0') << std::setw (2)
                << std::chrono::duration_cast<std::chrono::minutes>(diff).count()%60<<"m "
                << std::setfill ('0') << std::setw (2)
                << std::chrono::duration_cast<std::chrono::seconds>(diff).count()%60<<"s"
                << ")";
    }

    auto diff = std::chrono::system_clock::now() - start;
    std::cerr<<"\r--------------------------------------------------------"<<endl;
    std::cerr<<"  Result"<<endl;
    std::cerr<<"--------------------------------------------------------"<<endl;

    // ゴールノード分のパス(==empty())が含まれるため、-1
    int pathLength=path.size()-1;

    cout<<" start("<<startState<<")"<<" ---> "<<"goal("<<endState<<")"<<endl;
    if(!path.empty()){
      cout<<" [Start]🚩 "<<"->";
      while(!path.empty()){
        if(path.top().second>-1)cout<<"["<<path.top().second<<"]->";
        path.pop();
      }
      cout<<" 🏁  [Goal]" <<endl;
    }else{
      std::cerr<<" パスはなし"<<endl;
    }
    std::cerr<<" 訪れた状態の個数\t : "<< alreadyVisitedGlobalState.size()<<endl
             <<" 適用ルールの個数\t : "<< ((pathLength>0)?to_string(pathLength):"-") <<endl
             <<" 処理時間\t\t : "    << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()
             << " msec("
             << std::setfill ('0') << std::setw (2)
             << std::chrono::duration_cast<std::chrono::hours>(diff).count()<<"h "
             << std::setfill ('0') << std::setw (2)
             << std::chrono::duration_cast<std::chrono::minutes>(diff).count()%60<<"m "
             << std::setfill ('0') << std::setw (2)
             << std::chrono::duration_cast<std::chrono::seconds>(diff).count()%60<<"s"
             << ")" << std::endl;
    std::cerr<<"--------------------------------------------------------"<<endl;

    neo4j_end_session(session);
    neo4j_close(connection);
    neo4j_client_cleanup();
    return 0;

  }catch(const char* str){
    cout<<str<<endl;
    return -1;
  }
}