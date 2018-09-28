#include "expression_evaluator.h"
#include "statements.h"
#include "template_impl.h"
#include "value_visitors.h"

#include <iostream>


namespace jinja2
{

void ForStatement::Render(OutStream& os, RenderContext& values)
{
    InternalValue loopVal = m_value->Evaluate(values);

    RenderLoop(loopVal, os, values);
}

void ForStatement::RenderLoop(const InternalValue& loopVal, OutStream& os, RenderContext& values)
{
    auto& context = values.EnterScope();

    InternalValueMap loopVar;
    context["loop"] = MapAdapter::CreateAdapter(&loopVar);
    if (m_isRecursive)
    {
        loopVar["operator()"] = Callable([this](const CallParams& params, OutStream& stream, RenderContext& context) {
                bool isSucceeded = false;
                auto parsedParams = helpers::ParseCallParams({{"var", true}}, params, isSucceeded);
                if (!isSucceeded)
                {
                    return;
                }

                auto var = parsedParams["var"];
                if (!var)
                {
                    return;
                }

                RenderLoop(var->Evaluate(context), stream, context);
            });
    }

    bool isConverted = false;
    auto loopItems = ConvertToList(loopVal, InternalValue(), isConverted);
    if (!isConverted)
    {
        if (m_elseBody)
            m_elseBody->Render(os, values);
        values.ExitScope();
        return;
    }

    if (m_ifExpr)
    {
        auto& tempContext = values.EnterScope();
        InternalValueList newLoopItems;
        for (auto& curValue : loopItems)
        {
            if (m_vars.size() > 1)
            {
                for (auto& varName : m_vars)
                    tempContext[varName] = Subscript(curValue, varName);
            }
            else
                tempContext[m_vars[0]] = curValue;

            if (ConvertToBool(m_ifExpr->Evaluate(values)))
                newLoopItems.push_back(curValue);
        }
        values.ExitScope();

        loopItems = ListAdapter::CreateAdapter(std::move(newLoopItems));
    }

    int64_t itemsNum = static_cast<int64_t>(loopItems.GetSize());
    loopVar["length"] = InternalValue(itemsNum);
    bool loopRendered = false;
    for (int64_t itemIdx = 0; itemIdx != itemsNum; ++ itemIdx)
    {
        loopRendered = true;
        loopVar["index"] = InternalValue(itemIdx + 1);
        loopVar["index0"] = InternalValue(itemIdx);
        loopVar["first"] = InternalValue(itemIdx == 0);
        loopVar["last"] = InternalValue(itemIdx == itemsNum - 1);
        if (itemIdx != 0)
            loopVar["previtem"] = loopItems.GetValueByIndex(static_cast<size_t>(itemIdx - 1));
        if (itemIdx != itemsNum - 1)
            loopVar["nextitem"] = loopItems.GetValueByIndex(static_cast<size_t>(itemIdx + 1));
        else
            loopVar.erase("nextitem");

        const auto& curValue = loopItems.GetValueByIndex(static_cast<size_t>(itemIdx));
        if (m_vars.size() > 1)
        {
            for (auto& varName : m_vars)
                context[varName] = Subscript(curValue, varName);
        }
        else
            context[m_vars[0]] = curValue;

        m_mainBody->Render(os, values);
    }

    if (!loopRendered && m_elseBody)
        m_elseBody->Render(os, values);

    values.ExitScope();
}

void IfStatement::Render(OutStream& os, RenderContext& values)
{
    InternalValue val = m_expr->Evaluate(values);
    bool isTrue = Apply<visitors::BooleanEvaluator>(val);

    if (isTrue)
    {
        m_mainBody->Render(os, values);
        return;
    }

    for (auto& b : m_elseBranches)
    {
        if (b->ShouldRender(values))
        {
            b->Render(os, values);
            break;
        }
    }
}

bool ElseBranchStatement::ShouldRender(RenderContext& values) const
{
    if (!m_expr)
        return true;

    return Apply<visitors::BooleanEvaluator>(m_expr->Evaluate(values));
}

void ElseBranchStatement::Render(OutStream& os, RenderContext& values)
{
    m_mainBody->Render(os, values);
}

void SetStatement::Render(OutStream&, RenderContext& values)
{
   if (m_expr)
   {
       InternalValue val = m_expr->Evaluate(values);
       if (m_fields.size() == 1)
           values.GetCurrentScope()[m_fields[0]] = val;
       else
       {
           for (auto& name : m_fields)
               values.GetCurrentScope()[name] = Subscript(val, name);
       }
   }
}

class BlocksRenderer : public RendererBase
{
public:
    virtual bool HasBlock(const std::string& blockName) = 0;
    virtual void RenderBlock(const std::string& blockName, OutStream& os, RenderContext& values) = 0;
};

void ParentBlockStatement::Render(OutStream& os, RenderContext& values)
{
    RenderContext innerContext = values.Clone(m_isScoped);
    bool found = false;
    auto parentTplVal = values.FindValue("$$__parent_template", found);
    if (!found)
        return;

    bool isConverted = false;
    auto parentTplsList = ConvertToList(parentTplVal->second, isConverted);
    if (!isConverted)
        return;

    BlocksRenderer* blockRenderer = nullptr; // static_cast<BlocksRenderer*>(*parentTplPtr);
    for (auto& tplVal : parentTplsList)
    {
        auto ptr = GetIf<RendererBase*>(&tplVal);
        if (!ptr)
            continue;

        auto parentTplPtr = static_cast<BlocksRenderer*>(*ptr);

        if (parentTplPtr->HasBlock(m_name))
        {
            blockRenderer = parentTplPtr;
            break;
        }
    }

    if (!blockRenderer)
        return;


    auto& scope = innerContext.EnterScope();
    scope["$$__super_block"] = static_cast<RendererBase*>(this);
    scope["super"] = Callable([this](const CallParams&, OutStream& stream, RenderContext& context) {
        m_mainBody->Render(stream, context);
    });
    if (!m_isScoped)
        scope["$$__parent_template"] = parentTplsList;

    blockRenderer->RenderBlock(m_name, os, innerContext);
    innerContext.ExitScope();

    auto& globalScope = values.GetGlobalScope();
    auto selfMap = GetIf<MapAdapter>(&globalScope[std::string("self")]);
    if (!selfMap->HasValue(m_name))
        selfMap->SetValue(m_name, MakeWrapped(Callable([this](const CallParams&, OutStream& stream, RenderContext& context) {
            Render(stream, context);
        })));
}

void BlockStatement::Render(OutStream& os, RenderContext& values)
{
    m_mainBody->Render(os, values);
}

template<typename CharT>
class ParentTemplateRenderer : public BlocksRenderer
{
public:
    ParentTemplateRenderer(std::shared_ptr<TemplateImpl<CharT>> tpl, ExtendsStatement::BlocksCollection* blocks)
        : m_template(tpl)
        , m_blocks(blocks)
    {
    }

