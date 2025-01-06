/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Parser.h"

#include "AST.h"
#include "Lexer.h"
#include "ParserPrivate.h"
#include "WGSLShaderModule.h"

#include <wtf/Deque.h>
#include <wtf/HashSet.h>
#include <wtf/SetForScope.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WGSL {

template<TokenType TT, TokenType... TTs>
struct TemplateTypes {
    static bool includes(TokenType tokenType)
    {
        return TT == tokenType || TemplateTypes<TTs...>::includes(tokenType);
    }

    static void appendNameTo(StringBuilder& builder)
    {
        builder.append(toString(TT), ", "_s);
        TemplateTypes<TTs...>::appendNameTo(builder);
    }
};

template <TokenType TT>
struct TemplateTypes<TT> {
    static bool includes(TokenType tokenType)
    {
        return TT == tokenType;
    }

    static void appendNameTo(StringBuilder& builder)
    {
        builder.append(toString(TT));
    }
};

#define START_PARSE() \
    auto _startOfElementPosition = m_currentPosition;

#define CURRENT_SOURCE_SPAN() \
    SourceSpan(_startOfElementPosition, m_currentPosition)

#define MAKE_ARENA_NODE(type, ...) \
    m_builder.construct<AST::type>(CURRENT_SOURCE_SPAN() __VA_OPT__(,) __VA_ARGS__) /* NOLINT */

#define RETURN_ARENA_NODE(type, ...) \
    return { MAKE_ARENA_NODE(type __VA_OPT__(,) __VA_ARGS__) }; /* NOLINT */

#define FAIL(string) \
    return makeUnexpected(Error(string, CURRENT_SOURCE_SPAN()));

