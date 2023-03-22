#include<map>
#include<vector>
#include<exception>
#include<typeinfo>
#include<variant>
#include<string>
#include <type_traits>
#include<fstream>
#include <sstream>
#include<iostream>
namespace tiny_json_parser{
    struct Token;
    //JSON数据类型包括 null,number,string,bool,array,object
    using Int=int;
    using Bool =bool;
    using Null=std::monostate; // monostate c++17 新特性 
    using Float = double;
    using String = std::string;
    using Array = std::vector<Token>;
    using Object = std::map<std::string, Token>;
    using Value = std::variant<Int,Null,Float,String,Array,Object>;
    //JSON语义节点
    struct Token 
    {
        Value val; //保存token的值 可能是六种类型的任意一种
        //获取 Object 中key对应的结果 重载[]
        Token():val(Null{}) {}
        Token(Value _v):val(_v){}
        auto& operator=(const Token &t)
        {
            this->val=t.val;
            return t;
        }
        auto& operator[](const std::string &key)
        {
            //get_if 判断读取到的是否是Object 
            //成功返回一个储存variant值的指针 
            //失败返回空指针
            if(auto v=std::get_if<Object>(&val))
            {
                return (*v)[key];
            }
            throw std::runtime_error("Not an Object!");
        }
        //获取 Array 中下标为key的结果
        auto& operator[](const size_t key)
        {
            //get_if 判断读取到的是否是Object 
            //成功返回一个储存variant值的指针 
            //失败返回空指针
            if(auto v=std::get_if<Array>(&val))
            {
                //at边界安全 如果越界 返回out_of_range
                return v->at(key);
            }
            throw std::runtime_error("Not an Array!");
        }
        //push 插入一个 Token 进入 Array
        void push(const Token &t)
        {
            if(auto v=std::get_if<Array>(&val))
            {
                v->emplace_back(t);
            }
        }
        //push Token插入Object[key] 需要获取当前Object[key]的类型
        //is_same_v
        // void push(const Token &t,const std::string&key)
        // {
        //     if(auto v=std::get_if<Object>(&val))
        //     {
        //         //1.val[key] 不存在
        //         if(!v->count(key))
        //         {
        //             (*v)[key]=t;
        //             //v->emplace({key,t});
        //         }
        //         //2.val[key]存在
        //         else
        //         {
        //             //2.1 是Array
        //             if(std::is_same_v<Array,decltype((*v)[key])>)
        //             {
        //                 (*v)[key].emplace_back(t);
        //             }
        //             //2.2 是Object
        //             if(std::is_same_v<Object,decltype((*v)[key])>)
        //             {
        //                 push(t,(*v)[key]);
        //             }
        //             //2.3 Other
        //             else
        //             {
        //                 (*v)[key]=t;
        //             }
        //         }
        //     }
        // }
    };
    //将json解析为token
    struct Parser
    {
        std::string_view jsonStr;
        size_t currPos=0;

        //跳过空格
        auto parseWhiteSpace(){
            while(currPos<jsonStr.size()&&std::isspace(jsonStr[currPos]))
                currPos++;
        }
        
        auto parseNull()->std::optional<Value>{
            if(jsonStr.substr(currPos,4)=="null")
            {
                currPos+=4;
                return Null{};
            }
            return {};
        }
        auto parseTrue()->std::optional<Value>{
            if(jsonStr.substr(currPos,4)=="true")
            {
                currPos+=4;
                return true;
            }
            return {};
        }
        auto parseFalse()->std::optional<Value>{
            if(jsonStr.substr(currPos,5)=="false")
            {
                currPos+=5;
                return false;
            }
            return {};
        }
        auto parseString()->std::optional<Value>{
            currPos+=1;// "
            auto endPos=currPos;
            while(currPos<jsonStr.size()&&jsonStr[endPos]!='"')
            {
                endPos++;
            }
            std::string str=std::string{jsonStr.substr(currPos,endPos-currPos)};
            currPos=endPos+1;
            return str;
        }
        auto parseObject()->std::optional<Value>{
            Object obj; //返回的Object对象
            currPos+=1; // {
            while(currPos<jsonStr.size()&&jsonStr[currPos]!='}')
            {
                //获取key
                auto key=parseValue();
                parseWhiteSpace();
                //key非字符串 错误
                if(!std::holds_alternative<String>(key.value()))
                    return {};
                //获取value 可能是任意类型 用parseValue递归获取
                if(currPos<jsonStr.size()&&jsonStr[currPos]==':')currPos++;
                parseWhiteSpace();
                auto v=parseValue();
                obj[std::get<String>(key.value())]=v.value();
                parseWhiteSpace();
                //获取下一对
                if(currPos<jsonStr.size()&&jsonStr[currPos]==',')currPos++;
                parseWhiteSpace();
            }
            currPos++;
            return obj;
        }
        auto parseArray()->std::optional<Value>{
            currPos++;// [
            Array arr;//返回的Array对象
            while(currPos<jsonStr.size()&&jsonStr[currPos]!=']')
            {
                parseWhiteSpace();
                auto v=parseValue();
                arr.push_back(v.value());
                parseWhiteSpace();
                if(currPos<jsonStr.size()&&jsonStr[currPos]==',')currPos++;
            }
            currPos++;// ]
            return arr;
        }
        //小数 科学计数
        auto parseNumber()->std::optional<Value>{
            size_t endpos = currPos;
            while (endpos < jsonStr.size() && (
                std::isdigit(jsonStr[endpos]) ||
                jsonStr[endpos] == 'e' ||
                jsonStr[endpos] == '.')) {
                endpos++;
            }
            std::string number = std::string{ jsonStr.substr(currPos, endpos - currPos) };
            currPos = endpos;
            static auto is_Float = [](std::string& number) {
                return number.find('.') != number.npos ||
                    number.find('e') != number.npos;
            };
            if (is_Float(number)) {
                //本身可能是一个不合法的数字 stod不会去校验
                if(std::count(number.begin(),number.end(),'.')>1
                    ||std::count(number.begin(),number.end(),'e')>1)
                    {
                        throw std::runtime_error("not a float:"+number);
                        return {};
                    }
                try {
                    //stod会尝试把字符串解析为double 遇到第一个不合法的字符结束
                    Float ret = std::stod(number);
                    return ret;
                }
                catch (...) {
                    return {};
                }
            }
            else {
                try {
                    Int ret = std::stoi(number);
                    return ret;
                }
                catch (...) {
                    return {};
                }
            }
        }

