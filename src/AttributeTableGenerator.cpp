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
struct ruleGrammar : qi::grammar<Iterator,std::vector<std::string>(),qi::space_type>{
  ruleGrammar() : ruleGrammar::base_type(rule){
    rule      = precondition>>"->">>postcondition;
    precondition = property >> *("&&">>property | "||">>property);
    postcondition = attribute >> *("&&">>attribute);
    property  = '('>>precondition>>')' | attribute;
    attribute = as_string[raw[value>>'='>>value]][boost::phoenix::push_back(boost::phoenix::ref(_val),_1)];
    value     = qi::as_string[lexeme[(standard::alpha | standard::char_('_'))>>*(standard::alnum | standard::char_('_'))]];
  }
  qi::rule<Iterator,std::vector<std::string>(),qi::space_type> rule;
  qi::rule<Iterator,std::vector<std::string>(),qi::space_type> precondition;
  qi::rule<Iterator,std::vector<std::string>(),qi::space_type> postcondition;
  qi::rule<Iterator,std::vector<std::string>(),qi::space_type> property;
  qi::rule<Iterator,std::vector<std::string>(),qi::space_type> attribute;
  qi::rule<Iterator,std::string(),qi::space_type> value;
};

auto parseRule(const std::string& str){
  std::vector<std::string> result;
  ruleGrammar<std::string::const_iterator> grammar;
  auto it = str.begin();

  // 入力の全てが文法に従っているか？
  if(qi::phrase_parse(it,str.end(),grammar >> eoi,qi::space,result)){
    return result;
  }else throw "Parse Error.";
}


int main(const int argc, char const *argv[]){
  using namespace std;
  try{
    //
    // 制御ルール定義
    //
    fstream ruleList(argv[1], std::ios::in | std::ios::out);

    if(argc!=2){
      throw "Require one argument(Rulelist).";
    }
    if(ruleList.fail()){
      throw "Failed to load Rulelist";
    }

    boost::filesystem::path workingDir(boost::filesystem::current_path());
    boost::filesystem::path target(argv[1]);
    ofstream attributesTable(workingDir.string()+"/"+target.stem().string()+"_attrTable",std::ios::trunc);
    if(attributesTable.fail()){

      throw "Failed to create file : attributesTable";
    }

    //
    // 制御ルールをパース
    //
    vector<string> attributes;
    typedef map<string,set<string>> AttributeValueList;
    AttributeValueList attributeValueList{};
    for(string str;std::getline(ruleList,str) && str.size()>0;){
      auto attrParts = parseRule(str);
      std::copy(attrParts.begin(),attrParts.end(),std::back_inserter(attributes));
    }
    for(auto attribute : attributes){
      std::vector<string> values;
      boost::split(values,attribute,boost::is_any_of("="));
      if(values.size()>2){
        throw "Error at Parser Logic(Parsing '=')";
      }
      attributeValueList[*values.begin()].emplace(*(--values.end()));
    }

    //
    // 属性定義を出力
    //
    for(auto iter=attributeValueList.begin();iter!=attributeValueList.end();iter++){
      auto result=iter->first;
      result+=",";
      for(auto str : iter->second){
        result+=str+",";
      }
      result.pop_back();
      attributesTable<<result;
      if(std::next(iter)!=attributeValueList.end())attributesTable<<std::endl;
    }
    return 0;
  //
  // Error handling
  //
  }catch(const char* str){
    std::cerr<<"ERROR: "<<str<<std::endl;
    return 1;
  }
}