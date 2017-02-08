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

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/fusion/include/adapt_struct.hpp>

#include "../lib/Eigen/Dense"
#include "../lib/Eigen/Sparse"

using namespace boost::spirit;

struct parsedAttributes {
  std::set<std::string> preAttributes;
  std::set<std::string> postAttributes;
};

BOOST_FUSION_ADAPT_STRUCT(
  parsedAttributes,
  (std::set<std::string>,preAttributes)
  (std::set<std::string>,postAttributes)
);

template<typename Iterator>
struct ruleGrammar : qi::grammar<Iterator,parsedAttributes(),qi::space_type>{
  ruleGrammar() : ruleGrammar::base_type(rule){
    rule      = precondition>>"->">>postcondition;
    precondition = property >> *("&&">>property | "||">>property);
    postcondition = attribute >> *("&&">>attribute);
    property  = '('>>precondition>>')' | attribute;
    // attribute = as_string[raw[value>>'='>>value]];
    attribute = value>>'='>>omit[value];
    value     = as_string[lexeme[(standard::alpha | standard::char_('_'))>>*(standard::alnum | standard::char_('_'))]];
  }
  qi::rule<Iterator,parsedAttributes(),qi::space_type> rule;
  qi::rule<Iterator,std::set<std::string>(),qi::space_type> precondition,postcondition,property;
  qi::rule<Iterator,std::string(),qi::space_type> attribute,value;
};

auto parseRule(const std::string& str){
  parsedAttributes result;
  ruleGrammar<std::string::const_iterator> grammar;
  auto it = str.begin();
  // 入力の全てが文法に従っているか？
  if(qi::phrase_parse(it,str.end(),grammar>>eoi,qi::space,result)==true){
    return result;
  }else {throw "Parse Error.";}
}

