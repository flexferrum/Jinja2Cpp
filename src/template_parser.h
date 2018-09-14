#ifndef TEMPLATE_PARSER_H
#define TEMPLATE_PARSER_H

#include "renderer.h"
#include "template_parser.h"
#include "lexer.h"
#include "lexertk.h"
#include "error_handling.h"
#include "expression_evaluator.h"
#include "expression_parser.h"
#include "statements.h"
#include "helpers.h"
#include "value_visitors.h"

#include <jinja2cpp/error_info.h>
#include <jinja2cpp/template_env.h>

#include <nonstd/expected.hpp>

#include <string>
#include <regex>
#include <vector>
#include <iostream>
#include <list>
#include <sstream>

namespace jinja2
{
template<typename CharT>
struct ParserTraits;

struct KeywordsInfo
{
    MultiStringLiteral name;
    Token::Type type;
};

struct TokenStrInfo : MultiStringLiteral
{
    template<typename CharT>
    auto GetName() const
    {
        return MultiStringLiteral::template GetValue<CharT>();
    }

};

template<typename T = void>
struct ParserTraitsBase
{
    static Token::Type s_keywords[];
    static KeywordsInfo s_keywordsInfo[30];
    static std::unordered_map<int, MultiStringLiteral> s_tokens;
};

template<>
struct ParserTraits<char> : public ParserTraitsBase<>
{
    static std::regex GetRoughTokenizer()
    {
        return std::regex(R"((\{\{)|(\}\})|(\{%)|(%\})|(\{#)|(#\})|(\n))");
    }
    static std::regex GetKeywords()
    {
        std::string pattern;
        std::string prefix("(^");
        std::string postfix("$)");

        bool isFirst = true;
        for (auto& info : s_keywordsInfo)
        {
            if (!isFirst)
                pattern += "|";
            else
                isFirst = false;

            pattern += prefix + info.name.charValue + postfix;
        }
        return std::regex(pattern);
    }
    static std::string GetAsString(const std::string& str, CharRange range)
    {
        return str.substr(range.startOffset, range.size());
    }
    static InternalValue RangeToNum(const std::string& str, CharRange range, Token::Type hint)
    {
        char buff[std::max(std::numeric_limits<int64_t>::max_digits10, std::numeric_limits<double>::max_digits10) * 2 + 1];
        std::copy(str.data() + range.startOffset, str.data() + range.endOffset, buff);
        buff[range.size()] = 0;
        InternalValue result;
        if (hint == Token::IntegerNum)
        {
            result = InternalValue(static_cast<int64_t>(strtoll(buff, nullptr, 0)));
        }
        else
        {
            char* endBuff = nullptr;
            int64_t val = strtoll(buff, &endBuff, 10);
            if ((errno == ERANGE) || *endBuff)
            {
                endBuff = nullptr;
                double dblVal = strtod(buff, nullptr);
                result = static_cast<double>(dblVal);
            }
            else
                result = static_cast<int64_t>(val);
        }
        return result;
    }
};

template<>
struct ParserTraits<wchar_t> : public ParserTraitsBase<>
{
    static std::wregex GetRoughTokenizer()
    {
        return std::wregex(LR"((\{\{)|(\}\})|(\{%)|(%\})|(\{#)|(#\})|(\n))");
    }
    static std::wregex GetKeywords()
    {
        std::wstring pattern;
        std::wstring prefix(L"(^");
        std::wstring postfix(L"$)");

        bool isFirst = true;
        for (auto& info : s_keywordsInfo)
        {
            if (!isFirst)
                pattern += L"|";
            else
                isFirst = false;

            pattern += prefix + info.name.wcharValue + postfix;
        }
        return std::wregex(pattern);
    }
    static std::string GetAsString(const std::wstring& str, CharRange range)
    {
        auto tmpStr = str.substr(range.startOffset, range.size());
        std::string result;
        result.resize(tmpStr.size(), '\0');
#ifdef _MSC_VER
        size_t dummy = 0;
        wcstombs_s(&dummy, &result[0], result.size(), tmpStr.c_str(), tmpStr.size());
#else
        wcstombs(&result[0], tmpStr.c_str(), result.size());
#endif
        return result;
    }
    static InternalValue RangeToNum(const std::wstring& /*str*/, CharRange /*range*/, Token::Type /*hint*/)
    {
        return InternalValue();
    }
};

struct StatementInfo
{
    enum Type
    {
        TemplateRoot,
        IfStatement,
        ElseIfStatement,
        ForStatement,
        SetStatement,
        ExtendsStatement,
        BlockStatement,
        ParentBlockStatement,
        MacroStatement,
        MacroCallStatement
    };

