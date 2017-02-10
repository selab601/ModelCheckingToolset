#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <list>
#include <unordered_set>
#include <string>
#include <iterator>
#include <algorithm>
#include <functional>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/fusion/include/adapt_struct.hpp>

#include "../lib/Eigen/Dense"
#include "../lib/Eigen/Sparse"

using namespace boost::spirit;

struct parsedAttributes {
  std::unordered_set<std::string> preAttributes;
  std::unordered_set<std::string> postAttributes;
};

BOOST_FUSION_ADAPT_STRUCT(
  parsedAttributes,
  (std::unordered_set<std::string>,preAttributes)
  (std::unordered_set<std::string>,postAttributes)
);

template<typename Iterator>
struct ruleGrammar : qi::grammar<Iterator,parsedAttributes(),qi::space_type>{
  ruleGrammar() : ruleGrammar::base_type(rule){
    rule      = precondition>>"->">>postcondition;
    precondition = property >> *("&&">>property | "||">>property);
    postcondition = attribute >> *("&&">>attribute);
    property  = '('>>precondition>>')' | attribute;
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
  if(qi::phrase_parse(it,str.end(),grammar>>eoi,qi::space,result)==true){
    return result;
  }else {throw "Parse Error.";}
}

int main(int argc, char const *argv[]){
  using namespace std;
  try{
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

    string resultDirFullPath = resultDirPath.string()+rulePath.stem().string()+"_separated/";

    if(boost::filesystem::exists(resultDirFullPath)){
      resultDirFullPath.insert(0,"分割結果がすでにある : ");
      throw resultDirFullPath.c_str();
    }

    boost::filesystem::create_directory(resultDirFullPath);

    ifstream ruleList(argv[1]);
    if(ruleList.fail()){
      throw "ルールリスト読込みエラー";
    }

    ifstream attrTable(argv[2]);
    if(attrTable.fail()){
      throw "属性テーブル読込みエラー";
    }

    // ルールRnの[x]-conditionの条件式
    // conditions[n].first(="pre-condition")
    vector<pair<unordered_set<string>,unordered_set<string>>> rules;

    for(string str;getline(ruleList,str) && str.size()>0;){
      // 事前事後に分ける
      auto result=parseRule(str);
      rules.emplace_back(make_pair(result.preAttributes,result.postAttributes));
    }

    // 「ノード」と、「そのノードから遷移するノードの集合」の組を取得
    // relations[ID].{source:ノード,destinations:次ノード群,commonProperty:共有変数}
    struct relationStructure{
      set<int> source;
      list<set<int>> destinations;
    };

    Eigen::MatrixXd laplacianMatrix=Eigen::MatrixXd::Zero(rules.size(),rules.size());
    vector<relationStructure> relations;
    for(auto rule=rules.begin();rule!=rules.end();++rule){
      relationStructure relation;
      relation.source.emplace(distance(rules.begin(),rule));
      // rule.post -> destRule.pre==true || destRule.post -> rule.pre==true
      for(auto destRule=rules.begin();destRule!=rules.end();++destRule){
        if(rule==destRule){
          continue;
        }
        // 隣接数A(rule)(destRule)を求める
        if(any_of(rule->second.begin(),rule->second.end(),[&](auto postAttribute){
          return destRule->first.count(postAttribute)==1;
        })){
          relation.destinations.emplace_back(set<int>({static_cast<int>(distance(rules.begin(),destRule))}));
          laplacianMatrix(static_cast<int>(distance(rules.begin(),rule)),static_cast<int>(distance(rules.begin(),destRule)))-=1.0;
        }
        // 隣接数B(rule)(destRule)を求める
        if(any_of(rule->first.begin(),rule->first.end(),[&](auto preAttribute){
          return destRule->second.count(preAttribute)==1;
        })){
          relation.destinations.emplace_back(set<int>({static_cast<int>(distance(rules.begin(),destRule))}));
          laplacianMatrix(static_cast<int>(distance(rules.begin(),rule)),static_cast<int>(distance(rules.begin(),destRule)))-=1.0;
        }
      }
      laplacianMatrix(static_cast<int>(distance(rules.begin(),rule)),static_cast<int>(distance(rules.begin(),rule)))=relation.destinations.size();
      relations.emplace_back(relation);
    }


    Eigen::EigenSolver<Eigen::MatrixXd> es(laplacianMatrix);

    // 固有値をベクトル化
    Eigen::VectorXd m_solved_val = es.eigenvalues().real().cast<double>();

    // さらに探索用のvectorに変換
    vector<double> sortee(m_solved_val.data(), m_solved_val.data() + m_solved_val.rows() * m_solved_val.cols());

    // 一番大きい要素を削除
    sortee.erase(remove(sortee.begin(), sortee.end(), *min_element(sortee.begin(),sortee.end())), sortee.end());

    Eigen::VectorXd resultEigen=es.eigenvectors().col(distance(
      m_solved_val.data(),
      find(m_solved_val.data(), m_solved_val.data() + m_solved_val.rows() * m_solved_val.cols(),*min_element(sortee.begin(),sortee.end()))
      )).real().cast<double>();
    vector<int> ruleCuts;

    int count=0;
    for(auto iter=resultEigen.data();iter!=(resultEigen.data()+resultEigen.rows());++iter,++count){
      if(*iter>0.0){
        ruleCuts.emplace_back(count);
      }
    }

    ruleList.clear();
    ruleList.seekg(0,ios::beg);


    //
    // 部分モデルの作成(制御ルール定義と、符号化テーブルの1行目)
    //

    auto extractAttributeSet = [&](string buffer,unordered_set<string>* preProperties,unordered_set<string>* postProperties){
      auto result=parseRule(buffer);
      preProperties->insert(result.preAttributes.begin(), result.preAttributes.end());
      postProperties->insert(result.postAttributes.begin(), result.postAttributes.end());
    };

    // decodeTable_*
    // カンマ区切りの、
    // 1行目:m番目のルール番号n
    // ruleCuts_*のm番目のルールを、全体グラフのルール番号nに置き換えるためのテーブル
    // 2行目:m番目の属性n
    // →AttrTable_*のm番目の属性を、全体に直した時のn番目の属性になおす
    ofstream decodeTable_A(resultDirFullPath+"decodeTable_A",ios::trunc);
    ofstream decodeTable_B(resultDirFullPath+"decodeTable_B",ios::trunc);
    fstream separateResult_A(resultDirFullPath+"ruleCuts_A",ios::in | ios::out | ios::trunc);
    fstream separateResult_B(resultDirFullPath+"ruleCuts_B",ios::in | ios::out | ios::trunc);
    string decodeStrA,decodeStrB;
    string readingLineBuffer;
    unordered_set<string> preProperties_A={};
    unordered_set<string> postProperties_A={};
    unordered_set<string> preProperties_B={};
    unordered_set<string> postProperties_B={};

    for(int i=0;getline(ruleList, readingLineBuffer);i++){
      if(find(ruleCuts.begin(), ruleCuts.end(),i)!=ruleCuts.end()){
        separateResult_A<<readingLineBuffer<<endl;
        decodeStrA+=to_string(i)+",";
      }else{
        separateResult_B<<readingLineBuffer<<endl;
        decodeStrB+=to_string(i)+",";
      }
    }

    decodeStrA.pop_back();
    decodeStrB.pop_back();
    decodeTable_A<<decodeStrA;
    decodeTable_B<<decodeStrB;

    //
    // 部分モデルの作成(属性定義と、符号化テーブルの2行目)
    //

    separateResult_A.seekg(0,ios::beg);
    separateResult_B.seekg(0,ios::beg);

    while(getline(separateResult_A, readingLineBuffer)){
      extractAttributeSet(readingLineBuffer,&preProperties_A,&postProperties_A);
    }
    while(getline(separateResult_B, readingLineBuffer)){
      extractAttributeSet(readingLineBuffer,&preProperties_B,&postProperties_B);
    }

    ofstream attributesTable_A(resultDirFullPath+"ruleCuts_A_attrTable",ios::trunc);
    ofstream attributesTable_B(resultDirFullPath+"ruleCuts_B_attrTable",ios::trunc);
    unordered_set<string> propertiesOfA=preProperties_A;
    unordered_set<string> propertiesOfB=preProperties_B;
    propertiesOfA.insert(begin(postProperties_A),end(postProperties_A));
    propertiesOfB.insert(begin(postProperties_B),end(postProperties_B));

    decodeStrA.clear();
    decodeStrB.clear();

    for(int i=0;getline(attrTable,readingLineBuffer);i++){
      string attr=readingLineBuffer.substr(0,readingLineBuffer.find_first_of(","));
      // 読み込んだ属性がA側に出てくるなら、A側にコピー
      if(propertiesOfA.count(attr)>0){
        attributesTable_A<<readingLineBuffer<<endl;
        decodeStrA+=to_string(i)+",";
      }
      if(propertiesOfB.count(attr)>0){
        attributesTable_B<<readingLineBuffer<<endl;
        decodeStrB+=to_string(i)+",";
      }
    }
    decodeStrA.pop_back();
    decodeStrB.pop_back();
    decodeTable_A<<'\n'<<decodeStrA;
    decodeTable_B<<'\n'<<decodeStrB;

    //
    // 共通属性定義の作成
    //
    vector<string> commonProperties;
    vector<string> commonPropertiesAtoB;
    vector<string> commonPropertiesBtoA;

    set_intersection(preProperties_A.begin(), preProperties_A.end()
      , postProperties_B.begin(), postProperties_B.end()
      , back_inserter(commonPropertiesAtoB));

    set_intersection(preProperties_B.begin(), preProperties_B.end()
      , postProperties_A.begin(), postProperties_A.end()
      , back_inserter(commonPropertiesBtoA));

    set_intersection(commonPropertiesAtoB.begin(), commonPropertiesAtoB.end()
      , commonPropertiesBtoA.begin(), commonPropertiesBtoA.end()
      , back_inserter(commonProperties));

    string tmp="";
    for(auto i : commonProperties){
      tmp+=i+",";
    }
    if(tmp.size()>0){
      tmp.pop_back();
      ofstream commonPropertiesFile(resultDirFullPath+"commonProperties",ios::trunc);
      commonPropertiesFile<<tmp;
    }

    cout<<"Done."<<endl;
    return 0;
  }catch(const char* str){
    cout<<str<<endl;
    return -1;
  }
}