        auto parseValue() ->std::optional<Value>
        {
            parseWhiteSpace();//跳过空格
            switch(jsonStr[currPos])
            {
                case 'n': return parseNull();
                case 't': return parseTrue();
                case 'f': return parseFalse();
                case '[': return parseArray();
                case '{': return parseObject();
                case '"': return parseString();
                default: return parseNumber();
            }
        }

        auto parse() ->std::optional<Token>
        {
            parseWhiteSpace();
            auto v=parseValue();
            if(!v)return {};
            return Token(*v);
        }
    };
    auto parser(std::string_view s)->std::optional<Token>
    {
        Parser p{s};
        std::cout<<"Parser start"<<std::endl;
        return p.parse();
    }
    class JsonGenerator
    {
        public:
            static auto generator(const Token &t)->std::string
            {
                return std::visit([](auto&& arg) -> std::string {
                    //类型萃取 constexpr编译期确定类型 无需等到运行时
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, Null>)
                    {
                        return "null";
                    }
                    else if constexpr (std::is_same_v<T,Bool>)
                        return arg ? "true":"false";
                    else if constexpr (std::is_same_v<T,Int>)
                        return std::to_string(arg);
                    else if constexpr (std::is_same_v<T,Float>)
                        return std::to_string(arg);
                    else if constexpr (std::is_same_v<T, String>)
                        return generatorString(arg);
                    else if constexpr (std::is_same_v<T,Array>)
                        return generatorArray(arg);
                    else if constexpr (std::is_same_v<T,Object>)
                        return generatorObj(arg);
                },t.val);
            }
            //  auto f(const Null &n)->std::string{return "null";};
            //  auto f(const Bool &b)->std::string{return b==true?"true":"false";};
            //  auto f(const Int &n)->std::string{return std::to_string(n);};
            //  auto f(const Float &n)->std::string{return std::to_string(n);};
            //  auto f(const String &n)->std::string{return generatorString(n);};
            //  auto f(const Array &n)->std::string{return generatorArray(n);};
            //  auto f(const Object &n)->std::string{return generatorObj(n);};
            
            static auto generatorString(const String &n)->std::string
            {
                std::string s="\"";
                s+=n;
                s+="\"";
                return s;
            }
            static auto generatorArray(const Array &n)->std::string
            {
                std::string s="[";
                for(const auto &v:n)
                {
                    s+=generator(v);
                }
                s+="]";
                return s;
            }
            static auto generatorObj(Object n)->std::string
            {
                std::string s="{";
                for(const auto &[key,token]:n)
                {
                    s+=generatorString(key);
                    s+=":";
                    s+=generator(token);
                    s+=",";
                }
                if(!n.empty())s.pop_back();//多了一个，
                s+="}";
                return s;
            }
    };
    inline auto generate(const Token& t) -> std::string { return JsonGenerator::generator(t); }


    auto  operator << (std::ostream& out, const Token& t) ->std::ostream& {
        out << JsonGenerator::generator(t);
        return out;
    }
    
}
using namespace tiny_json_parser;
int main() {
std::cout<<1<<std::endl;
std::ifstream fin("json.txt");

std::stringstream ss; ss << fin.rdbuf();
std::string s{ ss.str() };
std::cout<<2<<std::endl;
auto x = parser(s).value();
std::cout << x << "\n";

// x["configurations"].push({true});
// //x["configurations"].push({Null {}});
Object o;
o["你好"]={123};
x["你好"].push({o});
std::cout<<x<<"\n";
// x["configurations"].push({o});
// //x["configurations"].push({123,"abc"});
// x["version"] = { 114514LL };
// std::cout << x << "\n";
}