    void Render(OutStream& os, RenderContext& values) override
    {
        auto& scope = values.GetCurrentScope();
        InternalValueList parentTemplates;
        parentTemplates.push_back(InternalValue(static_cast<RendererBase*>(this)));
        bool isFound = false;
        auto p = values.FindValue("$$__parent_template", isFound);
        if (isFound)
        {
            bool isConverted = false;
            auto prevTplsList = ConvertToList(p->second, isConverted);
            if (isConverted)
            {
                for (auto& tpl : prevTplsList)
                    parentTemplates.push_back(tpl);
            }
        }
        scope["$$__parent_template"] = ListAdapter::CreateAdapter(std::move(parentTemplates));
        m_template->GetRenderer()->Render(os, values);
    }

    void RenderBlock(const std::string& blockName, OutStream& os, RenderContext& values) override
    {
        auto p = m_blocks->find(blockName);
        if (p == m_blocks->end())
            return;

        p->second->Render(os, values);
    }

    bool HasBlock(const std::string &blockName) override
    {
        return m_blocks->count(blockName) != 0;
    }

private:
    std::shared_ptr<TemplateImpl<CharT>> m_template;
    ExtendsStatement::BlocksCollection* m_blocks;
};

struct TemplateImplVisitor
{
    ExtendsStatement::BlocksCollection* m_blocks;

    TemplateImplVisitor(ExtendsStatement::BlocksCollection* blocks)
        : m_blocks(blocks)
    {}

    template<typename CharT>
    RendererPtr operator()(nonstd::expected<std::shared_ptr<TemplateImpl<CharT>>, ErrorInfoTpl<CharT>> tpl) const
    {
        if (!tpl)
            return RendererPtr();
        return std::make_shared<ParentTemplateRenderer<CharT>>(tpl.value(), m_blocks);
    }