    using ComposedPtr = std::shared_ptr<ComposedRenderer>;
    Type type;
    ComposedPtr currentComposition;
    std::vector<ComposedPtr> compositions;
    Token token;
    RendererPtr renderer;

    static StatementInfo Create(Type type, const Token& tok, ComposedPtr renderers = std::make_shared<ComposedRenderer>())
    {
        StatementInfo result;
        result.type = type;
        result.currentComposition = renderers;
        result.compositions.push_back(renderers);
        result.token = tok;
        return result;
    }
};

using StatementInfoList = std::list<StatementInfo>;

class StatementsParser
{
public:
    using ParseResult = nonstd::expected<void, ParseError>;

    ParseResult Parse(LexScanner& lexer, StatementInfoList& statementsInfo);

private:
    ParseResult ParseFor(LexScanner &lexer, StatementInfoList &statementsInfo, const Token& stmtTok);
    ParseResult ParseEndFor(LexScanner& lexer, StatementInfoList& statementsInfo, const Token& stmtTok);
    ParseResult ParseIf(LexScanner &lexer, StatementInfoList &statementsInfo, const Token& stmtTok);
    ParseResult ParseElse(LexScanner& lexer, StatementInfoList& statementsInfo, const Token& stmtTok);
    ParseResult ParseElIf(LexScanner& lexer, StatementInfoList& statementsInfo, const Token& stmtTok);
    ParseResult ParseEndIf(LexScanner& lexer, StatementInfoList& statementsInfo, const Token& pos);
    ParseResult ParseSet(LexScanner& lexer, StatementInfoList& statementsInfo, const Token& pos);
    ParseResult ParseEndSet(LexScanner& lexer, StatementInfoList& statementsInfo, const Token& stmtTok);
    ParseResult ParseBlock(LexScanner& lexer, StatementInfoList& statementsInfo, const Token& stmtTok);
    ParseResult ParseEndBlock(LexScanner& lexer, StatementInfoList& statementsInfo, const Token& stmtTok);
    ParseResult ParseExtends(LexScanner& lexer, StatementInfoList& statementsInfo, const Token& stmtTok);
    ParseResult ParseMacro(LexScanner& lexer, StatementInfoList& statementsInfo, const Token& stmtTok);
    nonstd::expected<MacroParams, ParseError> ParseMacroParams(LexScanner& lexer);
    ParseResult ParseEndMacro(LexScanner& lexer, StatementInfoList& statementsInfo, const Token& stmtTok);
    ParseResult ParseCall(LexScanner& lexer, StatementInfoList& statementsInfo, const Token& stmtTok);
    ParseResult ParseEndCall(LexScanner& lexer, StatementInfoList& statementsInfo, const Token& stmtTok);
};

template<typename CharT>
class TemplateParser : public LexerHelper
{
public:
    using string_t = std::basic_string<CharT>;
    using traits_t = ParserTraits<CharT>;
    using sregex_iterator = std::regex_iterator<typename string_t::const_iterator>;
    using ErrorInfo = ErrorInfoTpl<CharT>;
    using ParseResult = nonstd::expected<RendererPtr, std::vector<ErrorInfo>>;

    TemplateParser(const string_t* tpl, const Settings& setts, std::string tplName)
        : m_template(tpl)
        , m_templateName(std::move(tplName))
        , m_settings(setts)
        , m_roughTokenizer(traits_t::GetRoughTokenizer())
        , m_keywords(traits_t::GetKeywords())
    {
    }