int main(int argc, char const *argv[]){
  try{
    // argv[1]="./sample/kamenoko";
    // argv[2]="./kamenoko_attrTable";
    // argv[3]="./";
    if(argc!=4){
      throw "Usage: ./RuleSeparater [RuleList] [AttributeList] [OutPutDir]";
    }

    boost::filesystem::path rulePath(argv[1]);
    boost::filesystem::path attrTablePath(argv[2]);
    boost::filesystem::path resultDirPath(argv[3]);

    if(!boost::filesystem::exists(rulePath)){
      throw "そんなRuleListはない";
    }

    if(!boost::filesystem::exists(attrTablePath)){
      throw "そんなAttrTableはない";
    }

    if(!boost::filesystem::exists(resultDirPath)){
      throw "そんなディレクトリはない";
    }

    std::string resultDirFullPath = resultDirPath.string()+rulePath.stem().string()+"_separated/";

    if(boost::filesystem::exists(resultDirFullPath)){
      resultDirFullPath.insert(0,"分割結果がすでにある : ");
      throw resultDirFullPath.c_str();
    }

    boost::filesystem::create_directory(resultDirFullPath);

    std::ifstream ruleList(argv[1]);
    if(ruleList.fail()){
      throw "ルールリスト読込みエラー";
    }

    std::ifstream attrTable(argv[2]);
    if(attrTable.fail()){
      throw "属性テーブル読込みエラー";
    }

    // ルールRnの[x]-conditionの条件式
    // conditions[n].first(=pre)
    std::vector<std::pair<std::set<std::string>,std::set<std::string>>> rules;

    for(std::string str;std::getline(ruleList,str) && str.size()>0;){
      // 事前事後に分ける
      auto result=parseRule(str);
      rules.emplace_back(make_pair(result.preAttributes,result.postAttributes));
    }

    // 「ノード」と、「そのノードから遷移するノードの集合」の組を取得
    // relations[ID].{source:ノード,destinations:次ノード群,commonProperty:共有変数}
    struct relationStructure{
      std::set<int> source;
      std::list<std::set<int>> destinations;
    };

    Eigen::MatrixXd laplacianMatrix=Eigen::MatrixXd::Zero(rules.size(),rules.size());
    std::vector<relationStructure> relations;
    for(auto rule=rules.begin();rule!=rules.end();++rule){
      relationStructure relation;
      relation.source.emplace(std::distance(rules.begin(),rule));
      // rule.post -> destRule.pre==true || destRule.post -> rule.pre==true
      for(auto destRule=rules.begin();destRule!=rules.end();++destRule){
        if(rule==destRule){
          continue;
        }
        // 隣接数A(rule)(destRule)を求める
        if(std::any_of(rule->second.begin(),rule->second.end(),[&](auto postAttribute){
          return destRule->first.count(postAttribute)==1;
        })){
          relation.destinations.emplace_back(std::set<int>({static_cast<int>(std::distance(rules.begin(),destRule))}));
          laplacianMatrix(static_cast<int>(std::distance(rules.begin(),rule)),static_cast<int>(std::distance(rules.begin(),destRule)))-=1.0;
        }
        // 隣接数B(rule)(destRule)を求める
        if(std::any_of(rule->first.begin(),rule->first.end(),[&](auto preAttribute){
          return destRule->second.count(preAttribute)==1;
        })){
          relation.destinations.emplace_back(std::set<int>({static_cast<int>(std::distance(rules.begin(),destRule))}));
          laplacianMatrix(static_cast<int>(std::distance(rules.begin(),rule)),static_cast<int>(std::distance(rules.begin(),destRule)))-=1.0;
        }
      }
      laplacianMatrix(static_cast<int>(std::distance(rules.begin(),rule)),static_cast<int>(std::distance(rules.begin(),rule)))=relation.destinations.size();
      relations.emplace_back(relation);
    }


    Eigen::EigenSolver<Eigen::MatrixXd> es(laplacianMatrix);

    // std::cout << laplacianMatrix << std::endl;
    // std::cout <<std::fixed<< "全ての固有値:" << std::endl << es.eigenvalues()<<std::endl;
    // std::cout <<std::fixed<< "全ての固有ベクトル:" << std::endl << es.eigenvectors()<<std::endl;

    // 固有値をベクトル化
    Eigen::VectorXd m_solved_val = es.eigenvalues().real().cast<double>();

    // さらに探索用のstd::vectorに変換
    std::vector<double> sortee(m_solved_val.data(), m_solved_val.data() + m_solved_val.rows() * m_solved_val.cols());

    // 一番大きい要素を削除
    sortee.erase(std::remove(sortee.begin(), sortee.end(), *std::min_element(sortee.begin(),sortee.end())), sortee.end()); 

    // 2番目に大きい要素
    // std::cout<<"minPos: "<<std::distance(
    //   m_solved_val.data(),
    //   std::find(m_solved_val.data(), m_solved_val.data() + m_solved_val.rows() * m_solved_val.cols(),*std::min_element(sortee.begin(),sortee.end()))
    //   );
    // std::cout<<std::endl;
    // std::cout << "欲しい固有ベクトル:" << std::endl << es.eigenvectors().col(std::distance(
      // m_solved_val.data(),
      // std::find(m_solved_val.data(), m_solved_val.data() + m_solved_val.rows() * m_solved_val.cols(),*std::min_element(sortee.begin(),sortee.end()))
      // ))<<std::endl;

    Eigen::VectorXd resultEigen=es.eigenvectors().col(std::distance(
      m_solved_val.data(),
      std::find(m_solved_val.data(), m_solved_val.data() + m_solved_val.rows() * m_solved_val.cols(),*std::min_element(sortee.begin(),sortee.end()))
      )).real().cast<double>();
    std::vector<int> ruleCuts;

    int count=0;
    for(auto iter=resultEigen.data();iter!=(resultEigen.data()+resultEigen.rows());++iter,++count){
      if(*iter>0.0){
        ruleCuts.emplace_back(count);
      }
    }

    ruleList.clear();
    ruleList.seekg(0,std::ios::beg);

    // 
    // generate DecodeTable
    // 

    // decodeTable_*
    // カンマ区切りの、
    // 1行目:m番目のルール番号n
    // ruleCuts_*のm番目のルールを、全体グラフのルール番号nに置き換えるためのテーブル
    // 2行目:m番目の属性n
    // →AttrTable_*のm番目の属性を、全体に直した時のn番目の属性になおす
    std::ofstream decodeTable_A(resultDirFullPath+"decodeTable_A",std::ios::trunc);
    std::ofstream decodeTable_B(resultDirFullPath+"decodeTable_B",std::ios::trunc);
    std::string decodeStrA,decodeStrB;
    std::string readingLineBuffer;

    // 
    // Generate separated rules & write first line of decodeTable (Mapping Rules)
    // 

    std::fstream separateResult_A(resultDirFullPath+"ruleCuts_A",std::ios::in | std::ios::out | std::ios::trunc);
    std::fstream separateResult_B(resultDirFullPath+"ruleCuts_B",std::ios::in | std::ios::out | std::ios::trunc);
    for(int i=0;std::getline(ruleList, readingLineBuffer);i++){
      if(std::find(ruleCuts.begin(), ruleCuts.end(),i)!=ruleCuts.end()){
        separateResult_A<<readingLineBuffer<<std::endl;
        decodeStrA+=std::to_string(i)+",";
      }else{
        separateResult_B<<readingLineBuffer<<std::endl;
        decodeStrB+=std::to_string(i)+",";
      }
    }
    decodeStrA.pop_back();
    decodeStrB.pop_back();
    decodeTable_A<<decodeStrA;
    decodeTable_B<<decodeStrB;

    // 
    // Write second line of decodeTable (Mapping Attrs)
    // 
    std::set<std::string> preProperties_A={};
    std::set<std::string> postProperties_A={};
    std::set<std::string> preProperties_B={};
    std::set<std::string> postProperties_B={};
    auto extractAttributeSet = [&](std::string buffer,std::set<std::string>* preProperties,std::set<std::string>* postProperties){
      auto result=parseRule(buffer);
      //*preProperties+=result.preAttributes;
      //*postProperties+=result.postAttributes;
      preProperties->insert(result.preAttributes.begin(), result.preAttributes.end());
      postProperties->insert(result.postAttributes.begin(), result.postAttributes.end());
    };
    separateResult_A.seekg(0,std::ios::beg);
    separateResult_B.seekg(0,std::ios::beg);
    while(std::getline(separateResult_A, readingLineBuffer)){
      extractAttributeSet(readingLineBuffer,&preProperties_A,&postProperties_A);
    }
    while(std::getline(separateResult_B, readingLineBuffer)){
      extractAttributeSet(readingLineBuffer,&preProperties_B,&postProperties_B);
    }

    std::ofstream attributesTable_A(resultDirFullPath+"ruleCuts_A_attrTable",std::ios::trunc);
    std::ofstream attributesTable_B(resultDirFullPath+"ruleCuts_B_attrTable",std::ios::trunc);
    std::set<std::string> propertiesOfA=preProperties_A;
    std::set<std::string> propertiesOfB=preProperties_B;
    propertiesOfA.insert(begin(postProperties_A),end(postProperties_A));
    propertiesOfB.insert(begin(postProperties_B),end(postProperties_B));

    decodeStrA.clear();
    decodeStrB.clear();

    // よみこみ、
    for(int i=0;std::getline(attrTable,readingLineBuffer);i++){
      std::string attr=readingLineBuffer.substr(0,readingLineBuffer.find_first_of(","));
      // 読み込んだ属性がA側に出てくるなら、A側にコピー
      if(propertiesOfA.count(attr)>0){
        attributesTable_A<<readingLineBuffer<<std::endl;
        decodeStrA+=std::to_string(i)+",";
      }
      if(propertiesOfB.count(attr)>0){
        attributesTable_B<<readingLineBuffer<<std::endl;
        decodeStrB+=std::to_string(i)+",";
      }
    }
    decodeStrA.pop_back();
    decodeStrB.pop_back();
    decodeTable_A<<'\n'<<decodeStrA;
    decodeTable_B<<'\n'<<decodeStrB;

    std::vector<std::string> commonProperties;
    std::vector<std::string> commonPropertiesAtoB;
    std::vector<std::string> commonPropertiesBtoA;

    std::set_intersection(preProperties_A.begin(), preProperties_A.end()
      , postProperties_B.begin(), postProperties_B.end()
      , std::back_inserter(commonPropertiesAtoB));

    std::set_intersection(preProperties_B.begin(), preProperties_B.end()
      , postProperties_A.begin(), postProperties_A.end()
      , std::back_inserter(commonPropertiesBtoA));

    std::set_intersection(commonPropertiesAtoB.begin(), commonPropertiesAtoB.end()
      , commonPropertiesBtoA.begin(), commonPropertiesBtoA.end()
      , std::back_inserter(commonProperties));

    std::string tmp="";
    for(auto i : commonProperties){
      tmp+=i+",";
    }
    if(tmp.size()>0){
      tmp.pop_back();
      std::ofstream commonPropertiesFile(resultDirFullPath+"commonProperties",std::ios::trunc);
      commonPropertiesFile<<tmp;
    }

    std::cout<<"Done."<<std::endl;
    return 0;
  }catch(const char* str){
    std::cout<<str<<std::endl;
    return -1;
  }
}