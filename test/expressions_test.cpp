#include <iostream>
#include <string>

#include "gtest/gtest.h"

#include "jinja2cpp/template.h"
#include "test_tools.h"

using namespace jinja2;

using ExpressionsMultiStrTest = BasicTemplateRenderer;

MULTISTR_TEST(ExpressionsMultiStrTest, BinaryMathOperations,
R"(
{{ 1 + 10 }}
{{ 1 - 10}}
{{ 0.1 + 1 }}
{{ 0.1 - 10.5 }}
{{ 1 * 10 }}
{{ 7 / 3}}
{{ 7 // 3 }}
{{ 7 % intValue }}
{{ 3 ** 4 }}
{{ 10 ** -2 }}
{{ 10/10 + 2*5 }}
{{ ([1, 2] + [3, 4]) | pprint }}
{{ 'Hello' + " " + 'World ' + stringValue }}
{{ 'Hello' + " " + 'World ' + wstringValue }}
{{ stringValue + ' ' + wstringValue }}
{{ wstringValue + ' ' + stringValue }}
{{ wstringValue + ' ' + wstringValue }}
{{ stringValue + ' ' + stringValue }}
{{ 'Hello' ~ " " ~ 123 ~ ' ' ~ 1.234 ~ " " ~ true ~ " " ~ intValue ~ " " ~ false ~ ' ' ~ 'World ' ~ stringValue  ~ ' ' ~ wstringValue}}
{{ 'abc' * 0 }}
{{ 'abc' * 1 }}
{{ '123' * intValue }}
{{ ([1, 2, 3] * intValue) | pprint }}
{{ stringValue * intValue }}
{{ wstringValue * intValue }}
)",
//-----------
R"(
11
-9
1.1
-10.4
10
2.3333333
2
1
81
0.01
11
[1, 2, 3, 4]
Hello World rain
Hello World rain
rain rain
rain rain
rain rain
rain rain
Hello 123 1.234 true 3 false World rain rain

abc
123123123
[1, 2, 3, 1, 2, 3, 1, 2, 3]
rainrainrain
rainrainrain
)")
{
    params = {
        {"intValue", 3},
        {"doubleValue", 12.123f},
        {"stringValue", "rain"},
        {"wstringValue", std::wstring(L"rain")},
        {"boolFalseValue", false},
        {"boolTrueValue", true},
    };
}

MULTISTR_TEST(ExpressionsMultiStrTest, IfExpression,
R"(
{{ intValue if intValue is eq(3) }}
{{ stringValue if intValue < 3 else doubleValue }}
{{ wstringValue if intValue == 3 else doubleValue }}
)",
//-----------
R"(
3
12.123
rain
)")
{
    params = {
        {"intValue", 3},
        {"doubleValue", 12.123f},
        {"stringValue", "rain"},
        {"wstringValue", std::wstring(L"rain")},
        {"boolFalseValue", false},
        {"boolTrueValue", true},
    };
}

TEST(ExpressionTest, DoStatement)
{
    std::string source = R"(
{{ data.strValue }}{% do setData('Inner Value') %}
{{ data.strValue }}
)";

    TemplateEnv env;
    env.GetSettings().extensions.Do = true;

    TestInnerStruct innerStruct;
    innerStruct.strValue = "Outer Value";

    ValuesMap params = {
        {"data", Reflect(&innerStruct)},
        {"setData", MakeCallable(
            [&innerStruct](const std::string& val) -> Value {
                innerStruct.strValue = val;
                return "String not to be shown";
            },
            ArgInfo{"val"})
            },
    };

    Template tpl(&env);

    ASSERT_TRUE(tpl.Load(source));
    std::string result = tpl.RenderAsString(params).value();
    std::cout << result << std::endl;
    std::string expectedResult = R"(
Outer Value
Inner Value
)";

    EXPECT_STREQ(expectedResult.c_str(), result.c_str());
}

TEST(ExpressionsTest, PipeOperatorPrecedenceTest)
{
    const std::string source = R"(>> {{ 2 < '6' | int }} <<
  >> {{ -30 | abs < str | int }} <<)";

    Template tpl;
    ASSERT_TRUE(tpl.Load(source));
   
    const ValuesMap params = {{"str", "20"}};

    const auto result = tpl.RenderAsString(params).value();
    std::cout << result << std::endl;
    const std::string expectedResult = R"(>> true <<
  >> false <<)";

    EXPECT_STREQ(expectedResult.c_str(), result.c_str());
}