    ParseResult Parse()
    {
        auto roughResult = DoRoughParsing();

        if (!roughResult)
        {
            return ParseErrorsToErrorInfo(roughResult.error());
        }

        auto composeRenderer = std::make_shared<ComposedRenderer>();

        auto fineResult = DoFineParsing(composeRenderer);
        if (!fineResult)
            return ParseErrorsToErrorInfo(fineResult.error());

        return composeRenderer;
    }

private:
    enum
    {
        RM_Unknown = 0,
        RM_ExprBegin = 1,
        RM_ExprEnd,
        RM_StmtBegin,
        RM_StmtEnd,
        RM_CommentBegin,
        RM_CommentEnd,
        RM_NewLine
    };

    struct LineInfo
    {
        CharRange range;
        unsigned lineNumber;
    };

    enum class TextBlockType
    {
        RawText,
        Expression,
        Statement,
        Comment,
        LineStatement
    };

    struct TextBlockInfo
    {
        CharRange range;
        TextBlockType type;
    };

    nonstd::expected<void, std::vector<ParseError>> DoRoughParsing()
    {
        std::vector<ParseError> foundErrors;

        auto matchBegin = sregex_iterator(m_template->begin(), m_template->end(), m_roughTokenizer);
        auto matchEnd = sregex_iterator();

        auto matches = std::distance(matchBegin, matchEnd);
        // One line, no customization
        if (matches == 0)
        {
            CharRange range{0ULL, m_template->size()};
            m_lines.push_back(LineInfo{range, 0});
            m_textBlocks.push_back(TextBlockInfo{range, (!m_template->empty() && m_template->front() == '#') ? TextBlockType::LineStatement : TextBlockType::RawText});
            return nonstd::expected<void, std::vector<ParseError>>();
        }

        m_currentBlockInfo.range.startOffset = 0;
        m_currentBlockInfo.range.endOffset = 0;
        m_currentLineInfo.range = m_currentBlockInfo.range;
        m_currentLineInfo.lineNumber = 0;
        if (m_settings.useLineStatements)
            m_currentBlockInfo.type = m_template->front() == '#' ? TextBlockType::LineStatement : TextBlockType::RawText;
        else
            m_currentBlockInfo.type = TextBlockType::RawText;
        do
        {
            auto result = ParseRoughMatch(matchBegin, matchEnd);
            if (!result)
            {
                foundErrors.push_back(result.error());
            }
        } while (matchBegin != matchEnd);
        FinishCurrentLine(m_template->size());
        FinishCurrentBlock(m_template->size());

        if (!foundErrors.empty())
            return nonstd::make_unexpected(std::move(foundErrors));
        return nonstd::expected<void, std::vector<ParseError>>();
    }
    nonstd::expected<void, ParseError> ParseRoughMatch(sregex_iterator& curMatch, const sregex_iterator& /*endMatch*/)
    {
        auto match = *curMatch;
        ++ curMatch;
        unsigned matchType = RM_Unknown;
        for (unsigned idx = 1; idx != match.size(); ++ idx)
        {
            if (match.length(idx) != 0)
            {
                matchType = idx;
                break;
            }
        }

        size_t matchStart = static_cast<size_t>(match.position());

        switch (matchType)
        {
        case RM_NewLine:
            FinishCurrentLine(match.position());
            m_currentLineInfo.range.startOffset = m_currentLineInfo.range.endOffset + 1;
            if (m_currentLineInfo.range.startOffset < m_template->size() &&
                    (m_currentBlockInfo.type == TextBlockType::RawText || m_currentBlockInfo.type == TextBlockType::LineStatement))
            {
                if (m_currentBlockInfo.type == TextBlockType::LineStatement)
                {
                    FinishCurrentBlock(matchStart);
                    m_currentBlockInfo.range.startOffset = m_currentLineInfo.range.startOffset;
                }

                if (m_settings.useLineStatements)
                    m_currentBlockInfo.type = (*m_template)[m_currentLineInfo.range.startOffset] == '#' ? TextBlockType::LineStatement : TextBlockType::RawText;
                else
                    m_currentBlockInfo.type = TextBlockType::RawText;
            }
            break;
        case RM_CommentBegin:
            if (m_currentBlockInfo.type != TextBlockType::RawText)
                return  MakeParseError(ErrorCode::UnexpectedCommentBegin, MakeToken(Token::CommentBegin, {matchStart, matchStart + 2}));

            FinishCurrentBlock(matchStart);
            m_currentBlockInfo.range.startOffset = matchStart + 2;
            m_currentBlockInfo.type = TextBlockType::Comment;
            break;

        case RM_CommentEnd:
            if (m_currentBlockInfo.type != TextBlockType::Comment)
                return  MakeParseError(ErrorCode::UnexpectedCommentEnd, MakeToken(Token::CommentEnd, {matchStart, matchStart + 2}));

            FinishCurrentBlock(matchStart);
            m_currentBlockInfo.range.startOffset = matchStart + 2;
            break;
        case RM_ExprBegin:
            StartControlBlock(TextBlockType::Expression, matchStart);
            break;
        case RM_ExprEnd:
            if (m_currentBlockInfo.type == TextBlockType::RawText)
                return  MakeParseError(ErrorCode::UnexpectedExprEnd, MakeToken(Token::ExprEnd, {matchStart, matchStart + 2}));
            else if (m_currentBlockInfo.type != TextBlockType::Expression || (*m_template)[match.position() - 1] == '\'')
                break;

            m_currentBlockInfo.range.startOffset = FinishCurrentBlock(matchStart);
            break;
        case RM_StmtBegin:
            StartControlBlock(TextBlockType::Statement, matchStart);
            break;
        case RM_StmtEnd:
            if (m_currentBlockInfo.type == TextBlockType::RawText)
                return  MakeParseError(ErrorCode::UnexpectedStmtEnd, MakeToken(Token::StmtEnd, {matchStart, matchStart + 2}));
            else if (m_currentBlockInfo.type != TextBlockType::Statement || (*m_template)[match.position() - 1] == '\'')
                break;

            m_currentBlockInfo.range.startOffset = FinishCurrentBlock(matchStart);
            break;
        }

        return nonstd::expected<void, ParseError>();
    }