// Warning: cannot use the do..while trick because it defines a new identifier named `name`.
// So do not use after an if/for/while without braces.
#define PARSE(name, element, ...) \
    auto name##Expected = parse##element(__VA_ARGS__); \
    if (!name##Expected) \
        return makeUnexpected(name##Expected.error()); \
    auto& name = *name##Expected;

#define PARSE_MOVE(name, element, ...) \
    auto name##Expected = parse##element(__VA_ARGS__); \
    if (!name##Expected) \
        return makeUnexpected(name##Expected.error()); \
    name = WTFMove(*name##Expected);

// Warning: cannot use the do..while trick because it defines a new identifier named `name`.
// So do not use after an if/for/while without braces.
#define CONSUME_TYPE_NAMED(name, type) \
    auto name##Expected = consumeType(TokenType::type); \
    if (!name##Expected) { \
        auto error = makeString("Expected a "_s, \
            toString(TokenType::type), \
            ", but got a "_s, \
            toString(name##Expected.error())); \
        FAIL(WTFMove(error)); \
    } \
    auto& name = *name##Expected;

#define CONSUME_TYPE(type) \
    do { \
        auto expectedToken = consumeType(TokenType::type); \
        if (!expectedToken) { \
            auto error = makeString("Expected a "_s, \
                toString(TokenType::type), \
                ", but got a "_s, \
                toString(expectedToken.error())); \
            FAIL(WTFMove(error)); \
        } \
    } while (false)

#define CONSUME_TYPES_NAMED(name, ...) \
    auto name##Expected = consumeTypes<__VA_ARGS__>(); \
    if (!name##Expected) { \
        StringBuilder builder; \
        builder.append("Expected one of ["_s); \
        TemplateTypes<__VA_ARGS__>::appendNameTo(builder); \
        builder.append("], but got a "_s, toString(name##Expected.error())); \
        FAIL(builder.toString()); \
    } \
    auto& name = *name##Expected;

#define CHECK_RECURSION() \
    SetForScope __parseDepth(m_parseDepth, m_parseDepth + 1); \
    if (m_parseDepth > 128) \
        FAIL("maximum parser recursive depth reached"_s);

static bool canBeginUnaryExpression(const Token& token)
{
    switch (token.type) {
    case TokenType::And:
    case TokenType::Tilde:
    case TokenType::Star:
    case TokenType::Minus:
    case TokenType::Bang:
        return true;
    default:
        return false;
    }
}

static bool canContinueMultiplicativeExpression(const Token& token)
{
    switch (token.type) {
    case TokenType::Modulo:
    case TokenType::Slash:
    case TokenType::Star:
        return true;
    default:
        return false;
    }
}

static bool canContinueAdditiveExpression(const Token& token)
{
    switch (token.type) {
    case TokenType::Minus:
    case TokenType::Plus:
        return true;
    default:
        return canContinueMultiplicativeExpression(token);
    }
}

static bool canContinueBitwiseExpression(const Token& token)
{
    switch (token.type) {
    case TokenType::And:
    case TokenType::Or:
    case TokenType::Xor:
        return true;
    default:
        return false;
    }
}

static bool canContinueRelationalExpression(const Token& token)
{
    switch (token.type) {
    case TokenType::Gt:
    case TokenType::GtEq:
    case TokenType::Lt:
    case TokenType::LtEq:
    case TokenType::EqEq:
    case TokenType::BangEq:
        return true;
    default:
        return false;
    }
}

static bool canContinueShortCircuitAndExpression(const Token& token)
{
    return token.type == TokenType::AndAnd;
}

static bool canContinueShortCircuitOrExpression(const Token& token)
{
    return token.type == TokenType::OrOr;
}

static bool canContinueCompoundAssignmentStatement(const Token& token)
{
    switch (token.type) {
    case TokenType::PlusEq:
    case TokenType::MinusEq:
    case TokenType::StarEq:
    case TokenType::SlashEq:
    case TokenType::ModuloEq:
    case TokenType::AndEq:
    case TokenType::OrEq:
    case TokenType::XorEq:
    case TokenType::GtGtEq:
    case TokenType::LtLtEq:
        return true;
    default:
        return false;
    }
}

static AST::BinaryOperation toBinaryOperation(const Token& token)
{
    switch (token.type) {
    case TokenType::And:
    case TokenType::AndEq:
        return AST::BinaryOperation::And;
    case TokenType::AndAnd:
        return AST::BinaryOperation::ShortCircuitAnd;
    case TokenType::BangEq:
        return AST::BinaryOperation::NotEqual;
    case TokenType::EqEq:
        return AST::BinaryOperation::Equal;
    case TokenType::Gt:
        return AST::BinaryOperation::GreaterThan;
    case TokenType::GtEq:
        return AST::BinaryOperation::GreaterEqual;
    case TokenType::GtGt:
    case TokenType::GtGtEq:
        return AST::BinaryOperation::RightShift;
    case TokenType::Lt:
        return AST::BinaryOperation::LessThan;
    case TokenType::LtEq:
        return AST::BinaryOperation::LessEqual;
    case TokenType::LtLt:
    case TokenType::LtLtEq:
        return AST::BinaryOperation::LeftShift;
    case TokenType::Minus:
    case TokenType::MinusEq:
        return AST::BinaryOperation::Subtract;
    case TokenType::Modulo:
    case TokenType::ModuloEq:
        return AST::BinaryOperation::Modulo;
    case TokenType::Or:
    case TokenType::OrEq:
        return AST::BinaryOperation::Or;
    case TokenType::OrOr:
        return AST::BinaryOperation::ShortCircuitOr;
    case TokenType::Plus:
    case TokenType::PlusEq:
        return AST::BinaryOperation::Add;
    case TokenType::Slash:
    case TokenType::SlashEq:
        return AST::BinaryOperation::Divide;
    case TokenType::Star:
    case TokenType::StarEq:
        return AST::BinaryOperation::Multiply;
    case TokenType::Xor:
    case TokenType::XorEq:
        return AST::BinaryOperation::Xor;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static AST::UnaryOperation toUnaryOperation(const Token& token)
{
    switch (token.type) {
    case TokenType::And:
        return AST::UnaryOperation::AddressOf;
    case TokenType::Tilde:
        return AST::UnaryOperation::Complement;
    case TokenType::Star:
        return AST::UnaryOperation::Dereference;
    case TokenType::Minus:
        return AST::UnaryOperation::Negate;
    case TokenType::Bang:
        return AST::UnaryOperation::Not;

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

template<typename Lexer>
std::optional<FailedCheck> parse(ShaderModule& shaderModule)
{
    Lexer lexer(shaderModule.source());
    Parser parser(shaderModule, lexer);
    auto result = parser.parseShader();
    if (!result.has_value())
        return FailedCheck { { result.error() }, { /* warnings */ } };
    return std::nullopt;
}

std::optional<FailedCheck> parse(ShaderModule& shaderModule)
{
    if (shaderModule.source().is8Bit())
        return parse<Lexer<LChar>>(shaderModule);
    return parse<Lexer<UChar>>(shaderModule);
}

template<typename Lexer>
Expected<Token, TokenType> Parser<Lexer>::consumeType(TokenType type)
{
    if (current().type == type) {
        Expected<Token, TokenType> result = { m_current };
        consume();
        return result;
    }
    return makeUnexpected(current().type);
}

template<typename Lexer>
template<TokenType... TTs>
Expected<Token, TokenType> Parser<Lexer>::consumeTypes()
{
    auto token = m_current;
    if (TemplateTypes<TTs...>::includes(token.type)) {
        consume();
        return { token };
    }
    return makeUnexpected(token.type);
}

template<typename Lexer>
void Parser<Lexer>::consume()
{
    do {
        m_current = m_tokens[++m_currentTokenIndex];
        m_currentPosition = SourcePosition { m_current.span.line, m_current.span.lineOffset, m_current.span.offset };
    } while (m_current.type == TokenType::Placeholder);
}

template<typename Lexer>
Result<void> Parser<Lexer>::parseShader()
{
    START_PARSE();
    disambiguateTemplates();
    while (current().type != TokenType::EndOfFile) {
        switch (current().type) {
        case TokenType::KeywordEnable:
            if (auto result = parseEnableDirective(); !result)
                return makeUnexpected(result.error());
            break;
        case TokenType::KeywordRequires:
            if (auto result = parseRequireDirective(); !result)
                return makeUnexpected(result.error());
            break;
        case TokenType::KeywordDiagnostic: {
            consume();
            PARSE(diagnostic, Diagnostic);
            CONSUME_TYPE(Semicolon);
            auto& directive = MAKE_ARENA_NODE(DiagnosticDirective, WTFMove(diagnostic));
            m_shaderModule.directives().append(directive);
            break;
        }
        default:
            goto declarations;
        }
    }

    declarations:
    while (current().type != TokenType::EndOfFile) {
        if (current().type == TokenType::Semicolon) {
            consume();
            continue;
        }

        PARSE(declaration, Declaration);
        m_shaderModule.declarations().append(WTFMove(declaration));
    }

    return { };
}

template<typename Lexer>
Result<void> Parser<Lexer>::parseEnableDirective()
{
    START_PARSE();
    CONSUME_TYPE(KeywordEnable);
    do {
        CONSUME_TYPE_NAMED(identifier, Identifier);
        auto* extension = parseExtension(identifier.ident);
        if (!extension)
            FAIL("Expected 'f16'"_s);
        m_shaderModule.enabledExtensions().add(*extension);

        if (current().type != TokenType::Comma)
            break;
        CONSUME_TYPE(Comma);
    } while (current().type != TokenType::Semicolon);
    CONSUME_TYPE(Semicolon);
    return { };
}

template<typename Lexer>
Result<void> Parser<Lexer>::parseRequireDirective()
{
    START_PARSE();
    CONSUME_TYPE(KeywordRequires);
    do {
        CONSUME_TYPE_NAMED(identifier, Identifier);
        auto* languageFeature = parseLanguageFeature(identifier.ident);
        if (!languageFeature)
            FAIL("Expected 'readonly_and_readwrite_storage_textures', 'packed_4x8_integer_dot_product', 'unrestricted_pointer_parameters' or 'pointer_composite_access'"_s);
        m_shaderModule.requiredFeatures().add(*languageFeature);

        if (current().type != TokenType::Comma)
            break;
        CONSUME_TYPE(Comma);
    } while (current().type != TokenType::Semicolon);
    CONSUME_TYPE(Semicolon);
    return { };
}

template<typename Lexer>
void Parser<Lexer>::maybeSplitToken(unsigned index)
{
    TokenType replacement;
    switch (m_tokens[index + 0].type) {
    case TokenType::GtGt:
        replacement = TokenType::Gt;
        break;
    case TokenType::GtEq:
        replacement = TokenType::Equal;
        break;
    case TokenType::GtGtEq:
        replacement = TokenType::GtEq;
        break;
    default:
        return;
    }

    ASSERT(m_tokens[index + 1].type == TokenType::Placeholder);
    m_tokens[index + 0].type = TokenType::Gt;
    m_tokens[index + 1].type = replacement;
}

template<typename Lexer>
void Parser<Lexer>::disambiguateTemplates()
{
    // Reference algorithm: https://github.com/gpuweb/gpuweb/issues/3770
    const size_t count = m_tokens.size();

    // The current expression nesting depth.
    // Each '(', '[' increments the depth.
    // Each ')', ']' decrements the depth.
    unsigned expressionDepth = 0;

    // A stack of '<' tokens.
    // Used to pair '<' and '>' tokens at the same expression depth.
    struct StackEntry {
        Token* token; // A pointer to the opening '<' token
        unsigned expressionDepth; // The value of 'expr_depth' for the opening '<'
    };
    Deque<StackEntry, 16> stack;

    for (size_t i = 0; i < count - 1; i++) {
        switch (m_tokens[i].type) {
        case TokenType::Identifier:
        case TokenType::KeywordVar: {
            // Potential start to a template argument list.
            // Push the address-of '<' to the stack, along with the current nesting expr_depth.
            auto& next = m_tokens[i + 1];
            if (next.type == TokenType::Lt) {
                stack.append(StackEntry { &m_tokens[i + 1], expressionDepth });
                i++;
            }
            break;
        }
        case TokenType::Gt:
        case TokenType::GtGt:
        case TokenType::GtEq:
        case TokenType::GtGtEq:
            // Note: Depending on your lexer - you may need split '>>', '>=', '>>='.
            // MaybeSplitToken() splits the left-most '>' from the token, updating 'tokens'
            // and returning the first token ('>').
            // If the token is not splittable, then MaybeSplitToken() simply returns 'token'.
            // As 'tokens' is updated with the split tokens, '>>=' may split to ['>', '>='], then
            // on the next iteration again to ['>', '='].
            if (!stack.isEmpty() && stack.last().expressionDepth == expressionDepth) {
                maybeSplitToken(i);
                stack.takeLast().token->type = TokenType::TemplateArgsLeft;
                m_tokens[i].type = TokenType::TemplateArgsRight;
            }
            break;

        case TokenType::ParenLeft:
        case TokenType::BracketLeft:
            // Entering a nested expression.
            expressionDepth++;
            break;

        case TokenType::ParenRight:
        case TokenType::BracketRight:
            // Exiting a nested expression
            // Pop the stack until we return to the current expression expr_depth
            while (!stack.isEmpty() && stack.last().expressionDepth == expressionDepth)
                stack.removeLast();
            if (expressionDepth > 0)
                expressionDepth--;
            break;

        case TokenType::Semicolon:
        case TokenType::BraceLeft:
        case TokenType::Equal:
        case TokenType::Colon:
            // Expression terminating tokens (non-exhaustive).
            // No opening template list can continue across these tokens, so clear
            // the stack and expression depth.
            expressionDepth = 0;
            stack.clear();
            break;

        case TokenType::OrOr:
        case TokenType::AndAnd:
            // Exception tokens for template argument lists.
            // Treat 'a < b || c > d' as a logical binary operator of two comparison operators
            // instead of a single template argument 'b||c'.
            // Requires parentheses around 'b||c' to parse as a template argument list.
            while (!stack.isEmpty() && stack.last().expressionDepth == expressionDepth)
                stack.removeLast();
            break;

        default:
            break;
        }
    }
}

template<typename Lexer>
Result<AST::Identifier> Parser<Lexer>::parseIdentifier()
{
    START_PARSE();

    CONSUME_TYPE_NAMED(name, Identifier);

    return AST::Identifier::makeWithSpan(CURRENT_SOURCE_SPAN(), WTFMove(name.ident));
}

template<typename Lexer>
Result<AST::Declaration::Ref> Parser<Lexer>::parseDeclaration()
{
    START_PARSE();

    if (current().type == TokenType::KeywordConst) {
        PARSE(variable, Variable);
        CONSUME_TYPE(Semicolon);
        return { variable };
    } else if (current().type == TokenType::KeywordAlias) {
        PARSE(alias, TypeAlias);
        return { alias };
    } else if (current().type ==  TokenType::KeywordConstAssert) {
        PARSE(assert, ConstAssert);
        return { assert };
    }

    PARSE(attributes, Attributes);

    switch (current().type) {
    case TokenType::KeywordStruct: {
        PARSE(structure, Structure, WTFMove(attributes));
        return { structure };
    }
    case TokenType::KeywordOverride:
    case TokenType::KeywordVar: {
        PARSE(variable, VariableWithAttributes, WTFMove(attributes));
        CONSUME_TYPE(Semicolon);
        return { variable };
    }
    case TokenType::KeywordFn: {
        PARSE(function, Function, WTFMove(attributes));
        return { function };
    }
    default:
        FAIL("Trying to parse a GlobalDecl, expected 'const', 'fn', 'override', 'struct' or 'var'."_s);
    }
}

template<typename Lexer>
Result<AST::ConstAssert::Ref> Parser<Lexer>::parseConstAssert()
{
    START_PARSE();
    CONSUME_TYPE(KeywordConstAssert);
    PARSE(test, Expression);
    CONSUME_TYPE(Semicolon);
    RETURN_ARENA_NODE(ConstAssert, WTFMove(test));
}

template<typename Lexer>
Result<AST::Attribute::List> Parser<Lexer>::parseAttributes()
{
    AST::Attribute::List attributes;

    while (current().type == TokenType::Attribute) {
        PARSE(firstAttribute, Attribute);
        attributes.append(WTFMove(firstAttribute));
    }

    return { WTFMove(attributes) };
}

template<typename Lexer>
Result<AST::Attribute::Ref> Parser<Lexer>::parseAttribute()
{
    START_PARSE();

    CONSUME_TYPE(Attribute);

    if (current().type == TokenType::KeywordDiagnostic) {
        consume();
        PARSE(diagnostic, Diagnostic);
        RETURN_ARENA_NODE(DiagnosticAttribute, WTFMove(diagnostic));
    }


    CONSUME_TYPE_NAMED(ident, Identifier);

    if (ident.ident == "group"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(group, Expression);
        if (current().type  == TokenType::Comma)
            consume();
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(GroupAttribute, WTFMove(group));
    }

    if (ident.ident == "binding"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(binding, Expression);
        if (current().type  == TokenType::Comma)
            consume();
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(BindingAttribute, WTFMove(binding));
    }

    if (ident.ident == "location"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(location, Expression);
        if (current().type  == TokenType::Comma)
            consume();
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(LocationAttribute, WTFMove(location));
    }

    if (ident.ident == "builtin"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(name, Identifier);
        auto* builtin = parseBuiltin(name);
        if (!builtin)
            FAIL("Unknown builtin value. Expected 'vertex_index', 'instance_index', 'position', 'front_facing', 'frag_depth', 'sample_index', 'sample_mask', 'local_invocation_id', 'local_invocation_index', 'global_invocation_id', 'workgroup_id' or 'num_workgroups'"_s);
        switch (*builtin) {
        case Builtin::FragDepth:
            m_shaderModule.setUsesFragDepth();
            break;
        case Builtin::SampleMask:
            m_shaderModule.setUsesSampleMask();
            break;
        case Builtin::SampleIndex:
            m_shaderModule.setUsesSampleIndex();
            break;
        case Builtin::FrontFacing:
            m_shaderModule.setUsesFrontFacing();
            break;
        default:
            break;
        }

        if (current().type  == TokenType::Comma)
            consume();
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(BuiltinAttribute, *builtin);
    }

    if (ident.ident == "workgroup_size"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(x, Expression);
        AST::Expression::Ptr maybeY = nullptr;
        AST::Expression::Ptr maybeZ = nullptr;
        if (current().type == TokenType::Comma) {
            consume();
            if (current().type != TokenType::ParenRight) {
                PARSE(y, Expression);
                maybeY = &y.get();

                if (current().type == TokenType::Comma) {
                    consume();
                    if (current().type != TokenType::ParenRight) {
                        PARSE(z, Expression);
                        maybeZ = &z.get();

                        if (current().type == TokenType::Comma)
                            consume();
                    }
                }
            }
        }
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(WorkgroupSizeAttribute, WTFMove(x), WTFMove(maybeY), WTFMove(maybeZ));
    }

    if (ident.ident == "align"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(alignment, Expression);
        if (current().type  == TokenType::Comma)
            consume();
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(AlignAttribute, WTFMove(alignment));
    }

    if (ident.ident == "interpolate"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(interpolate, Identifier);
        auto* interpolationType = parseInterpolationType(interpolate);
        if (!interpolationType)
            FAIL("Unknown interpolation type. Expected 'flat', 'linear' or 'perspective'"_s);
        InterpolationSampling sampleType { InterpolationSampling::Center };
        if (current().type == TokenType::Comma) {
            consume();
            PARSE(sampling, Identifier);
            auto* interpolationSampling = parseInterpolationSampling(sampling);
            if (!interpolationSampling)
                FAIL("Unknown interpolation sampling. Expected 'center', 'centroid', 'sample', 'first' or 'either"_s);
            sampleType = *interpolationSampling;
        }
        if (current().type == TokenType::Comma)
            consume();
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(InterpolateAttribute, *interpolationType, sampleType);
    }

    if (ident.ident == "size"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(size, Expression);
        if (current().type  == TokenType::Comma)
            consume();
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(SizeAttribute, WTFMove(size));
    }

    if (ident.ident == "id"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(size, Expression);
        if (current().type  == TokenType::Comma)
            consume();
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(IdAttribute, WTFMove(size));
    }

    if (ident.ident == "invariant"_s)
        RETURN_ARENA_NODE(InvariantAttribute);

    if (ident.ident == "must_use"_s)
        RETURN_ARENA_NODE(MustUseAttribute);

    if (ident.ident == "const"_s)
        RETURN_ARENA_NODE(ConstAttribute);

    // https://gpuweb.github.io/gpuweb/wgsl/#pipeline-stage-attributes
    if (ident.ident == "vertex"_s)
        RETURN_ARENA_NODE(StageAttribute, ShaderStage::Vertex);
    if (ident.ident == "compute"_s)
        RETURN_ARENA_NODE(StageAttribute, ShaderStage::Compute);
    if (ident.ident == "fragment"_s)
        RETURN_ARENA_NODE(StageAttribute, ShaderStage::Fragment);

    FAIL("Unknown attribute. Supported attributes are 'align', 'binding', 'builtin', 'compute', 'const', 'diagnostic', 'fragment', 'group', 'id', 'interpolate', 'invariant', 'location', 'must_use', 'size', 'vertex', 'workgroup_size'."_s);
}

template<typename Lexer>
Result<AST::Diagnostic> Parser<Lexer>::parseDiagnostic()
{
    START_PARSE();
    CONSUME_TYPE(ParenLeft);
    PARSE(severity, Identifier);
    auto* severityControl = parseSeverityControl(severity);
    if (!severityControl)
        FAIL("Unknown severity control. Expected 'error', 'info', 'off' or 'warning'"_s);
    CONSUME_TYPE(Comma);

    PARSE(name, Identifier);
    std::optional<AST::Identifier> suffix;
    if (current().type == TokenType::Period) {
        consume();
        PARSE(suffix, Identifier);
        suffix = WTFMove(suffix);
    }
    if (current().type == TokenType::Comma)
        consume();
    CONSUME_TYPE(ParenRight);
    return AST::Diagnostic { *severityControl, AST::TriggeringRule { WTFMove(name), WTFMove(suffix) } };
}

template<typename Lexer>
Result<AST::Structure::Ref> Parser<Lexer>::parseStructure(AST::Attribute::List&& attributes)
{
    START_PARSE();

    CONSUME_TYPE(KeywordStruct);
    PARSE(name, Identifier);
    CONSUME_TYPE(BraceLeft);

    AST::StructureMember::List members;
    HashSet<String> seenMembers;
    while (current().type != TokenType::BraceRight) {
        PARSE(member, StructureMember);
        auto result = seenMembers.add(member.get().name());
        if (!result.isNewEntry)
            FAIL(makeString("duplicate member '"_s, member.get().name(), "' in struct '"_s, name, '\''));
        members.append(member);

        // https://www.w3.org/TR/WGSL/#limits
        static constexpr unsigned maximumNumberOfStructMembers = 1023;
        if (UNLIKELY(members.size() > maximumNumberOfStructMembers))
            FAIL(makeString("struct cannot have more than "_s, String::number(maximumNumberOfStructMembers), " members"_s));

        if (current().type == TokenType::Comma)
            consume();
        else
            break;
    }

    if (members.isEmpty())
        FAIL("structures must have at least one member"_str);

    CONSUME_TYPE(BraceRight);

    RETURN_ARENA_NODE(Structure, WTFMove(name), WTFMove(members), WTFMove(attributes), AST::StructureRole::UserDefined);
}

template<typename Lexer>
Result<std::reference_wrapper<AST::StructureMember>> Parser<Lexer>::parseStructureMember()
{
    START_PARSE();

    PARSE(attributes, Attributes);
    PARSE(name, Identifier);
    CONSUME_TYPE(Colon);
    PARSE(type, TypeName);

    RETURN_ARENA_NODE(StructureMember, WTFMove(name), WTFMove(type), WTFMove(attributes));
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseTypeName()
{
    START_PARSE();

    auto scope = SetForScope(m_compositeTypeDepth, m_compositeTypeDepth + 1);
    //
    // https://www.w3.org/TR/WGSL/#limits
    static constexpr unsigned maximumCompositeTypeNestingDepth = 15;
    if (UNLIKELY(m_compositeTypeDepth > maximumCompositeTypeNestingDepth))
        FAIL(makeString("composite type may not be nested more than "_s, String::number(maximumCompositeTypeNestingDepth), " levels"_s));

    if (current().type == TokenType::Identifier) {
        PARSE(name, Identifier);
        // FIXME: remove the special case for array
        if (name == "array"_s)
            return parseArrayType();
        return parseTypeNameAfterIdentifier(WTFMove(name), _startOfElementPosition);
    }

    FAIL("Tried parsing a type and it did not start with an identifier"_s);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseTypeNameAfterIdentifier(AST::Identifier&& name, SourcePosition _startOfElementPosition) // NOLINT
{
    if (current().type == TokenType::TemplateArgsLeft) {
        CONSUME_TYPE(TemplateArgsLeft);
        AST::Expression::List arguments;
        do {
            PARSE(elementType, TypeName);
            arguments.append(WTFMove(elementType));
            if (current().type != TokenType::Comma)
                break;
            CONSUME_TYPE(Comma);
        } while (current().type != TokenType::TemplateArgsRight);
        CONSUME_TYPE(TemplateArgsRight);
        RETURN_ARENA_NODE(ElaboratedTypeExpression, WTFMove(name), WTFMove(arguments));
    }
    RETURN_ARENA_NODE(IdentifierExpression, WTFMove(name));
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseArrayType()
{
    START_PARSE();

    AST::Expression::Ptr maybeElementType = nullptr;
    AST::Expression::Ptr maybeElementCount = nullptr;

    if (current().type == TokenType::TemplateArgsLeft) {
        // We differ from the WGSL grammar here by allowing the type to be optional,
        // which allows us to use `parseArrayType` in `parseCallExpression`.
        consume();

        PARSE(elementType, TypeName);
        maybeElementType = &elementType.get();

        if (current().type == TokenType::Comma) {
            consume();
            // FIXME: According to https://www.w3.org/TR/WGSL/#syntax-element_count_expression
            // this should be: AdditiveExpression | BitwiseExpression.
            //
            // The WGSL grammar doesn't specify expression operator precedence so
            // until then just parse AdditiveExpression.
            if (current().type != TokenType::TemplateArgsRight) {
                PARSE(elementCountLHS, UnaryExpression);
                PARSE(elementCount, AdditiveExpressionPostUnary, WTFMove(elementCountLHS));
                maybeElementCount = &elementCount.get();

                if (current().type == TokenType::Comma)
                    consume();
            }
        }
        CONSUME_TYPE(TemplateArgsRight);
    }
    RETURN_ARENA_NODE(ArrayTypeExpression, maybeElementType, maybeElementCount);
}

template<typename Lexer>
Result<AST::Variable::Ref> Parser<Lexer>::parseVariable()
{
    return parseVariableWithAttributes(AST::Attribute::List { });
}

template<typename Lexer>
Result<AST::Variable::Ref> Parser<Lexer>::parseVariableWithAttributes(AST::Attribute::List&& attributes)
{
    auto flavor = [](const Token& token) -> AST::VariableFlavor {
        switch (token.type) {
        case TokenType::KeywordConst:
            return AST::VariableFlavor::Const;
        case TokenType::KeywordLet:
            return AST::VariableFlavor::Let;
        case TokenType::KeywordOverride:
            return AST::VariableFlavor::Override;
        default:
            ASSERT(token.type == TokenType::KeywordVar);
            return AST::VariableFlavor::Var;
        }
    };

    START_PARSE();

    CONSUME_TYPES_NAMED(varKind,
        TokenType::KeywordConst,
        TokenType::KeywordOverride,
        TokenType::KeywordLet,
        TokenType::KeywordVar);

    auto varFlavor = flavor(varKind);

    AST::VariableQualifier::Ptr maybeQualifier = nullptr;
    if (current().type == TokenType::TemplateArgsLeft) {
        PARSE(variableQualifier, VariableQualifier);
        maybeQualifier = &variableQualifier.get();
    }

    PARSE(name, Identifier);

    AST::Expression::Ptr maybeType = nullptr;
    if (current().type == TokenType::Colon) {
        consume();
        PARSE(typeName, TypeName);
        maybeType = &typeName.get();
    }

    AST::Expression::Ptr maybeInitializer = nullptr;
    if (varFlavor == AST::VariableFlavor::Const || varFlavor == AST::VariableFlavor::Let || current().type == TokenType::Equal) {
        CONSUME_TYPE(Equal);
        PARSE(initializerExpr, Expression);
        maybeInitializer = &initializerExpr.get();
    }

    if (!maybeType && !maybeInitializer) {
        ASCIILiteral flavor = [&] {
            switch (varFlavor) {
            case AST::VariableFlavor::Const:
                RELEASE_ASSERT_NOT_REACHED();
            case AST::VariableFlavor::Let:
                RELEASE_ASSERT_NOT_REACHED();
            case AST::VariableFlavor::Override:
                return "override"_s;
            case AST::VariableFlavor::Var:
                return "var"_s;
            }
        }();
        FAIL(makeString(flavor, " declaration requires a type or initializer"_s));
    }

    RETURN_ARENA_NODE(Variable, varFlavor, WTFMove(name), WTFMove(maybeQualifier), WTFMove(maybeType), WTFMove(maybeInitializer), WTFMove(attributes));
}

template<typename Lexer>
Result<AST::VariableQualifier::Ref> Parser<Lexer>::parseVariableQualifier()
{
    START_PARSE();

    CONSUME_TYPE(TemplateArgsLeft);
    PARSE(addressSpace, AddressSpace);

    AccessMode accessMode;
    if (current().type == TokenType::Comma) {
        if (addressSpace != AddressSpace::Storage)
            FAIL("only variables in the <storage> address space may specify an access mode"_s);

        consume();
        PARSE(actualAccessMode, AccessMode);
        accessMode = actualAccessMode;
    } else
        accessMode = defaultAccessModeForAddressSpace(addressSpace);

    CONSUME_TYPE(TemplateArgsRight);
    RETURN_ARENA_NODE(VariableQualifier, addressSpace, accessMode);
}

template<typename Lexer>
Result<AddressSpace> Parser<Lexer>::parseAddressSpace()
{
    START_PARSE();

    CONSUME_TYPE_NAMED(identifier, Identifier);
    if (auto* addressSpace = WGSL::parseAddressSpace(identifier.ident); addressSpace && *addressSpace != AddressSpace::Handle)
        return { *addressSpace };

    FAIL("Expected one of 'function'/'private'/'storage'/'uniform'/'workgroup'"_s);
}

template<typename Lexer>
Result<AccessMode> Parser<Lexer>::parseAccessMode()
{
    START_PARSE();

    CONSUME_TYPE_NAMED(identifier, Identifier);
    if (auto* accessMode = WGSL::parseAccessMode(identifier.ident))
        return { *accessMode };

    FAIL("Expected one of 'read'/'write'/'read_write'"_s);
}

template<typename Lexer>
Result<AST::TypeAlias::Ref> Parser<Lexer>::parseTypeAlias()
{
    START_PARSE();

    CONSUME_TYPE(KeywordAlias);
    PARSE(name, Identifier);

    CONSUME_TYPE(Equal);

    PARSE(type, TypeName);

    CONSUME_TYPE(Semicolon);

    RETURN_ARENA_NODE(TypeAlias, WTFMove(name), WTFMove(type));
}

template<typename Lexer>
Result<AST::Function::Ref> Parser<Lexer>::parseFunction(AST::Attribute::List&& attributes)
{
    START_PARSE();

    CONSUME_TYPE(KeywordFn);
    PARSE(name, Identifier);

    CONSUME_TYPE(ParenLeft);
    AST::Parameter::List parameters;
    while (current().type != TokenType::ParenRight) {
        PARSE(parameter, Parameter);
        parameters.append(WTFMove(parameter));

        // https://www.w3.org/TR/WGSL/#limits
        static constexpr unsigned maximumNumberOfFunctionParameters = 255;
        if (UNLIKELY(parameters.size() > maximumNumberOfFunctionParameters))
            FAIL(makeString("function cannot have more than "_s, String::number(maximumNumberOfFunctionParameters), " parameters"_s));

        if (current().type == TokenType::Comma)
            consume();
        else
            break;
    }
    CONSUME_TYPE(ParenRight);

    AST::Attribute::List returnAttributes;
    AST::Expression::Ptr maybeReturnType = nullptr;
    if (current().type == TokenType::Arrow) {
        consume();
        PARSE(parsedReturnAttributes, Attributes);
        returnAttributes = WTFMove(parsedReturnAttributes);
        PARSE(type, TypeName);
        maybeReturnType = &type.get();
    }

    PARSE(body, CompoundStatement);

    RETURN_ARENA_NODE(Function, WTFMove(name), WTFMove(parameters), WTFMove(maybeReturnType), WTFMove(body), WTFMove(attributes), WTFMove(returnAttributes));
}

template<typename Lexer>
Result<std::reference_wrapper<AST::Parameter>> Parser<Lexer>::parseParameter()
{
    START_PARSE();

    PARSE(attributes, Attributes);
    PARSE(name, Identifier)
    CONSUME_TYPE(Colon);
    PARSE(type, TypeName);

    RETURN_ARENA_NODE(Parameter, WTFMove(name), WTFMove(type), WTFMove(attributes), AST::ParameterRole::UserDefined);
}

template<typename Lexer>
Result<AST::Statement::Ref> Parser<Lexer>::parseStatement()
{
    START_PARSE();
    CHECK_RECURSION();

    switch (current().type) {
    case TokenType::BraceLeft: {
        PARSE(compoundStmt, CompoundStatement);
        return { compoundStmt };
    }
    case TokenType::KeywordIf: {
        // FIXME: Handle attributes attached to statement.
        return parseIfStatement();
    }
    case TokenType::KeywordReturn: {
        PARSE(returnStmt, ReturnStatement);
        CONSUME_TYPE(Semicolon);
        return { returnStmt };
    }
    case TokenType::KeywordConst:
    case TokenType::KeywordLet:
    case TokenType::KeywordVar: {
        PARSE(variable, Variable);
        CONSUME_TYPE(Semicolon);
        RETURN_ARENA_NODE(VariableStatement, WTFMove(variable));
    }
    case TokenType::Identifier: {
        PARSE(ident, Identifier);

        if (current().type == TokenType::TemplateArgsLeft || current().type == TokenType::ParenLeft) {
            PARSE(type, TypeNameAfterIdentifier, WTFMove(ident), _startOfElementPosition);
            PARSE(arguments, ArgumentExpressionList);
            auto& call = MAKE_ARENA_NODE(CallExpression, WTFMove(type), WTFMove(arguments));
            CONSUME_TYPE(Semicolon);
            RETURN_ARENA_NODE(CallStatement, call);
        }

        AST::Expression::Ref identifierExpression = MAKE_ARENA_NODE(IdentifierExpression, WTFMove(ident));
        PARSE(lhs, PostfixExpression, WTFMove(identifierExpression), _startOfElementPosition);
        PARSE(variableUpdatingStatement, VariableUpdatingStatement, WTFMove(lhs));
        CONSUME_TYPE(Semicolon);
        return { variableUpdatingStatement };
    }
    case TokenType::ParenLeft:
    case TokenType::And:
    case TokenType::Star: {
        PARSE(variableUpdatingStatement, VariableUpdatingStatement);
        CONSUME_TYPE(Semicolon);
        return { variableUpdatingStatement };
    }
    case TokenType::KeywordFor: {
        // FIXME: Handle attributes attached to statement.
        return parseForStatement();
    }
    case TokenType::KeywordLoop: {
        // FIXME: Handle attributes attached to statement.
        return parseLoopStatement();
    }
    case TokenType::KeywordSwitch: {
        // FIXME: Handle attributes attached to statement.
        return parseSwitchStatement();
    }
    case TokenType::KeywordWhile: {
        // FIXME: Handle attributes attached to statement.
        return parseWhileStatement();
    }
    case TokenType::KeywordBreak: {
        consume();
        CONSUME_TYPE(Semicolon);
        RETURN_ARENA_NODE(BreakStatement);
    }
    case TokenType::KeywordContinue: {
        consume();
        CONSUME_TYPE(Semicolon);
        RETURN_ARENA_NODE(ContinueStatement);
    }
    case TokenType::KeywordDiscard: {
        consume();
        CONSUME_TYPE(Semicolon);
        RETURN_ARENA_NODE(DiscardStatement);
    }
    case TokenType::Underbar : {
        consume();
        CONSUME_TYPE(Equal);
        PARSE(rhs, Expression);
        CONSUME_TYPE(Semicolon);
        RETURN_ARENA_NODE(PhonyAssignmentStatement, WTFMove(rhs));
    }
    case TokenType::KeywordConstAssert: {
        PARSE(assert, ConstAssert);
        RETURN_ARENA_NODE(ConstAssertStatement, WTFMove(assert));
    }
    default:
        FAIL("Not a valid statement"_s);
    }
}

template<typename Lexer>
Result<AST::CompoundStatement::Ref> Parser<Lexer>::parseCompoundStatement()
{
    START_PARSE();

    PARSE(attributes, Attributes);

    CONSUME_TYPE(BraceLeft);

    AST::Statement::List statements;
    while (current().type != TokenType::BraceRight) {
        if (current().type == TokenType::Semicolon) {
            consume();
            continue;
        }

        PARSE(stmt, Statement);
        statements.append(WTFMove(stmt));
    }

    CONSUME_TYPE(BraceRight);

    RETURN_ARENA_NODE(CompoundStatement, WTFMove(attributes), WTFMove(statements));
}

template<typename Lexer>
Result<AST::Statement::Ref> Parser<Lexer>::parseIfStatement()
{
    START_PARSE();

    PARSE(attributes, Attributes);

    return parseIfStatementWithAttributes(WTFMove(attributes), _startOfElementPosition);
}

template<typename Lexer>
Result<AST::Statement::Ref> Parser<Lexer>::parseIfStatementWithAttributes(AST::Attribute::List&& attributes, SourcePosition _startOfElementPosition)
{
    CONSUME_TYPE(KeywordIf);
    PARSE(testExpr, Expression);
    PARSE(thenStmt, CompoundStatement);

    AST::Statement::Ptr maybeElseStmt = nullptr;
    if (current().type == TokenType::KeywordElse) {
        consume();
        // The syntax following an 'else' keyword can be either an 'if'
        // statement or a brace-delimited compound statement.
        if (current().type == TokenType::KeywordIf) {
            CHECK_RECURSION();
            PARSE(elseStmt, IfStatementWithAttributes, { }, _startOfElementPosition);
            maybeElseStmt = &elseStmt.get();
        } else {
            PARSE(elseStmt, CompoundStatement);
            maybeElseStmt = &elseStmt.get();
        }
    }

    RETURN_ARENA_NODE(IfStatement, WTFMove(testExpr), WTFMove(thenStmt), maybeElseStmt, WTFMove(attributes));
}

template<typename Lexer>
Result<AST::Statement::Ref> Parser<Lexer>::parseForStatement()
{
    START_PARSE();

    CONSUME_TYPE(KeywordFor);

    AST::Statement::Ptr maybeInitializer = nullptr;
    AST::Expression::Ptr maybeTest = nullptr;
    AST::Statement::Ptr maybeUpdate = nullptr;

    CONSUME_TYPE(ParenLeft);

    if (current().type != TokenType::Semicolon) {
        switch (current().type) {
        case TokenType::KeywordConst:
        case TokenType::KeywordLet:
        case TokenType::KeywordVar: {
            PARSE(variable, Variable);
            maybeInitializer = &MAKE_ARENA_NODE(VariableStatement, WTFMove(variable));
            break;
        }
        case TokenType::Identifier: {
            // FIXME: this should be should also include function calls
            PARSE(variableUpdatingStatement, VariableUpdatingStatement);
            maybeInitializer = &variableUpdatingStatement.get();
            break;
        }
        default:
            FAIL("Invalid for-loop initialization clause"_s);
        }
    }
    CONSUME_TYPE(Semicolon);

    if (current().type != TokenType::Semicolon) {
        PARSE(test, Expression);
        maybeTest = &test.get();
    }
    CONSUME_TYPE(Semicolon);

    if (current().type != TokenType::ParenRight) {
        // FIXME: this should be should also include function calls
        if (current().type != TokenType::Identifier)
            FAIL("Invalid for-loop update clause"_s);

        PARSE(variableUpdatingStatement, VariableUpdatingStatement);
        maybeUpdate = &variableUpdatingStatement.get();
    }
    CONSUME_TYPE(ParenRight);

    PARSE(body, CompoundStatement);

    RETURN_ARENA_NODE(ForStatement, maybeInitializer, maybeTest, maybeUpdate, WTFMove(body));
}

template<typename Lexer>
Result<AST::Statement::Ref> Parser<Lexer>::parseLoopStatement()
{
    START_PARSE();

    CONSUME_TYPE(KeywordLoop);
    PARSE(attributes, Attributes);

    CONSUME_TYPE(BraceLeft);
    AST::Statement::List bodyStatements;
    std::optional<AST::Continuing> maybeContinuing;

    while (current().type != TokenType::BraceRight) {
        if (current().type != TokenType::KeywordContinuing) {
            PARSE(statement, Statement);
            bodyStatements.append(WTFMove(statement));
            continue;
        }


        CONSUME_TYPE(KeywordContinuing);

        AST::Statement::List continuingStatements;
        AST::Expression* breakIf = nullptr;
        PARSE(continuingAttributes, Attributes);

        CONSUME_TYPE(BraceLeft);
        while (current().type != TokenType::BraceRight) {
            if (current().type != TokenType::KeywordBreak) {
                PARSE(statement, Statement);
                continuingStatements.append(statement);
                continue;
            }

            CONSUME_TYPE(KeywordBreak);
            if (current().type != TokenType::KeywordIf) {
                CONSUME_TYPE(Semicolon);
                continuingStatements.append(MAKE_ARENA_NODE(BreakStatement));
                continue;
            }

            CONSUME_TYPE(KeywordIf);
            PARSE(expression, Expression);
            CONSUME_TYPE(Semicolon);

            breakIf = &expression.get();
            break;
        }
        CONSUME_TYPE(BraceRight);

        maybeContinuing = { WTFMove(continuingStatements), WTFMove(continuingAttributes), breakIf };
    }
    CONSUME_TYPE(BraceRight);

    RETURN_ARENA_NODE(LoopStatement, WTFMove(attributes), WTFMove(bodyStatements), WTFMove(maybeContinuing));
}

template<typename Lexer>
Result<AST::Statement::Ref> Parser<Lexer>::parseSwitchStatement()
{
    START_PARSE();

    CONSUME_TYPE(KeywordSwitch);

    PARSE(value, Expression);
    PARSE(valueAttributes, Attributes);

    CONSUME_TYPE(BraceLeft);

    Vector<AST::SwitchClause> clauses;
    std::optional<AST::SwitchClause> defaultClause;
    unsigned selectorCount = 0;
    while (current().type != TokenType::BraceRight) {
        AST::Expression::List selectors;
        bool hasDefault = false;
        if (current().type == TokenType::KeywordCase) {
            consume();
            do {
                if (current().type == TokenType::KeywordDefault) {
                    consume();
                    hasDefault = true;
                } else {
                    ++selectorCount;
                    PARSE(selector, Expression);
                    selectors.append(WTFMove(selector));
                }

                if (current().type != TokenType::Comma)
                    break;
                CONSUME_TYPE(Comma);
            } while (current().type != TokenType::BraceLeft && current().type != TokenType::Colon);
        } else if (current().type == TokenType::KeywordDefault) {
            consume();
            hasDefault = true;
        } else
            FAIL("Expected either a `case` or `default` switch clause"_s);

        if (hasDefault && defaultClause.has_value())
            FAIL("Switch statement contains more than one default clause"_s);

        if (current().type == TokenType::Colon)
            consume();
        PARSE(body, CompoundStatement);
        ASSERT(hasDefault || !selectors.isEmpty());
        if (hasDefault)
            defaultClause = { WTFMove(selectors), body };
        else
            clauses.append({ WTFMove(selectors), body });

        // https://www.w3.org/TR/WGSL/#limits
        static constexpr unsigned maximumNumberOfCaseSelectors = 1023;
        if (UNLIKELY(selectorCount > maximumNumberOfCaseSelectors))
            FAIL(makeString("switch statement cannot have more than "_s, String::number(maximumNumberOfCaseSelectors), " case selector values"_s));
    }
    CONSUME_TYPE(BraceRight);

    if (!defaultClause.has_value())
        FAIL("Switch statement must have exactly one default clause, but it has none"_s);

    RETURN_ARENA_NODE(SwitchStatement, WTFMove(value), WTFMove(valueAttributes), WTFMove(clauses), WTFMove(*defaultClause));
}

template<typename Lexer>
Result<AST::Statement::Ref> Parser<Lexer>::parseWhileStatement()
{
    START_PARSE();

    CONSUME_TYPE(KeywordWhile);

    PARSE(test, Expression);
    PARSE(body, CompoundStatement);

    RETURN_ARENA_NODE(WhileStatement, WTFMove(test), WTFMove(body));
}

template<typename Lexer>
Result<AST::Statement::Ref> Parser<Lexer>::parseReturnStatement()
{
    START_PARSE();

    CONSUME_TYPE(KeywordReturn);

    if (current().type == TokenType::Semicolon) {
        RETURN_ARENA_NODE(ReturnStatement, nullptr);
    }

    PARSE(expr, Expression);
    RETURN_ARENA_NODE(ReturnStatement, &expr.get());
}

template<typename Lexer>
Result<AST::Statement::Ref> Parser<Lexer>::parseVariableUpdatingStatement()
{
    // https://www.w3.org/TR/WGSL/#recursive-descent-syntax-variable_updating_statement
    PARSE(lhs, LHSExpression);
    return parseVariableUpdatingStatement(WTFMove(lhs));
}

template<typename Lexer>
Result<AST::Statement::Ref> Parser<Lexer>::parseVariableUpdatingStatement(AST::Expression::Ref&& lhs)
{
    START_PARSE();

    std::optional<AST::DecrementIncrementStatement::Operation> operation;
    if (current().type == TokenType::PlusPlus)
        operation = AST::DecrementIncrementStatement::Operation::Increment;
    else if (current().type == TokenType::MinusMinus)
        operation = AST::DecrementIncrementStatement::Operation::Decrement;
    if (operation) {
        consume();
        RETURN_ARENA_NODE(DecrementIncrementStatement, WTFMove(lhs), *operation);
    }

    std::optional<AST::BinaryOperation> maybeOp;
    if (canContinueCompoundAssignmentStatement(current())) {
        maybeOp = toBinaryOperation(current());
        consume();
    } else if (current().type == TokenType::Equal)
        consume();
    else
        FAIL("Expected one of `=`, `++`, or `--`"_s);

    PARSE(rhs, Expression);

    if (maybeOp)
        RETURN_ARENA_NODE(CompoundAssignmentStatement, WTFMove(lhs), WTFMove(rhs), *maybeOp);

    RETURN_ARENA_NODE(AssignmentStatement, WTFMove(lhs), WTFMove(rhs));
}


template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseShortCircuitExpression(AST::Expression::Ref&& lhs, TokenType continuingToken, AST::BinaryOperation op)
{
    START_PARSE();
    while (current().type == continuingToken) {
        consume();
        PARSE(rhs, RelationalExpression);
        lhs = MAKE_ARENA_NODE(BinaryExpression, WTFMove(lhs), WTFMove(rhs), op);
    }

    return WTFMove(lhs);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseRelationalExpression()
{
    PARSE(unary, UnaryExpression);
    PARSE(relational, RelationalExpressionPostUnary, WTFMove(unary));
    return WTFMove(relational);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseRelationalExpressionPostUnary(AST::Expression::Ref&& lhs)
{
    START_PARSE();
    PARSE_MOVE(lhs, ShiftExpressionPostUnary, WTFMove(lhs));

    if (canContinueRelationalExpression(current())) {
        auto op = toBinaryOperation(current());
        consume();
        PARSE(rhs, ShiftExpression);
        lhs = MAKE_ARENA_NODE(BinaryExpression, WTFMove(lhs), WTFMove(rhs), op);
    }

    return WTFMove(lhs);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseShiftExpression()
{
    PARSE(unary, UnaryExpression);
    PARSE(shift, ShiftExpressionPostUnary, WTFMove(unary));
    return WTFMove(shift);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseShiftExpressionPostUnary(AST::Expression::Ref&& lhs)
{
    if (canContinueAdditiveExpression(current()))
        return parseAdditiveExpressionPostUnary(WTFMove(lhs));

    START_PARSE();
    switch (current().type) {
    case TokenType::GtGt: {
        consume();
        PARSE(rhs, UnaryExpression);
        RETURN_ARENA_NODE(BinaryExpression, WTFMove(lhs), WTFMove(rhs), AST::BinaryOperation::RightShift);
    }

    case TokenType::LtLt: {
        consume();
        PARSE(rhs, UnaryExpression);
        RETURN_ARENA_NODE(BinaryExpression, WTFMove(lhs), WTFMove(rhs), AST::BinaryOperation::LeftShift);
    }

    default:
        return WTFMove(lhs);
    }
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseAdditiveExpressionPostUnary(AST::Expression::Ref&& lhs)
{
    START_PARSE();
    PARSE_MOVE(lhs, MultiplicativeExpressionPostUnary, WTFMove(lhs));

    while (canContinueAdditiveExpression(current())) {
        // parseMultiplicativeExpression handles multiplicative operators so
        // token should be PLUS or MINUS.
        ASSERT(current().type == TokenType::Plus || current().type == TokenType::Minus);
        const auto op = toBinaryOperation(current());
        consume();
        PARSE(unary, UnaryExpression);
        PARSE(rhs, MultiplicativeExpressionPostUnary, WTFMove(unary));
        lhs = MAKE_ARENA_NODE(BinaryExpression, WTFMove(lhs), WTFMove(rhs), op);
    }

    return WTFMove(lhs);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseBitwiseExpressionPostUnary(AST::Expression::Ref&& lhs)
{
    START_PARSE();
    const auto op = toBinaryOperation(current());
    const TokenType continuingToken = current().type;
    while (current().type == continuingToken) {
        consume();
        PARSE(rhs, UnaryExpression);
        lhs = MAKE_ARENA_NODE(BinaryExpression, WTFMove(lhs), WTFMove(rhs), op);
    }

    return WTFMove(lhs);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseMultiplicativeExpressionPostUnary(AST::Expression::Ref&& lhs)
{
    START_PARSE();
    while (canContinueMultiplicativeExpression(current())) {
        auto op = AST::BinaryOperation::Multiply;
        switch (current().type) {
        case TokenType::Modulo:
            op = AST::BinaryOperation::Modulo;
            break;

        case TokenType::Slash:
            op = AST::BinaryOperation::Divide;
            break;

        case TokenType::Star:
            op = AST::BinaryOperation::Multiply;
            break;

        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        consume();
        PARSE(rhs, UnaryExpression);
        lhs = MAKE_ARENA_NODE(BinaryExpression, WTFMove(lhs), WTFMove(rhs), op);
    }

    return WTFMove(lhs);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseUnaryExpression()
{
    START_PARSE();
    CHECK_RECURSION();

    if (canBeginUnaryExpression(current())) {
        auto op = toUnaryOperation(current());
        consume();
        PARSE(expression, UnaryExpression);
        RETURN_ARENA_NODE(UnaryExpression, WTFMove(expression), op);
    }

    return parseSingularExpression();
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseSingularExpression()
{
    START_PARSE();
    PARSE(base, PrimaryExpression);
    return parsePostfixExpression(WTFMove(base), _startOfElementPosition);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parsePostfixExpression(AST::Expression::Ref&& base, SourcePosition startPosition)
{
    START_PARSE();

    AST::Expression::Ref expr = WTFMove(base);
    for (;;) {
        switch (current().type) {
        case TokenType::BracketLeft: {
            consume();
            PARSE(arrayIndex, Expression);
            CONSUME_TYPE(BracketRight);
            // FIXME: Replace with NODE_REF(...)
            SourceSpan span(startPosition, m_currentPosition);
            expr = m_builder.construct<AST::IndexAccessExpression>(span, WTFMove(expr), WTFMove(arrayIndex));
            break;
        }

        case TokenType::Period: {
            consume();
            PARSE(fieldName, Identifier);
            // FIXME: Replace with NODE_REF(...)
            SourceSpan span(startPosition, m_currentPosition);
            expr = m_builder.construct<AST::FieldAccessExpression>(span, WTFMove(expr), WTFMove(fieldName));
            break;
        }

        default:
            return { WTFMove(expr) };
        }
    }
}

// https://gpuweb.github.io/gpuweb/wgsl/#syntax-primary_expression
// primary_expression:
//   | ident
//   | callable argument_expression_list
//   | const_literal
//   | paren_expression
//   | bitcast less_than type_decl greater_than paren_expression
template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parsePrimaryExpression()
{
    START_PARSE();

    switch (current().type) {
    // paren_expression
    case TokenType::ParenLeft: {
        consume();
        PARSE(expr, Expression);
        CONSUME_TYPE(ParenRight);
        return { WTFMove(expr) };
    }
    case TokenType::Identifier: {
        PARSE(ident, Identifier);
        // FIXME: remove the special case for array
        if (ident == "array"_s) {
            PARSE(arrayType, ArrayType);
            PARSE(arguments, ArgumentExpressionList);
            RETURN_ARENA_NODE(CallExpression, WTFMove(arrayType), WTFMove(arguments));
        }
        if (current().type == TokenType::TemplateArgsLeft || current().type == TokenType::ParenLeft) {
            PARSE(type, TypeNameAfterIdentifier, WTFMove(ident), _startOfElementPosition);
            PARSE(arguments, ArgumentExpressionList);
            RETURN_ARENA_NODE(CallExpression, WTFMove(type), WTFMove(arguments));
        }
        RETURN_ARENA_NODE(IdentifierExpression, WTFMove(ident));
    }

    // const_literal
    case TokenType::KeywordTrue:
        consume();
        RETURN_ARENA_NODE(BoolLiteral, true);
    case TokenType::KeywordFalse:
        consume();
        RETURN_ARENA_NODE(BoolLiteral, false);
    case TokenType::IntegerLiteral: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteral);
        RETURN_ARENA_NODE(AbstractIntegerLiteral, lit.integerValue);
    }
    case TokenType::IntegerLiteralSigned: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteralSigned);
        RETURN_ARENA_NODE(Signed32Literal, lit.integerValue);
    }
    case TokenType::IntegerLiteralUnsigned: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteralUnsigned);
        RETURN_ARENA_NODE(Unsigned32Literal, lit.integerValue);
    }
    case TokenType::AbstractFloatLiteral: {
        CONSUME_TYPE_NAMED(lit, AbstractFloatLiteral);
        RETURN_ARENA_NODE(AbstractFloatLiteral, lit.floatValue);
    }
    case TokenType::FloatLiteral: {
        CONSUME_TYPE_NAMED(lit, FloatLiteral);
        RETURN_ARENA_NODE(Float32Literal, lit.floatValue);
    }
    case TokenType::HalfLiteral: {
        CONSUME_TYPE_NAMED(lit, HalfLiteral);
        RETURN_ARENA_NODE(Float16Literal, lit.floatValue);
    }
    // TODO: bitcast expression

    default:
        break;
    }

    FAIL("Expected one of '(', a literal, or an identifier"_s);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseExpression()
{
    PARSE(unary, UnaryExpression);
    if (canContinueBitwiseExpression(current()))
        return parseBitwiseExpressionPostUnary(WTFMove(unary));

    PARSE(relational, RelationalExpressionPostUnary, WTFMove(unary));
    if (canContinueShortCircuitAndExpression(current())) {
        PARSE_MOVE(relational, ShortCircuitExpression, WTFMove(relational), TokenType::AndAnd, AST::BinaryOperation::ShortCircuitAnd);
    } else if (canContinueShortCircuitOrExpression(current())) {
        PARSE_MOVE(relational, ShortCircuitExpression, WTFMove(relational), TokenType::OrOr, AST::BinaryOperation::ShortCircuitOr);
    } // NOLINT

    return WTFMove(relational);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseLHSExpression()
{
    START_PARSE();
    CHECK_RECURSION();

    if (current().type == TokenType::And || current().type == TokenType::Star) {
        auto op = toUnaryOperation(current());
        consume();
        PARSE(expression, LHSExpression);
        RETURN_ARENA_NODE(UnaryExpression, WTFMove(expression), op);
    }

    PARSE(base, CoreLHSExpression);
    return parsePostfixExpression(WTFMove(base), _startOfElementPosition);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseCoreLHSExpression()
{
    START_PARSE();

    switch (current().type) {
    case TokenType::ParenLeft: {
        consume();
        PARSE(expr, LHSExpression);
        CONSUME_TYPE(ParenRight);
        return { WTFMove(expr) };
    }
    case TokenType::Identifier: {
        PARSE(ident, Identifier);
        RETURN_ARENA_NODE(IdentifierExpression, WTFMove(ident));
    }
    default:
        break;
    }

    FAIL("Tried to parse the left-hand side of an assignment and failed"_s);
}

template<typename Lexer>
Result<AST::Expression::List> Parser<Lexer>::parseArgumentExpressionList()
{
    START_PARSE();
    CHECK_RECURSION();
    CONSUME_TYPE(ParenLeft);

    AST::Expression::List arguments;
    while (current().type != TokenType::ParenRight) {
        PARSE(expr, Expression);
        arguments.append(WTFMove(expr));
        if (current().type != TokenType::ParenRight) {
            CONSUME_TYPE(Comma);
        }
    }

    CONSUME_TYPE(ParenRight);
    return { WTFMove(arguments) };
}

} // namespace WGSL