struct LogicalExprTestTag;
using LogicalExprTest = InputOutputPairTest<LogicalExprTestTag>;

TEST_P(LogicalExprTest, Test)
{
    auto& testParam = GetParam();
    std::string source = "{{ 'true' if " + testParam.tpl + " else 'false' }}";

    Template tpl;
    auto parseRes = tpl.Load(source);
    EXPECT_TRUE(parseRes.has_value());
    if (!parseRes)
    {
        std::cout << parseRes.error() << std::endl;
        return;
    }

    std::string result = tpl.RenderAsString(PrepareTestData()).value();
    std::cout << result << std::endl;
    std::string expectedResult = testParam.result;
    EXPECT_EQ(expectedResult, result);
}

SUBSTITUION_TEST_P(ExpressionSubstitutionTest)

INSTANTIATE_TEST_CASE_P(ConstantSubstitutionTest, ExpressionSubstitutionTest, ::testing::Values(
                            InputOutputPair{"'str1'",            "str1"},
                            InputOutputPair{"\"str1\"",          "str1"},
                            InputOutputPair{"100500",            "100500"},
                            InputOutputPair{"'100.555'",         "100.555"},
                            InputOutputPair{"true",              "true"},
                            InputOutputPair{"false",             "false"}
                            ));

INSTANTIATE_TEST_CASE_P(LogicalExpressionTest, LogicalExprTest, ::testing::Values(
                            InputOutputPair{"true",            "true"},
                            InputOutputPair{"1 == 1",            "true"},
                            InputOutputPair{"1 != 1",            "false"},
                            InputOutputPair{"2 > 1",             "true"},
                            InputOutputPair{"1 > 1",            "false"},
                            InputOutputPair{"2 >= 1",             "true"},
                            InputOutputPair{"1 >= 1",            "true"},
                            InputOutputPair{"1 < 2",             "true"},
                            InputOutputPair{"1 < 1",            "false"},
                            InputOutputPair{"1 <= 2",             "true"},
                            InputOutputPair{"1 <= 1",            "true"},
                            InputOutputPair{"1 == 2 or 2 == 2",  "true"},
                            InputOutputPair{"2 == 2 or 1 == 2",  "true"},
                            InputOutputPair{"1 == 2 or 3 == 2",  "false"},
                            InputOutputPair{"1 == 2 and 2 == 2",  "false"},
                            InputOutputPair{"2 == 2 and 1 == 2",  "false"},
                            InputOutputPair{"1 == 2 and 3 == 2",  "false"},
                            InputOutputPair{"1 == 1 and 2 == 2",  "true"},
                            InputOutputPair{"not (1 == 2) and 2 == 2",  "true"},
                            InputOutputPair{"not false",         "true"},
                            InputOutputPair{"true and true and true",         "true"},
                            InputOutputPair{"false",             "false"}
                            ));

INSTANTIATE_TEST_CASE_P(BasicValueSubstitutionTest, ExpressionSubstitutionTest, ::testing::Values(
                            InputOutputPair{"intValue",       "3"},
                            InputOutputPair{"doubleValue",    "12.123"},
                            InputOutputPair{"stringValue",    "rain"},
                            InputOutputPair{"boolTrueValue",  "true"},
                            InputOutputPair{"boolFalseValue", "false"}
                            ));