    void StartControlBlock(TextBlockType blockType, size_t matchStart)
    {
        size_t startOffset = matchStart + 2;
        size_t endOffset = matchStart;
        if (m_currentBlockInfo.type != TextBlockType::RawText)
            return;
        else
            endOffset = StripBlockLeft(m_currentBlockInfo, startOffset, endOffset);

        FinishCurrentBlock(endOffset);
        if (startOffset < m_template->size())
        {
            if ((*m_template)[startOffset] == '+' ||
                    (*m_template)[startOffset] == '-')
                ++ startOffset;
        }
        m_currentBlockInfo.range.startOffset = startOffset;
        m_currentBlockInfo.type = blockType;
    }

    size_t StripBlockLeft(TextBlockInfo& currentBlockInfo, size_t ctrlCharPos, size_t endOffset)
    {
        bool doStrip = m_settings.lstripBlocks;
        if (ctrlCharPos < m_template->size())
        {
            auto ctrlChar = (*m_template)[ctrlCharPos];
            doStrip = ctrlChar == '+' ? false : (ctrlChar == '-' ? true : doStrip);
        }
        if (!doStrip || currentBlockInfo.type != TextBlockType::RawText)
            return endOffset;

        auto locale = std::locale();
        auto& tpl = *m_template;
        for (; endOffset > 0; -- endOffset)
        {
            auto ch = tpl[endOffset - 1];
            if (!std::isspace(ch, locale) || ch == '\n')
                break;
        }
        return endOffset;
    }

