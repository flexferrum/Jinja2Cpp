#include <iostream>
#include <string>

#include "test_tools.h"
#include "jinja2cpp/template.h"

using namespace jinja2;

struct ErrorsGenericTestTag;
using ErrorsGenericTest = InputOutputPairTest<ErrorsGenericTestTag>;


TEST_P(ErrorsGenericTest, Test)
{
    auto& testParam = GetParam();
    std::string source = testParam.tpl;

    Template tpl;
    auto parseResult = tpl.Load(source);
    EXPECT_TRUE(parseResult.HasError());

    std::ostringstream errorDescr;
    errorDescr << parseResult;
    std::string result = errorDescr.str();
    std::cout << result << std::endl;
    std::string expectedResult = testParam.result;
    EXPECT_EQ(expectedResult, result);
}

INSTANTIATE_TEST_CASE_P(BasicTest, ErrorsGenericTest, ::testing::Values(
                            InputOutputPair{"{{}}",
                                            "noname.j2tpl:1:3: error: Unexpected token: '<<End of block>>'\n{{}}\n--^-------"},
                            InputOutputPair{"{{ ) }}",
                                            "noname.j2tpl:1:4: error: Unexpected token: ')'\n{{ ) }}\n---^-------"},
                            InputOutputPair{"{% %}",
                                            "noname.j2tpl:1:4: error: Unexpected token: '<<End of block>>'\n{% %}\n---^-------"},
                            InputOutputPair{"{% if %}",
                                            "noname.j2tpl:1:7: error: Expected expression, got: '<<End of block>>'\n{% if %}\n   ---^-------"},
                            InputOutputPair{"{% endif %}",
                                            "noname.j2tpl:1:4: error: Unexpected statement: 'endif'\n{% endif %}\n---^-------"},
                            InputOutputPair{"Hello World!\n    {% if %}",
                                            "noname.j2tpl:2:11: error: Expected expression, got: '<<End of block>>'\n    {% if %}\n       ---^-------"},
                            InputOutputPair{"Hello World!\n\t{% if %}",
                                            "noname.j2tpl:2:8: error: Expected expression, got: '<<End of block>>'\n\t{% if %}\n\t   ---^-------"},
                            InputOutputPair{"{{",
                                            "noname.j2tpl:1:3: error: Unexpected token: '<<End of block>>'\n{{\n--^-------"},
                            InputOutputPair{"}}",
                                            "noname.j2tpl:1:1: error: Unexpected expression block end\n}}\n^-------"}
                            ));
