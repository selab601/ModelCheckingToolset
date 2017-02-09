#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <iterator>
#include <fstream>
#include <functional>
#include <boost/filesystem.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>

using namespace boost::spirit;

template<typename Iterator>
struct ruleGrammar : qi::grammar<Iterator,qi::space_type>{
  ruleGrammar() : ruleGrammar::base_type(rule){
    rule      = precondition>>"->">>postcondition;
    precondition = property >> *("&&">>property | "||">>property);
    postcondition = attribute >> *("&&">>attribute);
    property  = '('>>precondition>>')' | attribute;
    attribute = value>>'='>>value;
    value     = lexeme[(standard::alpha | standard::char_('_'))>>*(standard::alnum | standard::char_('_'))];
  }
  qi::rule<Iterator,qi::space_type> rule;
  qi::rule<Iterator,qi::space_type> precondition;
  qi::rule<Iterator,qi::space_type> postcondition;
  qi::rule<Iterator,qi::space_type> property;
  qi::rule<Iterator,qi::space_type> attribute;
  qi::rule<Iterator,qi::space_type> value;
};

int main(const int argc, char const *argv[]){
  using namespace std;
  try{
    //
    // Create Files
    //
    fstream ruleList(argv[1], std::ios::in | std::ios::out);
    fstream attributesTable(argv[2], std::ios::in | std::ios::out);
    boost::filesystem::path outputPath(argv[3]);
    if(argc!=4){
      throw "Require 3 arguments(Rulelist,AttributesTable,OutPutDir).";
    }
    if(ruleList.fail()){
      throw "Failed to load Rulelist.";
    }
    if(attributesTable.fail()){
      throw "Failed to load AttributesTable.";
    }
    if(!boost::filesystem::exists(outputPath)){
      throw "OutPutPath doesn't exist.";
    }

    ofstream initialStates(outputPath.string()+"/InitialStates",std::ios::trunc);
    ofstream verifier(outputPath.string()+"/verifier.pml",ios::trunc);
    if(initialStates.fail()){
      throw "Failed to create file : InitialStates";
    }
    if(verifier.fail()){
      throw "Failed to create file : verifier.pml";
    }

    //
    // 属性定義を初期化
    //
    typedef map<string,set<string>> AttributeValueTable;
    AttributeValueTable attributeValueTable{};
    for(string str;std::getline(attributesTable,str) && str.size()>0;){
      std::vector<string> values;
      boost::split(values,str,boost::is_any_of(","));
      attributeValueTable.emplace(*values.begin(),set<string>(values.begin()+1,values.end()));
    }
    
    //
    // 制御ルールの最適化
    //
    // 属性,属性値1,属性値2,...を扱うvector
    vector<string> attributeInfo;

    // attributestableの行数をカウント->属性の個数として使用
    auto attrNumber=0;

    // tuple<(置換前ルール文字列),(置換後ルール文字列)>をルールごとに生成
    vector<tuple<string,string>> stringReplacementTable;
    for(auto& attr : attributeValueTable){
      for(auto it=attr.second.begin();it!=attr.second.end();it++){
        stringReplacementTable.emplace_back(attr.first+"="+*it,string("val[")+to_string(attrNumber)+string("]=")+to_string(std::distance(attr.second.begin(), it)));
      }
      attrNumber++;
    }

    //
    // 制御ルール定義の作成
    //
    vector<string> ruleDefinition;

    // ruleListの行数をカウント->ルールIDとして使用
    auto ruleID=0;
    ruleGrammar<std::string::iterator> grammar;
    for(string rule;std::getline(ruleList,rule) && rule.size()>0;ruleID++){
      auto it = rule.begin();
      if(!qi::phrase_parse(it,rule.end(),grammar >> eoi,qi::space))throw "Parse Error.";
      for(auto strTuple : stringReplacementTable){
        boost::algorithm::replace_all(rule,get<0>(strTuple),get<1>(strTuple));
      }
      vector<string> condition;
      boost::split(condition,rule,boost::is_any_of("->"));  // パースされたものの場合、condition.size()が2であることは保証される
      boost::algorithm::replace_all(*condition.begin(),"=","==");
      boost::trim(*condition.begin());
      boost::trim(*condition.rbegin());
      boost::algorithm::replace_all(*condition.rbegin(),"&&","->");
      ruleDefinition.emplace_back("::d_step{("+*condition.begin()+")->"+*condition.rbegin()+"->rule="+to_string(ruleID+1)+";}");
    }

    //
    // 初期状態集合の作成
    //
    int id=0;
    std::function<void(string,AttributeValueTable::iterator)> func
    = [&](string result,AttributeValueTable::iterator iter){
      if(iter==attributeValueTable.end()){
        result.pop_back();
        initialStates<<id++<<","<<result<<std::endl;
        return;
      }
      for(auto it=iter->second.begin();it!=iter->second.end();it++){
        func(result+std::to_string(std::distance(iter->second.begin(), it))+",",std::next(iter));
      }
    };
    func("",attributeValueTable.begin());

    //
    // "verifier.pml"の作成
    //
    verifier<<"#define ATTR_NUM " << attrNumber     <<endl<<endl
      <<"byte val[ATTR_NUM];"                       <<endl
      <<"int rule=0;"                             <<endl<<endl
      <<"inline InitValues(){atomic{c_code{"        <<endl
      <<"\tint c=0;"                                <<endl
      <<"\tfor(c=0;c<ATTR_NUM;c++){"                <<endl
      <<"\tscanf(\",%d\", &(now.val[c]));"          <<endl
      <<"}};}}"                                     <<endl<<endl
      <<"inline Output(){atomic{c_code{"            <<endl
      <<"\tif(now.rule!=0){"                        <<endl
      <<"\t\tint c=0;"                              <<endl
      <<"\t\tprintf(\"@%d\",now.rule-1);"             <<endl
      <<"\t\tfor(c=0;c<ATTR_NUM;c++){"              <<endl
      <<"\t\t\tprintf(\",%d\",now.val[c]);"         <<endl
      <<"}printf(\"\\n\");}}}}"                     <<endl<<endl
      <<"init{atomic{"                              <<endl
      <<"\tInitValues();"                           <<endl
      <<"\trun ApplyRules()"                        <<endl
      <<"}}"                                        <<endl<<endl
      <<"proctype ApplyRules(){atomic{"             <<endl
      <<"\tif"                                      <<endl;
      for(const auto rule : ruleDefinition){
        verifier<<"\t"<<rule                        <<endl;
      }
      verifier<<"\t::else"                          <<endl
      <<"\tfi"                                      <<endl
      <<"\tOutput();"                               <<endl
      <<"}}";
      return 0;

  }catch(const char* str){
    std::cerr<<"ERROR: "<<str<<std::endl;
    return 1;
  }
}