    nonstd::expected<void, std::vector<ParseError>> DoFineParsing(std::shared_ptr<ComposedRenderer> renderers)
    {
        std::vector<ParseError> errors;
        TextBlockInfo* prevBlock = nullptr;
        StatementInfoList statementsStack;
        StatementInfo root = StatementInfo::Create(StatementInfo::TemplateRoot, Token(), renderers);
        statementsStack.push_back(root);
        for (auto& origBlock : m_textBlocks)
        {
            auto block = origBlock;
            if (block.type == TextBlockType::LineStatement)
                ++ block.range.startOffset;

            switch (block.type)
            {
            case TextBlockType::RawText:
            {
                if (block.range.size() == 0)
                    break;
                auto range = block.range;
                if ((*m_template)[range.startOffset] == '\n' && prevBlock != nullptr &&
                        prevBlock->type != TextBlockType::RawText && prevBlock->type != TextBlockType::Expression)
                    range.startOffset ++;
                if (range.size() == 0)
                    break;
                auto renderer = std::make_shared<RawTextRenderer>(m_template->data() + range.startOffset, range.size());
                statementsStack.back().currentComposition->AddRenderer(renderer);
                break;
            }
            case TextBlockType::Expression:
            {
                auto parseResult = InvokeParser<RendererPtr, ExpressionParser>(block);
                if (parseResult)
                    statementsStack.back().currentComposition->AddRenderer(*parseResult);
                else
                    errors.push_back(parseResult.error());
                break;
            }
            case TextBlockType::Statement:
            case TextBlockType::LineStatement:
            {
                auto parseResult = InvokeParser<void, StatementsParser>(block, statementsStack);
                if (!parseResult)
                    errors.push_back(parseResult.error());
                break;
            }
            default:
                break;
            }
            prevBlock = &origBlock;
        }

        if (!errors.empty())
            return nonstd::make_unexpected(std::move(errors));

        return nonstd::expected<void, std::vector<ParseError>>();
    }
    template<typename R, typename P, typename ... Args>
    nonstd::expected<R, ParseError> InvokeParser(const TextBlockInfo& block, Args&& ... args)
    {
        lexertk::generator<CharT> tokenizer;
        auto range = block.range;
        auto start = m_template->data();
        if (!tokenizer.process(start + range.startOffset, start + range.endOffset))
            return MakeParseError(ErrorCode::Unspecified, MakeToken(Token::Unknown, {range.startOffset, range.startOffset + 1}));

        tokenizer.begin();
        Lexer lexer([this, &tokenizer, adjust = range.startOffset]() mutable {
            lexertk::token tok = tokenizer.next_token();
            tok.position += adjust;
            return tok;
        }, this);

        if (!lexer.Preprocess())
            return MakeParseError(ErrorCode::Unspecified, MakeToken(Token::Unknown, {range.startOffset, range.startOffset + 1}));

        P praser;
        LexScanner scanner(lexer);
        auto result = praser.Parse(scanner, std::forward<Args>(args)...);
        if (!result)
            return result.get_unexpected();

        return result;
    }

    nonstd::unexpected_type<std::vector<ErrorInfo>> ParseErrorsToErrorInfo(const std::vector<ParseError>& errors)
    {
        std::vector<ErrorInfo> resultErrors;

        for (auto& e : errors)
        {
            typename ErrorInfo::Data errInfoData;
            errInfoData.code = e.errorCode;
            errInfoData.srcLoc.fileName = m_templateName;
            OffsetToLinePos(e.errorToken.range.startOffset, errInfoData.srcLoc.line, errInfoData.srcLoc.col);
            errInfoData.locationDescr = GetLocationDescr(errInfoData.srcLoc.line, errInfoData.srcLoc.col);
            errInfoData.extraParams.emplace_back(TokenToString(e.errorToken));
            for (auto& tok : e.relatedTokens)
            {
                errInfoData.extraParams.emplace_back(TokenToString(tok));
                if (tok.range.startOffset != e.errorToken.range.startOffset)
                {
                    SourceLocation relLoc;
                    relLoc.fileName = m_templateName;
                    OffsetToLinePos(tok.range.startOffset, relLoc.line, relLoc.col);
                    errInfoData.relatedLocs.push_back(std::move(relLoc));
                }
            }

            resultErrors.emplace_back(errInfoData);
        }

        return nonstd::make_unexpected(std::move(resultErrors));
    }