INSTANTIATE_TEST_CASE_P(IndexSubscriptionTest, ExpressionSubstitutionTest, ::testing::Values(
                            InputOutputPair{"intValue[0]",               ""},
                            InputOutputPair{"doubleValue[0]",            ""},
                            InputOutputPair{"stringValue[0]",            "r"},
                            InputOutputPair{"stringValue[100]",          ""},
                            InputOutputPair{"boolTrueValue[0]",          ""},
                            InputOutputPair{"boolFalseValue[0]",         ""},
                            InputOutputPair{"intList[-1]",               ""},
                            InputOutputPair{"intList[10]",               ""},
                            InputOutputPair{"intList[0]",                "9"},
                            InputOutputPair{"intList[9]",                "4"},
                            InputOutputPair{"intList[5]",                "2"},
                            InputOutputPair{"mapValue['intVal']",        "10"},
                            InputOutputPair{"mapValue['dblVal']",        "100.5"},
                            InputOutputPair{"mapValue['stringVal']",     "string100.5"},
                            InputOutputPair{"mapValue['boolValue']",     "true"},
                            InputOutputPair{"mapValue['intVAl']",        ""},
                            InputOutputPair{"mapValue[0]",               ""},
                            InputOutputPair{"(mapValue | dictsort | first)['key']", "boolValue"},
                            InputOutputPair{"(mapValue | dictsort | first)['value']", "true"},
                            InputOutputPair{ "reflectedStringVector[0]", "9" },
                            InputOutputPair{ "reflectedStringViewVector[0]", "9" },
                            InputOutputPair{"reflectedVal['intValue']",  "0"},
                            InputOutputPair{"reflectedVal['dblValue']",  "0"},
                            InputOutputPair{"reflectedVal['boolValue']", "false"},
                            InputOutputPair{"reflectedVal['strValue']",  "test string 0"},
                            InputOutputPair{"reflectedVal['StrValue']",  ""}
                            ));

INSTANTIATE_TEST_CASE_P(DotSubscriptionTest, ExpressionSubstitutionTest, ::testing::Values(InputOutputPair{ "mapValue.intVal", "10" },
                                          InputOutputPair{ "mapValue.dblVal", "100.5" },
                                          InputOutputPair{ "mapValue.stringVal", "string100.5" },
                                          InputOutputPair{ "mapValue.boolValue", "true" },
                                          InputOutputPair{ "mapValue.intVAl", "" },
                                          InputOutputPair{ "reflectedVal.intValue", "0" },
                                          InputOutputPair{ "reflectedVal.dblValue", "0" },
                                          InputOutputPair{ "reflectedVal.boolValue", "false" },
                                          InputOutputPair{ "reflectedVal.strValue", "test string 0" },
                                          InputOutputPair{ "reflectedVal.wstrValue", "test string 0" },
                                          InputOutputPair{ "reflectedVal.strViewValue", "test string 0" },
                                          InputOutputPair{ "reflectedVal.wstrViewValue", "test string 0" },
                                          InputOutputPair{ "reflectedVal.StrValue", "" }));


INSTANTIATE_TEST_CASE_P(ComplexSubscriptionTest, ExpressionSubstitutionTest, ::testing::Values(
                            InputOutputPair{"mapValue.reflectedList[1]['intValue']",    "1"},
                            InputOutputPair{"mapValue['reflectedList'][1]['intValue']",    "1"},
                            InputOutputPair{"mapValue.reflectedList[1].intValue",    "1"},
                            InputOutputPair{"{'fieldName'='field', 'fieldValue'=10}.fieldName",    "field"},
                            InputOutputPair{"{'fieldName'='field', 'fieldValue'=10}['fieldValue']",    "10"},
                            InputOutputPair{R"( ([
                                                    {'fieldName'='field1', 'fieldValue'=10},
                                                    {'fieldName'='field2', 'fieldValue'=11},
                                                    {'fieldName'='field3', 'fieldValue'=12}
                                                 ] | last).fieldValue)",    "12"},
                            InputOutputPair{"reflectedList[1].intValue",    "1"},
                            InputOutputPair{"(reflectedList[1]).intValue",    "1"},
                            InputOutputPair{"(reflectedList | first).intValue",    "0"},
                            InputOutputPair{"reflectedList[1].strValue[0]",    "t"},
                            InputOutputPair{"(reflectedList[1]).strValue[0]",    "t"},
                            InputOutputPair{"(reflectedList | first).strValue[0]",    "t"},
                            InputOutputPair{"reflectedVal.strValue[0]",        "t"},
                            InputOutputPair{"reflectedVal.innerStruct.strValue", "Hello World!"},
                            InputOutputPair{"reflectedVal.innerStructList[5].strValue", "Hello World!"},
                            InputOutputPair{"reflectedVal.tmpStructList[5].strValue", "Hello World!"}
                            ));
