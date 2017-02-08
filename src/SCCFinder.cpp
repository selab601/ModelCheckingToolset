#include <neo4j-client.h>
#include <iostream>
#include <errno.h>
#include <stdio.h>
#include <vector>
#include <list>
#include <stack>
#include <string>
#include <map>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <fstream>

#include <algorithm>

#include <boost/iterator/counting_iterator.hpp>

std::vector<int> getValue(neo4j_session_t* session,const char* cql){
  std::vector<int> vec;
  neo4j_result_stream_t *results = neo4j_run(session,cql,neo4j_null);
  // std::cout<<cql<<std::endl;
  if (results == NULL)
  {
      neo4j_perror(stderr, errno, "Failed to run statement");
  }

  neo4j_result_t *result;
  while((result=neo4j_fetch_next(results))!=NULL){
    if(neo4j_is_null(neo4j_result_field(result, 0)))return vec;
    vec.emplace_back(static_cast<int>(neo4j_int_value(neo4j_result_field(result, 0))));
  }
  return vec;
}

int main(int argc, char *argv[]){
    using namespace std;
    system("date");
    // std::ifstream readStream("./InitialStates_crane2f_B");
    // unordered_set<int> checker;

    neo4j_client_init();
    /* use NEO4J_INSECURE when connecting to disable TLS */
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

    unordered_map<string,string> results;

// 強連結成分分解と縮約について
// 強連結-最初のDFS
    auto start = std::chrono::system_clock::now();


  for(auto& tableID:vector<string>{"B","A"}){
    int nodeNumAfterContraction=0;
    auto result=getValue(session,string("MATCH (n) WHERE NOT (n)--() AND n:"+tableID+" return n.localID").c_str());
    auto nodeCount=*getValue(session,string("MATCH (n) where n:"+tableID+" return count(n.localID)").c_str()).begin();
    int contractedNodeNum=0;
    unordered_set<int> unsearchedID(boost::counting_iterator<int>(0), boost::counting_iterator<int>(nodeCount));

// init) 単一ノードのIDを、全て探索済みにする
    for(auto const& i : result){
      unsearchedID.erase(i);
    }
    // 強連結の、"2つのグラフ合わせて"一意なID
    int sccID=0;
    // こっからループ
    while(unsearchedID.size()!=0){
      cout<<endl<<"残りID数:"<<unsearchedID.size()<<endl;

      // status ... -1:未踏  0:到達,未評価 1以上:評価済み

      unordered_map<int,int> traversedNode;
      // 到達順序、ID
      map<int,int> traversedNodeOrder;
      // vector<int> sizesortedID;
      // 1) 未探索のIDから、IDを一つ選ぶ
      int subgraphStartID=*begin(unsearchedID);
      // 2) そのIDを含むサブグラフに含まれるIDリストを取得し、一時的な探索範囲にする
      // string getSubGraphIDListCQL("MATCH (root)-[rels*]-(b) where ID(root)="+ to_string(subgraphStartID) +" RETURN distinct ID(b)");
      string getSubGraphIDListCQL(string("match p=shortestPath((n)-[*]-(m)) where n.localID="+to_string(subgraphStartID)+" AND n:"+tableID+" AND m:"+tableID+" return m.localID").c_str());
      cout<<getSubGraphIDListCQL<<endl;
      auto subGraphIDList = getValue(session,getSubGraphIDListCQL.c_str());

      if(count(subGraphIDList.begin(),subGraphIDList.end(),subgraphStartID)==0){
        subGraphIDList.emplace_back(subgraphStartID);
      }

      for(auto i : subGraphIDList){
        traversedNode.emplace(i,-1);
      }

      // 3) 選んだIDを積む
      stack<int> destinations;

      destinations.emplace(subgraphStartID);
    // 3) 1回目の深さ優先探索の開始(終了条件:サイズ比較)
      int counter=0;
      while(counter!=subGraphIDList.size()){
        int chosenID;
        if(destinations.empty()){
          chosenID=(find_if(begin(traversedNode),end(traversedNode),[](auto const& x){
            return x.second==-1;
          }))->first;
          destinations.push(chosenID);
        }else{
        //   a) スタックをtop
          chosenID=destinations.top();
        }
        //   a) そのIDに対応するノードと、遷移先のノードの情報を取得

        auto goingList = getValue(session,string("match (d)-->(x) where d.localID="+to_string(chosenID)+" AND d:"+tableID+" AND x:"+tableID+" return distinct x.localID").c_str());
        // auto goingList = getValue(session,string("match (d) where ID(d) = "+to_string(chosenID)+"AND d:"+tableID+" OPTIONAL MATCH (d)-->(x) return distinct x.localID)").c_str());

        // b) 進める遷移がなければ(遷移がない or t[進行先id]が全て>0(=評価済み))
        if(goingList.empty() || all_of(goingList.begin(),goingList.end(),[&](int destID){
          return traversedNode[destID]!=-1;
        })){
        //   *) t[id]=(カウンタ) を到達順位にし、pop
        if(traversedNode[chosenID]<1){
          traversedNode[chosenID]=++counter;
          traversedNodeOrder.emplace(counter,chosenID);
        }
        //   *) 戻れる(t[戻り先ID]==-1,未踏)先のノードを、スタックに積む
          destinations.pop();

{
    //     cout<<"---------------"<<'\n';
    //       cout<<"ループ終了(巻き戻り)"<<'\n';
    //       cout<<"top():"<<chosenID<<'\n';
    //       cout<<"traversed:"<<'\n';
    //       for(auto i:traversedNode){
    //         if(i.second>0){
    //           cout<<i.first<<" : "<<"evaluated("<<i.second<<")"<<'\n';
    //         }
    //         if(i.second==0){
    //           cout<<i.first<<" : "<<"traversed"<<'\n';
    //         }
    //         if(i.second==-1){
    //           cout<<i.first<<" : "<<"-"<<'\n';
    //         }
    //       }
    //       cout<<"goingList: ";
    //       for(auto i:goingList){
    //         if(traversedNode[i]!=-1){
    //           cout<<"("<<i<<")"<<" ";
    //         }else{
    //           cout<<i<<" ";
    //         }
    //       }
    //       cout<<'\n';
    //       cout<<"destinations: ";
    //       for (auto dump = destinations; !dump.empty(); dump.pop())
    //           std::cout << dump.top() << ' ';
    //       cout<<'\n';
    //       cout<<"---------------"<<'\n';
}

            continue;
          }else{
        // c) 進める遷移があれば、その遷移先のノードをスタックに積み、t[いまのID]=0(到達済みだが未評価)
            for(auto i:goingList){
              if(traversedNode[i]==-1)destinations.push(i);
            }
            traversedNode[chosenID]=0;
/*
{
          cout<<"---------------"<<'\n';
          cout<<"ループ終了(進行)"<<'\n';
          cout<<"top():"<<chosenID<<'\n';
          cout<<"traversed:"<<'\n';
          for(auto i:traversedNode){
            if(i.second>0){
              cout<<i.first<<" : "<<"evaluated("<<i.second<<")"<<'\n';
            }
            if(i.second==0){
              cout<<i.first<<" : "<<"traversed"<<'\n';
            }
            if(i.second==-1){
              cout<<i.first<<" : "<<"-"<<'\n';
            }
          }
          cout<<"goingList: ";
          for(auto i:goingList){
            if(traversedNode[i]!=-1){
              cout<<"("<<i<<")"<<" ";
            }else{
              cout<<i<<" ";
            }
          }
          cout<<'\n';
          cout<<"destinations: ";
          for (auto dump=destinations;!dump.empty();dump.pop())
              std::cout << dump.top() << ' ';
          cout<<'\n';
          cout<<"---------------"<<'\n';
}
  */
        }
    }

{
        // cout<<"----------"<<'\n';
        // cout<<"DFS Part.1"<<'\n';
        // cout<<"----------"<<'\n';
        // for(auto i:traversedNodeOrder){
        //   cout<<i.first<<" : "<<i.second<<'\n';
        // }

        // for(auto i:traversedNode){
        //   if(i.second>0){
        //     cout<<i.first<<" : "<<"evaluated("<<i.second<<")"<<'\n';
        //   }
        //   if(i.second==0){
        //     cout<<i.first<<" : "<<"traversed"<<'\n';
        //   }
        //   if(i.second==-1){
        //     cout<<i.first<<" : "<<"-"<<'\n';
        //   }
        // }
}
        ////////////////////////////////////////////////////
        ////////////////////////////////////////////////////
        ////////////////////////////////////////////////////
        // 4) 2回目の深さ優先探索の開始(終了条件:サイズ比較)
        ////////////////////////////////////////////////////
        ////////////////////////////////////////////////////
        ////////////////////////////////////////////////////

        int traversedNodeSize=traversedNode.size();
        int traversedNodeIndex=static_cast<int>(traversedNodeSize);
        auto firstsearch=find_if(begin(traversedNode),end(traversedNode),[&](auto& x){
          return x.second==traversedNodeIndex;
        });

        destinations=stack<int>();
        destinations.emplace(firstsearch->first);
        // Strongly Connected Components:強連結成分

        list<tuple<int,vector<int>,vector<pair<int,int>>>> sccList;
        
        // "サブグラフ全体の"探索履歴
        unordered_set<int> traversedHistory;

      //   a) そのIDに対応するノードと、遷移先のノードの情報を取得
        // 2回目の探索は遷移を逆向きに捉えてDFS
        // while(counter2++<20){
        while(traversedNodeOrder.size()>0){ 
          // "一つのsccを探すための"探索履歴
          unordered_set<int> traversedTmp;
          vector<pair<int,int>> sourceIDOfScc;
          // while(subGraphIDList.size()>traversedHistory.size()){

         while(!destinations.empty()){
          //   a) スタックをtop
          int chosenID=destinations.top();
          traversedTmp.emplace(chosenID);
          traversedHistory.emplace(chosenID);
          auto goingList = getValue(session,string("match (x)-->(d) where d.localID="+to_string(chosenID)+" AND d:"+tableID+" AND x:"+tableID+" return distinct x.localID").c_str());
          // auto goingList = getValue(session,string("match (d) where ID(d) = "+to_string(chosenID)+" AND n:"+tableID+" OPTIONAL MATCH (x)-->(d) return distinct ID(x)").c_str());
          // b) 進める遷移がなければ(遷移がない or t[進行先id]が全て>0(=評価済み))
          if(goingList.empty()
            || all_of(goingList.begin(),goingList.end(),[&](int destID){
              return traversedHistory.count(destID)>0 || traversedTmp.count(destID)>0;
            })
            ){
        //   *) t[id]=(カウンタ) を到達順位にし、pop
        //   *) 戻れる(t[戻り先ID]==-1,未踏)先のノードを、スタックに積む
            for(auto i:goingList){
              if(traversedHistory.count(i)>0 && traversedTmp.count(i)==0 && traversedTmp.size()>1){
                // cout<<"(scc)<-"<<chosenID<<"<-"<<i<<"("<<traversedTmp.size()<<")"<<endl;
                // TODO:"脱出状態"と、"そこに至る遷移"を取得(SCC内部の脱出状態は必要ない)
                // パス導出の際には、脱出状態から、その遷移を使った先の状態を取得→SCCに入った状態と、その状態のパス探索?
                // Neo4j上の状態に、プロパティ(SCCのID)をセットするようなCQLを出力
                // ID、脱出可能ノード・遷移をファイルを保存

                // Neo4jに"強連結ノード"を作成
                // map[ID]={脱出可能ノード、遷移} をStateManagerで作る
                string scccql=string("match (m)-[r]->(n) where m.localID="+to_string(i)+" AND n.localID="+to_string(chosenID)+" AND m:"+tableID+" AND n:"+tableID+" return toInt(r.rule)");
                auto rules = getValue(session,scccql.c_str());
                sourceIDOfScc.emplace_back(make_pair(i,*rules.begin())); 
              }
             }
            destinations.pop();
            // 1回のDFSの探索要素(一筆書き) = 強連結成分
            // = destinationsがなくなるまでやる
            // 上のdestinationsの再スタート(stack<int>())は、1回のDFSが終わったあとの処理
/*{
          cout<<"--pop:--"<<'\n';
          cout<<"--traversedTmp--"<<'\n';
          for(auto i:traversedTmp){
            cout<<i<<" ";
          }
          cout<<'\n';
          cout<<"--traversedHistory--"<<'\n';
          for(auto i:traversedHistory){
            cout<<i<<" ";
          }
          cout<<'\n';
          cout<<"destinations: ";
          for (auto dump=destinations;!dump.empty();dump.pop())
              std::cout << dump.top() << ' ';
          cout<<'\n';
}*/
          }else{
            // 進行可能な条件
            // (1) 行き先のノードが探訪済み(=1)で無いこと
            for(auto i:goingList){

              // (1)
              if(traversedHistory.count(i)==0){
                destinations.push(i);
                traversedTmp.emplace(i);
                break;
              }
            }
/*{
          cout<<"--traversal:--"<<'\n';
          for(auto i:traversedTmp){
            cout<<i<<" ";
          }
          cout<<'\n';
          cout<<"destinations: ";
          for (auto dump=destinations;!dump.empty();dump.pop())
              std::cout << dump.top() << ' ';
          cout<<'\n';
}*/
          } //else 
        } //destination.empty()

      unordered_set<int> scc;
      // cout<<"traversed route:"<<'\n';
      for(auto i:traversedTmp){
        scc.emplace(i);
        traversedNodeOrder.erase(traversedNode.at(i));
        traversedHistory.emplace(i);
        // cout<<i<<" ";
      }
      // cout<<"\n残り"<<traversedNodeOrder.size()<<endl;
      // 強連結成分が2つ以上なら、それらは縮約対象
      if(scc.size()>=2){
        for(auto& i:scc){
          string cql="match(n) where n.localID="+to_string(i)+" AND n:"+tableID+" set n.sccID="+to_string(sccID);
          neo4j_result_stream_t *results = neo4j_run(session,cql.c_str(),neo4j_null);
          if(results==NULL){
             neo4j_perror(stderr, errno, "Failed to run statement");
          }
          neo4j_result_t *result;
          while((result=neo4j_fetch_next(results))!=NULL){
            if(neo4j_is_null(neo4j_result_field(result, 0)))break;
          }
        }
        sccList.emplace_back(sccID,vector<int>(begin(scc),end(scc)),sourceIDOfScc);
        sccID++;
      }
      if(traversedNodeOrder.size()>0){
        destinations=stack<int>();
        // cout<<"nextIndex:"<<traversedNodeOrder.rbegin()->second<<endl;
        destinations.emplace(traversedNodeOrder.rbegin()->second);
      }
    } // traversedNodeOrder.size()>0



      // cout<<"----------"<<'\n';
      // cout<<"DFS Part.2"<<'\n';
      // cout<<"----------"<<'\n';

      cout<<"このサブグラフの縮約対象:";;
      if(sccList.size()==0){
        cout<<"なし"<<'\n';
      }else{
        cout<<"\n";
        for(auto i:sccList){
          cerr<<"create (n:SCCNode{SCCID:"<<get<0>(i)<<"});"<<endl;
          cout<<"SCCID:"<<get<0>(i)<<"{ ";
          nodeNumAfterContraction++;
          for(auto j:get<1>(i)){
            contractedNodeNum++;
            cout<<j<<" ";
            // if(checker.count(j)==1){
            //   cout<<endl<<"ダブリ" <<endl;
            //   throw "x";
            // }
            // checker.emplace(j);
          }
          cout<<"}"<<"\n";
        }
        for(auto i:sccList){

          cout<<"SCCID:"<<get<0>(i)<<"{ ";
          for(auto j:get<1>(i)){
            cout<<j<<" ";
            cerr<<"match (sccSource),(SCCNode) ";
            cerr<<"where sccSource:"<<tableID<<" AND sccSource.localID="<<j<<" ";
            cerr<<"AND SCCNode:SCCNode AND SCCNode.SCCID="<<get<0>(i)<<" ";
            cerr<<" create (sccSource)<-[:SCCEdge]-(SCCNode);"<<endl;
          }

        }


      }

      if(sccList.size()==0){
        cout<<"なし"<<'\n';
      }else{
        cout<<"\n";
        for(auto i:sccList){

          if(get<2>(i).size()>0){
            for(auto j:get<2>(i)){
              cout<<"SCCID("<<get<0>(i)<<")"<<"<-["<<j.second<<"]-("<<j.first<<")"<<'\n';
              cerr<<"match (SCCNode),(sccDest) ";
              cerr<<"where SCCNode:SCCNode AND SCCNode.SCCID="<<get<0>(i)<<" ";
              cerr<<"AND sccDest:"<<tableID<<" AND sccDest.localID="<<j.first<<" ";
              cerr<<" create (SCCNode)<-[r:Rule{rule: \""<<j.second<<"\"}]-(sccDest);"<<endl;
            }
          }
        }
      }

      // 強連結成分分解済みのサブグラフ内ノードを探索済みにする
        for(const auto& i : traversedNode){
          unsearchedID.erase(i.first);
        }
      }
      auto tmp=nodeCount-contractedNodeNum+nodeNumAfterContraction;
      stringstream resultStr;
      resultStr<<"--------------------------------------------------------"<<endl;
      resultStr<<"  Result: "<<tableID<<endl;
      resultStr<<"--------------------------------------------------------"<<endl;
      resultStr<<" 縮約前の全状態の個数     \t: "    <<nodeCount<<endl;
      resultStr<<" 縮約された状態数         \t: "  <<contractedNodeNum<<endl;
      resultStr<<" 作成された縮約状態の個数  \t: "  <<nodeNumAfterContraction<<endl;
      resultStr<<" 縮約処理後の状態数        \t: "  <<tmp<<endl;
      resultStr<<" 縮約後遷移集合のサイズ比率\t: "  <<(static_cast<double>(tmp)/nodeCount)*100.0<<"%"<<endl;
      results.emplace(tableID,resultStr.str());
    }
// 縮約処理
// (1) 縮約対象を読み込む
// (2) match (n) where ID(n)=[縮約対象]をグループごとにやる

// 検索処理(到達可能性)
// in: start状態(全体)、goal状態(全体) vec1=の形でいい
// 以下ループ
// 1) ゴール状態を1つ読む
// 2) 部分ヘッダで分解、ID抽出
// 3) そのIDは強連結要素か？
  // そうなら、そのノード(dest)へ繋がる、"強連結成分の一意のID"を持たないノード(source)を探して全部格納(ワープ)
  // match (n)-[r:Rule]->(m) where n.cID IS NULL AND m.cID=0 return n,r,m
// match p=shortestPath((n)-[*..100000]-(m)) where n.cID IS NULL AND m.cID=0 AND ID(m)=18323 return ID(n)
  // なければそのままdestと∫するノードのIDを取る
 // 4) goal状態を相手側のヘッダをみつつ展開
//    (3分割以上ならヘッダとの差分)
// 5) 与えられたstartと比較、合ってれば終わり
// 6) そうでなければ、goal状態に格納

// 以上ループ
      auto end = std::chrono::system_clock::now();
      auto diff = end - start;
      for(auto& i:results){
        cout<<i.second;
      }

      cout<<"========================================================"<<endl;
      cout<<" 処理時間                \t: "    <<std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()
              << " msec("
              << std::setfill ('0') << std::setw (2)
              << std::chrono::duration_cast<std::chrono::hours>(diff).count()<<"h "
              << std::setfill ('0') << std::setw (2)
              << std::chrono::duration_cast<std::chrono::minutes>(diff).count()%60<<"m "
              << std::setfill ('0') << std::setw (2)
              << std::chrono::duration_cast<std::chrono::seconds>(diff).count()%60<<"s"
              << ")" << std::endl;
      cout<<"========================================================"<<endl;
  neo4j_end_session(session);
  neo4j_close(connection);
  neo4j_client_cleanup();
  return EXIT_SUCCESS;
}