    Token MakeToken(Token::Type type, const CharRange& range, string_t value = string_t())
    {
        Token tok;
        tok.type = type;
        tok.range = range;
        tok.value = static_cast<string_t>(value);

        return tok;
    }

    auto TokenToString(const Token& tok)
    {
        auto p = traits_t::s_tokens.find(tok.type);
        if (p != traits_t::s_tokens.end())
            return p->second.template GetValue<CharT>();

        if (tok.range.size() != 0)
            return m_template->substr(tok.range.startOffset, tok.range.size());
        else if (tok.type == Token::Identifier)
        {
            if (tok.value.index() != 0)
            {
                std::basic_string<CharT> tpl;
                return GetAsSameString(tpl, tok.value);
            }

            return UNIVERSAL_STR("<<Identifier>>").template GetValue<CharT>();
        }
        else if (tok.type == Token::String)
            return UNIVERSAL_STR("<<String>>").template GetValue<CharT>();

        return string_t();
    }

    size_t FinishCurrentBlock(size_t position)
    {
        bool doTrim = m_settings.trimBlocks && m_currentBlockInfo.type == TextBlockType::Statement;
        size_t newPos = position + 2;

        if ((m_currentBlockInfo.type != TextBlockType::RawText) && position != 0)
        {
            auto ctrlChar = (*m_template)[position - 1];
            doTrim = ctrlChar == '-' ? true : (ctrlChar == '+' ? false : doTrim);
            if (ctrlChar == '+' || ctrlChar == '-')
                -- position;
        }

        if (doTrim)
        {
            auto locale = std::locale();
            for (;newPos < m_template->size(); ++ newPos)
            {
                auto ch = (*m_template)[newPos];
                if (ch == '\n')
                {
                    ++ newPos;
                    break;
                }
                if (!std::isspace(ch, locale))
                    break;
            }
        }
        m_currentBlockInfo.range.endOffset = position;
        m_textBlocks.push_back(m_currentBlockInfo);
        m_currentBlockInfo.type = TextBlockType::RawText;
        return newPos;
    }

    void FinishCurrentLine(int64_t position)
    {
        m_currentLineInfo.range.endOffset = static_cast<size_t>(position);
        m_lines.push_back(m_currentLineInfo);
        m_currentLineInfo.lineNumber ++;
    }

    void OffsetToLinePos(size_t offset, unsigned& line, unsigned& col)
    {
        auto p = std::find_if(m_lines.begin(), m_lines.end(), [offset](const LineInfo& info) {
            return offset >= info.range.startOffset && offset < info.range.endOffset;});

        if (p == m_lines.end())
        {
            if (offset != m_lines.back().range.endOffset)
            {
                line = 0;
                col = 0;
                return;
            }
            p = m_lines.end() - 1;
        }

        line = p->lineNumber + 1;
        col = static_cast<unsigned>(offset - p->range.startOffset + 1);
    }

    string_t GetLocationDescr(unsigned line, unsigned col)
    {
        if (line == 0 && col == 0)
            return string_t();

        -- line;
        -- col;

        auto toCharT = [](char ch) {return static_cast<CharT>(ch);};

        auto& lineInfo = m_lines[line];
        std::basic_ostringstream<CharT> os;
        auto origLine = m_template->substr(lineInfo.range.startOffset, lineInfo.range.size());
        os << origLine << std::endl;

        string_t spacePrefix;
        auto locale = std::locale();
        for (auto ch : origLine)
        {
            if (!std::isspace(ch, locale))
                break;
            spacePrefix.append(1, ch);
        }

        const int headLen = 3;
        const int tailLen = 7;
        auto spacePrefixLen = spacePrefix.size();

        if (col < spacePrefixLen)
        {
            for (unsigned i = 0; i < col; ++ i)
                os << toCharT(' ');

            os << toCharT('^');
            for (int i = 0; i < tailLen; ++ i)
                os << toCharT('-');
            return os.str();
        }

        os << spacePrefix;
        int actualHeadLen = std::min(static_cast<int>(col - spacePrefixLen), headLen);

        if (actualHeadLen == headLen)
        {
            for (int i = 0; i < col - actualHeadLen - spacePrefixLen; ++ i)
                os << toCharT(' ');
        }
        for (int i = 0; i < actualHeadLen; ++ i)
            os << toCharT('-');
        os << toCharT('^');
        for (int i = 0; i < tailLen; ++ i)
            os << toCharT('-');

        return os.str();
    }