    RendererPtr operator()(EmptyValue) const
    {
        return RendererPtr();
    }
};

void ExtendsStatement::Render(OutStream& os, RenderContext& values)
{
    if (!m_isPath)
    {
        // FIXME: Implement processing of templates
        return;
    }
    auto tpl = values.GetRendererCallback()->LoadTemplate(m_templateName);
    auto renderer = visit(TemplateImplVisitor(&m_blocks), tpl);
    if (renderer)
        renderer->Render(os, values);
}

void MacroStatement::PrepareMacroParams(RenderContext& values)
{
    for (auto& p : m_params)
    {
        ArgumentInfo info(p.paramName, !p.defaultValue);
        if (p.defaultValue)
            info.defaultVal = p.defaultValue->Evaluate(values);
        m_preparedParams.push_back(std::move(info));
    }
}

void MacroStatement::Render(OutStream& os, RenderContext& values)
{
    PrepareMacroParams(values);

    values.GetCurrentScope()[m_name] = Callable([this](const CallParams& callParams, OutStream& stream, RenderContext& context) {
        InvokeMacroRenderer(callParams, stream, context);
    });
}

void MacroStatement::InvokeMacroRenderer(const CallParams& callParams, OutStream& stream, RenderContext& context)
{
    InternalValueMap callArgs;
    InternalValueMap kwArgs;
    InternalValueList varArgs;

    SetupCallArgs(m_preparedParams, callParams, context, callArgs, kwArgs, varArgs);
    InternalValueList arguments;
    InternalValueList defaults;
    for (auto& a : m_preparedParams)
    {
        arguments.emplace_back(a.name);
        defaults.emplace_back(a.defaultVal);
    }

    auto& scope = context.EnterScope();
    for (auto& a : callArgs)
        scope[a.first] = std::move(a.second);

    scope["kwargs"] = MapAdapter::CreateAdapter(std::move(kwArgs));
    scope["varargs"] = ListAdapter::CreateAdapter(std::move(varArgs));

    scope["name"] = static_cast<std::string>(m_name);
    scope["arguments"] = ListAdapter::CreateAdapter(std::move(arguments));
    scope["defaults"] = ListAdapter::CreateAdapter(std::move(defaults));

    m_mainBody->Render(stream, context);

    context.ExitScope();
}

void MacroStatement::SetupCallArgs(const std::vector<ArgumentInfo>& argsInfo, const CallParams& callParams, RenderContext& context, InternalValueMap& callArgs, InternalValueMap& kwArgs, InternalValueList& varArgs)
{
    bool isSucceeded = true;
    ParsedArguments args = helpers::ParseCallParams(argsInfo, callParams, isSucceeded);

    for (auto& a : args.args)
        callArgs[a.first] = a.second->Evaluate(context);

    for (auto& a : args.extraKwArgs)
        kwArgs[a.first] = a.second->Evaluate(context);

    for (auto& a : args.extraPosArgs)
        varArgs.push_back(a->Evaluate(context));
}

void MacroStatement::SetupMacroScope(InternalValueMap& scope)
{
    ;
}

void MacroCallStatement::Render(OutStream& os, RenderContext& values)
{
    bool isMacroFound = false;
    auto macroPtr = values.FindValue(m_macroName, isMacroFound);
    if (!isMacroFound)
        return;

    auto& fnVal = macroPtr->second;
    const Callable* callable = GetIf<Callable>(&fnVal);
    if (callable == nullptr || callable->GetType() == Callable::Type::Expression)
        return;

    PrepareMacroParams(values);
    auto& curScope = values.GetCurrentScope();
    auto callerP = curScope.find("caller");
    bool hasCallerVal = callerP != curScope.end();
    InternalValue prevCaller;
    if (hasCallerVal)
        prevCaller = callerP->second;

    curScope["caller"] = Callable([this](const CallParams& callParams, OutStream& stream, RenderContext& context) {
        InvokeMacroRenderer(callParams, stream, context);
    });

    callable->GetStatementCallable()(m_callParams, os, values);

    if (hasCallerVal)
        curScope["caller"] = prevCaller;
    else
        values.GetCurrentScope().erase("caller");
}

void MacroCallStatement::SetupMacroScope(InternalValueMap& scope)
{

}


} // jinja2
