#include <iostream>
#include <fstream>
#include <errno.h>
#include <stdio.h>
#include <vector>
#include <list>
#include <tuple>
#include <string>
#include <map>
#include <chrono>
#include <iomanip>
#include <functional>
#include <iterator>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <boost/algorithm/string.hpp>

int main(int argc, char const *argv[])
{
  /* code */

  using namespace std;
  using boost::algorithm::join;
  const int topFloor=2;
  string lineRangeA="AB";
  string lineRangeB="BC";

  auto goingRight=[&](auto& dest)->string{
    // vector<string> str;
    auto line=lineRangeB.substr(lineRangeB.find_first_of(dest));
    list<string> result;

    for(auto iter=begin(line)+1;iter!=end(line);iter++){
      // cout<<string(1,*iter); 
      result.push_back("CR2_X="+string(1,*iter));
    }
    if(result.size()>1){
      result.begin()->insert(0,"(");
      result.rbegin()->insert(result.rbegin()->size(),")");
    }
    return join(result," || ");
  };

  auto goingLeft=[&](auto& dest)->string{
    // vector<string> str;
    auto line=lineRangeA.substr(0,lineRangeA.find_first_of(dest));
    list<string> result;

    for(auto iter=begin(line);iter!=end(line);iter++){
      // cout<<string(1,*iter); 
      result.push_back("CR1_X="+string(1,*iter));
    }
    if(result.size()>1){
      result.begin()->insert(0,"(");
      result.rbegin()->insert(result.rbegin()->size(),")");
    }
    return join(result," || ");
  };
  cout<<"A1=None ->  A1=Loading"<<endl;

  for(auto& craneName:vector<pair<string,string>>{{"CR1",lineRangeA},{"CR2",lineRangeB}}){
    // 搬入出、上下
    for(int floor=1;floor<topFloor+1;floor++){
      for(auto line=craneName.second.begin();line!=craneName.second.end();line++){
        cout<<craneName.first<<"_LOADING"<<"=True && "<<*line<<floor<<"=None && "<<craneName.first<<"_X"<<"="<<*line<<" && "<<craneName.first<<"_Y"<<"="<<"_"<<floor<<" -> "<<craneName.first<<"_LOADING=False && "<<*line<<floor<<"=Loading"<<endl;
        cout<<craneName.first<<"_LOADING"<<"=False && "<<*line<<floor<<"=Loading && "<<craneName.first<<"_X"<<"="<<*line<<" && "<<craneName.first<<"_Y"<<"="<<"_"<<floor<<" -> "<<craneName.first<<"_LOADING=True && "<<*line<<floor<<"=None"<<endl;
      }
      if(floor!=topFloor)cout<<craneName.first<<"_Y"<<"=_"<<floor<<" -> "<<craneName.first<<"_Y=_"<<floor+1<<endl;
      if(floor!=1)cout<<craneName.first<<"_Y"<<"=_"<<floor<<" -> "<<craneName.first<<"_Y=_"<<floor-1<<endl;
    }
    // 左右
    for(auto line=craneName.second.begin();line!=craneName.second.end();line++){
      if(craneName.first=="CR1"){
        // 左
        if(line!=craneName.second.begin())cout<<craneName.first<<"_X="<<*line<<" -> "<<craneName.first<<"_X="<<*prev(line)<<endl;
        // 右
        if(line!=craneName.second.end()-1){
          cout<<craneName.first<<"_X="<<*line<<" && "<<goingRight(*next(line))<<" -> "<<craneName.first<<"_X="<<*next(line)<<endl;
        }
      }

      if(craneName.first=="CR2"){
        // 右
        if(line!=craneName.second.end()-1)cout<<craneName.first<<"_X="<<*line<<" -> "<<craneName.first<<"_X="<<*next(line)<<endl;
        // 左
        if(line!=craneName.second.begin()){
          cout<<craneName.first<<"_X="<<*line<<" && "<<goingLeft(*prev(line))<<" -> "<<craneName.first<<"_X="<<*prev(line)<<endl;
        }
      }
    }
    // CR1右、CR2左
  }
  return 0;
}