    // LexerHelper interface
    std::string GetAsString(const CharRange& range) override
    {
        return traits_t::GetAsString(*m_template, range);
    }
    InternalValue GetAsValue(const CharRange& range, Token::Type type) override
    {
        if (type == Token::String)
            return InternalValue(m_template->substr(range.startOffset, range.size()));
        else if (type == Token::IntegerNum || type == Token::FloatNum)
            return traits_t::RangeToNum(*m_template, range, type);
        return InternalValue();
    }
    Token::Type GetKeyword(const CharRange& range) override
    {
        auto matchBegin = sregex_iterator(m_template->begin() + range.startOffset, m_template->begin() + range.endOffset, m_keywords);
        auto matchEnd = sregex_iterator();

        auto matches = std::distance(matchBegin, matchEnd);
        // One line, no customization
        if (matches == 0)
            return Token::Unknown;

        auto& match = *matchBegin;
        for (size_t idx = 1; idx != match.size(); ++ idx)
        {
            if (match.length(idx) != 0)
            {
                return traits_t::s_keywordsInfo[idx - 1].type;
            }
        }

        return Token::Unknown;
    }
    char GetCharAt(size_t /*pos*/) override
    {
        return '\0';
    }
private:
    const string_t* m_template;
    std::string m_templateName;
    const Settings& m_settings;
    std::basic_regex<CharT> m_roughTokenizer;
    std::basic_regex<CharT> m_keywords;
    std::vector<LineInfo> m_lines;
    std::vector<TextBlockInfo> m_textBlocks;
    LineInfo m_currentLineInfo;
    TextBlockInfo m_currentBlockInfo;
};

template<typename T>
KeywordsInfo ParserTraitsBase<T>::s_keywordsInfo[30] = {
    {UNIVERSAL_STR("for"), Token::For},
    {UNIVERSAL_STR("endfor"), Token::Endfor},
    {UNIVERSAL_STR("in"), Token::In},
    {UNIVERSAL_STR("if"), Token::If},
    {UNIVERSAL_STR("else"), Token::Else},
    {UNIVERSAL_STR("elif"), Token::ElIf},
    {UNIVERSAL_STR("endif"), Token::EndIf},
    {UNIVERSAL_STR("or"), Token::LogicalOr},
    {UNIVERSAL_STR("and"), Token::LogicalAnd},
    {UNIVERSAL_STR("not"), Token::LogicalNot},
    {UNIVERSAL_STR("is"), Token::Is},
    {UNIVERSAL_STR("block"), Token::Block},
    {UNIVERSAL_STR("endblock"), Token::EndBlock},
    {UNIVERSAL_STR("extends"), Token::Extends},
    {UNIVERSAL_STR("macro"), Token::Macro},
    {UNIVERSAL_STR("endmacro"), Token::EndMacro},
    {UNIVERSAL_STR("call"), Token::Call},
    {UNIVERSAL_STR("endcall"), Token::EndCall},
    {UNIVERSAL_STR("filter"), Token::Filter},
    {UNIVERSAL_STR("endfilter"), Token::EndFilter},
    {UNIVERSAL_STR("set"), Token::Set},
    {UNIVERSAL_STR("endset"), Token::EndSet},
    {UNIVERSAL_STR("include"), Token::Include},
    {UNIVERSAL_STR("import"), Token::Import},
    {UNIVERSAL_STR("true"), Token::True},
    {UNIVERSAL_STR("false"), Token::False},
    {UNIVERSAL_STR("True"), Token::True},
    {UNIVERSAL_STR("False"), Token::False},
    {UNIVERSAL_STR("none"), Token::None},
    {UNIVERSAL_STR("None"), Token::None},
};

template<typename T>
std::unordered_map<int, MultiStringLiteral> ParserTraitsBase<T>::s_tokens = {
        {Token::Unknown, UNIVERSAL_STR("<<Unknown>>")},
        {Token::Lt, UNIVERSAL_STR("<")},
        {Token::Gt, UNIVERSAL_STR(">")},
        {Token::Plus, UNIVERSAL_STR("+")},
        {Token::Minus, UNIVERSAL_STR("-")},
        {Token::Percent, UNIVERSAL_STR("%")},
        {Token::Mul, UNIVERSAL_STR("*")},
        {Token::Div, UNIVERSAL_STR("/")},
        {Token::LBracket, UNIVERSAL_STR("(")},
        {Token::RBracket, UNIVERSAL_STR(")")},
        {Token::LSqBracket, UNIVERSAL_STR("[")},
        {Token::RSqBracket, UNIVERSAL_STR("]")},
        {Token::LCrlBracket, UNIVERSAL_STR("{")},
        {Token::RCrlBracket, UNIVERSAL_STR("}")},
        {Token::Assign, UNIVERSAL_STR("=")},
        {Token::Comma, UNIVERSAL_STR(",")},
        {Token::Eof, UNIVERSAL_STR("<<End of block>>")},
        {Token::Equal, UNIVERSAL_STR("==")},
        {Token::NotEqual, UNIVERSAL_STR("!=")},
        {Token::LessEqual, UNIVERSAL_STR("<=")},
        {Token::GreaterEqual, UNIVERSAL_STR(">=")},
        {Token::StarStar, UNIVERSAL_STR("**")},
        {Token::DashDash, UNIVERSAL_STR("//")},
        {Token::LogicalOr, UNIVERSAL_STR("or")},
        {Token::LogicalAnd, UNIVERSAL_STR("and")},
        {Token::LogicalNot, UNIVERSAL_STR("not")},
        {Token::MulMul, UNIVERSAL_STR("**")},
        {Token::DivDiv, UNIVERSAL_STR("//")},
        {Token::True, UNIVERSAL_STR("true")},
        {Token::False, UNIVERSAL_STR("false")},
        {Token::None, UNIVERSAL_STR("none")},
        {Token::In, UNIVERSAL_STR("in")},
        {Token::Is, UNIVERSAL_STR("is")},
        {Token::For, UNIVERSAL_STR("for")},
        {Token::Endfor, UNIVERSAL_STR("endfor")},
        {Token::If, UNIVERSAL_STR("if")},
        {Token::Else, UNIVERSAL_STR("else")},
        {Token::ElIf, UNIVERSAL_STR("elif")},
        {Token::EndIf, UNIVERSAL_STR("endif")},
        {Token::Block, UNIVERSAL_STR("block")},
        {Token::EndBlock, UNIVERSAL_STR("endblock")},
        {Token::Extends, UNIVERSAL_STR("extends")},
        {Token::Macro, UNIVERSAL_STR("macro")},
        {Token::EndMacro, UNIVERSAL_STR("endmacro")},
        {Token::Call, UNIVERSAL_STR("call")},
        {Token::EndCall, UNIVERSAL_STR("endcall")},
        {Token::Filter, UNIVERSAL_STR("filter")},
        {Token::EndFilter, UNIVERSAL_STR("endfilter")},
        {Token::Set, UNIVERSAL_STR("set")},
        {Token::EndSet, UNIVERSAL_STR("endset")},
        {Token::Include, UNIVERSAL_STR("include")},
        {Token::Import, UNIVERSAL_STR("import")},
        {Token::CommentBegin, UNIVERSAL_STR("{#")},
        {Token::CommentEnd, UNIVERSAL_STR("#}")},
        {Token::StmtBegin, UNIVERSAL_STR("{%")},
        {Token::StmtEnd, UNIVERSAL_STR("%}")},
        {Token::ExprBegin, UNIVERSAL_STR("{{")},
        {Token::ExprEnd, UNIVERSAL_STR("}}")},
};

} // jinga2

#endif // TEMPLATE_PARSER_H
