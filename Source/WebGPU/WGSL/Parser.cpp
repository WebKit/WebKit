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

#include <wtf/text/StringBuilder.h>

namespace WGSL {

template<TokenType TT, TokenType... TTs>
struct Types {
    static bool includes(TokenType tokenType)
    {
        return TT == tokenType || Types<TTs...>::includes(tokenType);
    }

    static void appendNameTo(StringBuilder& builder)
    {
        builder.append(toString(TT), ", ");
        Types<TTs...>::appendNameTo(builder);
    }
};

template <TokenType TT>
struct Types<TT> {
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
    auto _startOfElementPosition = m_lexer.currentPosition();

#define CURRENT_SOURCE_SPAN() \
    SourceSpan(_startOfElementPosition, m_lexer.currentPosition())

#define MAKE_NODE_UNIQUE_REF(type, ...) \
    makeUniqueRef<AST::type>(CURRENT_SOURCE_SPAN() __VA_OPT__(,) __VA_ARGS__) /* NOLINT */

#define RETURN_NODE(type, ...) \
    do { \
        AST::type astNodeResult(CURRENT_SOURCE_SPAN() __VA_OPT__(,) __VA_ARGS__); /* NOLINT */ \
        return { WTFMove(astNodeResult) }; \
    } while (false)

#define RETURN_NODE_REF(type, ...) \
    return { adoptRef(*new AST::type(CURRENT_SOURCE_SPAN(), __VA_ARGS__)) };

#define RETURN_NODE_UNIQUE_REF(type, ...) \
    return { MAKE_NODE_UNIQUE_REF(type __VA_OPT__(,) __VA_ARGS__) }; /* NOLINT */

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
        StringBuilder builder; \
        builder.append("Expected a "); \
        builder.append(toString(TokenType::type)); \
        builder.append(", but got a "); \
        builder.append(toString(name##Expected.error())); \
        FAIL(builder.toString()); \
    } \
    auto& name = *name##Expected;

#define CONSUME_TYPE(type) \
    do { \
        auto expectedToken = consumeType(TokenType::type); \
        if (!expectedToken) { \
            StringBuilder builder; \
            builder.append("Expected a "); \
            builder.append(toString(TokenType::type)); \
            builder.append(", but got a "); \
            builder.append(toString(expectedToken.error())); \
            FAIL(builder.toString()); \
        } \
    } while (false)

#define CONSUME_TYPES_NAMED(name, ...) \
    auto name##Expected = consumeTypes<__VA_ARGS__>(); \
    if (!name##Expected) { \
        StringBuilder builder; \
        builder.append("Expected one of ["); \
        Types<__VA_ARGS__>::appendNameTo(builder); \
        builder.append("], but got a "); \
        builder.append(toString(name##Expected.error())); \
        FAIL(builder.toString()); \
    } \
    auto& name = *name##Expected;

static bool canBeginUnaryExpression(const Token& token)
{
    switch (token.m_type) {
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
    switch (token.m_type) {
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
    switch (token.m_type) {
    case TokenType::Minus:
    case TokenType::Plus:
        return true;
    default:
        return canContinueMultiplicativeExpression(token);
    }
}

static bool canContinueBitwiseExpression(const Token& token)
{
    switch (token.m_type) {
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
    switch (token.m_type) {
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
    return token.m_type == TokenType::AndAnd;
}

static bool canContinueShortCircuitOrExpression(const Token& token)
{
    return token.m_type == TokenType::OrOr;
}

static AST::BinaryOperation toBinaryOperation(const Token& token)
{
    switch (token.m_type) {
    case TokenType::And:
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
        return AST::BinaryOperation::RightShift;
    case TokenType::Lt:
        return AST::BinaryOperation::LessThan;
    case TokenType::LtEq:
        return AST::BinaryOperation::LessEqual;
    case TokenType::LtLt:
        return AST::BinaryOperation::LeftShift;
    case TokenType::Minus:
        return AST::BinaryOperation::Subtract;
    case TokenType::Modulo:
        return AST::BinaryOperation::Modulo;
    case TokenType::Or:
        return AST::BinaryOperation::Or;
    case TokenType::OrOr:
        return AST::BinaryOperation::ShortCircuitOr;
    case TokenType::Plus:
        return AST::BinaryOperation::Add;
    case TokenType::Slash:
        return AST::BinaryOperation::Divide;
    case TokenType::Star:
        return AST::BinaryOperation::Multiply;
    case TokenType::Xor:
        return AST::BinaryOperation::Xor;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static AST::UnaryOperation toUnaryOperation(const Token& token)
{
    switch (token.m_type) {
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
std::optional<Error> parse(ShaderModule& shaderModule)
{
    Lexer lexer(shaderModule.source());
    Parser parser(shaderModule, lexer);
    auto result = parser.parseShader();
    if (!result.has_value())
        return result.error();
    return std::nullopt;
}

std::optional<Error> parse(ShaderModule& shaderModule)
{
    if (shaderModule.source().is8Bit())
        return parse<Lexer<LChar>>(shaderModule);
    return parse<Lexer<UChar>>(shaderModule);
}

template<typename Lexer>
Result<AST::Expression::Ref> parseExpression(const String& source)
{
    ShaderModule shaderModule(source);
    Lexer lexer(shaderModule.source());
    Parser parser(shaderModule, lexer);
    return parser.parseExpression();
}

Result<AST::Expression::Ref> parseExpression(const String& source)
{
    if (source.is8Bit())
        return parseExpression<Lexer<LChar>>(source);
    return parseExpression<Lexer<UChar>>(source);
}

template<typename Lexer>
Expected<Token, TokenType> Parser<Lexer>::consumeType(TokenType type)
{
    if (current().m_type == type) {
        Expected<Token, TokenType> result = { m_current };
        m_current = m_lexer.lex();
        return result;
    }
    return makeUnexpected(current().m_type);
}

template<typename Lexer>
template<TokenType... TTs>
Expected<Token, TokenType> Parser<Lexer>::consumeTypes()
{
    auto token = m_current;
    if (Types<TTs...>::includes(token.m_type)) {
        m_current = m_lexer.lex();
        return { token };
    }
    return makeUnexpected(token.m_type);
}

template<typename Lexer>
void Parser<Lexer>::consume()
{
    m_current = m_lexer.lex();
}

template<typename Lexer>
Result<void> Parser<Lexer>::parseShader()
{
    // FIXME: parse directives here.

    while (!m_lexer.isAtEndOfFile()) {
        auto globalExpected = parseGlobalDecl();
        if (!globalExpected)
            return makeUnexpected(globalExpected.error());
    }

    return { };
}

template<typename Lexer>
Result<AST::Identifier> Parser<Lexer>::parseIdentifier()
{
    START_PARSE();

    CONSUME_TYPE_NAMED(name, Identifier);

    return AST::Identifier::makeWithSpan(CURRENT_SOURCE_SPAN(), WTFMove(name.m_ident));
}

template<typename Lexer>
Result<void> Parser<Lexer>::parseGlobalDecl()
{
    START_PARSE();

    while (current().m_type == TokenType::Semicolon)
        consume();

    if (current().m_type == TokenType::KeywordConst) {
        PARSE(variable, Variable);
        CONSUME_TYPE(Semicolon);
        m_shaderModule.variables().append(WTFMove(variable));
        return { };
    }

    PARSE(attributes, Attributes);

    switch (current().m_type) {
    case TokenType::KeywordStruct: {
        PARSE(structure, Structure, WTFMove(attributes));
        m_shaderModule.structures().append(WTFMove(structure));
        return { };
    }
    case TokenType::KeywordOverride:
    case TokenType::KeywordVar: {
        PARSE(variable, VariableWithAttributes, WTFMove(attributes));
        CONSUME_TYPE(Semicolon);
        m_shaderModule.variables().append(WTFMove(variable));
        return { };
    }
    case TokenType::KeywordFn: {
        PARSE(fn, Function, WTFMove(attributes));
        m_shaderModule.functions().append(makeUniqueRef<AST::Function>(WTFMove(fn)));
        return { };
    }
    default:
        FAIL("Trying to parse a GlobalDecl, expected 'const', 'fn', 'override', 'struct' or 'var'."_s);
    }
}

template<typename Lexer>
Result<AST::Attribute::List> Parser<Lexer>::parseAttributes()
{
    AST::Attribute::List attributes;

    while (current().m_type == TokenType::Attribute) {
        PARSE(firstAttribute, Attribute);
        attributes.append(WTFMove(firstAttribute));
    }

    return { WTFMove(attributes) };
}

template<typename Lexer>
Result<Ref<AST::Attribute>> Parser<Lexer>::parseAttribute()
{
    START_PARSE();

    CONSUME_TYPE(Attribute);
    CONSUME_TYPE_NAMED(ident, Identifier);

    if (ident.m_ident == "group"_s) {
        CONSUME_TYPE(ParenLeft);
        // FIXME: should more kinds of literals be accepted here?
        CONSUME_TYPE_NAMED(id, IntegerLiteral);
        CONSUME_TYPE(ParenRight);
        RETURN_NODE_REF(GroupAttribute, id.m_literalValue);
    }

    if (ident.m_ident == "binding"_s) {
        CONSUME_TYPE(ParenLeft);
        // FIXME: should more kinds of literals be accepted here?
        CONSUME_TYPE_NAMED(id, IntegerLiteral);
        CONSUME_TYPE(ParenRight);
        RETURN_NODE_REF(BindingAttribute, id.m_literalValue);
    }

    if (ident.m_ident == "location"_s) {
        CONSUME_TYPE(ParenLeft);
        // FIXME: should more kinds of literals be accepted here?
        CONSUME_TYPE_NAMED(id, IntegerLiteral);
        CONSUME_TYPE(ParenRight);
        RETURN_NODE_REF(LocationAttribute, id.m_literalValue);
    }

    if (ident.m_ident == "builtin"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(name, Identifier);
        CONSUME_TYPE(ParenRight);
        RETURN_NODE_REF(BuiltinAttribute, WTFMove(name));
    }

    if (ident.m_ident == "workgroup_size"_s) {
        CONSUME_TYPE(ParenLeft);
        // FIXME: should more kinds of literals be accepted here?
        CONSUME_TYPE_NAMED(id, IntegerLiteralUnsigned);
        CONSUME_TYPE(ParenRight);
        RETURN_NODE_REF(WorkgroupSizeAttribute, id.m_literalValue);
    }

    // https://gpuweb.github.io/gpuweb/wgsl/#pipeline-stage-attributes
    if (ident.m_ident == "vertex"_s)
        RETURN_NODE_REF(StageAttribute, AST::StageAttribute::Stage::Vertex);
    if (ident.m_ident == "compute"_s)
        RETURN_NODE_REF(StageAttribute, AST::StageAttribute::Stage::Compute);
    if (ident.m_ident == "fragment"_s)
        RETURN_NODE_REF(StageAttribute, AST::StageAttribute::Stage::Fragment);

    FAIL("Unknown attribute. Supported attributes are 'group', 'binding', 'location', 'builtin', 'vertex', 'compute', 'fragment'."_s);
}

template<typename Lexer>
Result<AST::Structure::Ref> Parser<Lexer>::parseStructure(AST::Attribute::List&& attributes)
{
    START_PARSE();

    CONSUME_TYPE(KeywordStruct);
    PARSE(name, Identifier);
    CONSUME_TYPE(BraceLeft);

    AST::StructureMember::List members;
    while (current().m_type != TokenType::BraceRight) {
        PARSE(member, StructureMember);
        members.append(makeUniqueRef<AST::StructureMember>(WTFMove(member)));
        if (current().m_type == TokenType::Comma)
            consume();
        else
            break;
    }

    CONSUME_TYPE(BraceRight);

    RETURN_NODE_UNIQUE_REF(Structure, WTFMove(name), WTFMove(members), WTFMove(attributes), AST::StructureRole::UserDefined);
}

template<typename Lexer>
Result<AST::StructureMember> Parser<Lexer>::parseStructureMember()
{
    START_PARSE();

    PARSE(attributes, Attributes);
    PARSE(name, Identifier);
    CONSUME_TYPE(Colon);
    PARSE(type, TypeName);

    RETURN_NODE(StructureMember, WTFMove(name), WTFMove(type), WTFMove(attributes));
}

template<typename Lexer>
Result<AST::TypeName::Ref> Parser<Lexer>::parseTypeName()
{
    START_PARSE();

    if (current().m_type == TokenType::KeywordArray)
        return parseArrayType();
    if (current().m_type == TokenType::KeywordI32) {
        consume();
        RETURN_NODE_REF(NamedTypeName, AST::Identifier::makeWithSpan(CURRENT_SOURCE_SPAN(), StringView { "i32"_s }));
    }
    if (current().m_type == TokenType::KeywordF32) {
        consume();
        RETURN_NODE_REF(NamedTypeName, AST::Identifier::makeWithSpan(CURRENT_SOURCE_SPAN(), StringView { "f32"_s }));
    }
    if (current().m_type == TokenType::KeywordU32) {
        consume();
        RETURN_NODE_REF(NamedTypeName, AST::Identifier::makeWithSpan(CURRENT_SOURCE_SPAN(), StringView { "u32"_s }));
    }
    if (current().m_type == TokenType::KeywordBool) {
        consume();
        RETURN_NODE_REF(NamedTypeName, AST::Identifier::makeWithSpan(CURRENT_SOURCE_SPAN(), StringView { "bool"_s }));
    }
    if (current().m_type == TokenType::Identifier) {
        PARSE(name, Identifier);
        return parseTypeNameAfterIdentifier(WTFMove(name), _startOfElementPosition);
    }

    FAIL("Tried parsing a type and it did not start with an identifier"_s);
}

template<typename Lexer>
Result<AST::TypeName::Ref> Parser<Lexer>::parseTypeNameAfterIdentifier(AST::Identifier&& name, SourcePosition _startOfElementPosition) // NOLINT
{
    auto kind = AST::ParameterizedTypeName::stringViewToKind(name.id());
    if (kind && current().m_type == TokenType::Lt) {
        CONSUME_TYPE(Lt);
        PARSE(elementType, TypeName);
        CONSUME_TYPE(Gt);
        RETURN_NODE_REF(ParameterizedTypeName, *kind, WTFMove(elementType));
    }
    RETURN_NODE_REF(NamedTypeName, WTFMove(name));
}

template<typename Lexer>
Result<AST::TypeName::Ref> Parser<Lexer>::parseArrayType()
{
    START_PARSE();

    CONSUME_TYPE(KeywordArray);

    AST::TypeName::Ptr maybeElementType;
    AST::Expression::Ptr maybeElementCount;

    if (current().m_type == TokenType::Lt) {
        // We differ from the WGSL grammar here by allowing the type to be optional,
        // which allows us to use `parseArrayType` in `parseCallExpression`.
        consume();

        PARSE(elementType, TypeName);
        maybeElementType = WTFMove(elementType);

        if (current().m_type == TokenType::Comma) {
            consume();
            // FIXME: According to https://www.w3.org/TR/WGSL/#syntax-element_count_expression
            // this should be: AdditiveExpression | BitwiseExpression.
            //
            // The WGSL grammar doesn't specify expression operator precedence so
            // until then just parse AdditiveExpression.
            PARSE(elementCountLHS, UnaryExpression);
            PARSE(elementCount, AdditiveExpressionPostUnary, WTFMove(elementCountLHS));
            maybeElementCount = elementCount.moveToUniquePtr();
        }
        CONSUME_TYPE(Gt);
    }

    RETURN_NODE_REF(ArrayTypeName, WTFMove(maybeElementType), WTFMove(maybeElementCount));
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
        switch (token.m_type) {
        case TokenType::KeywordConst:
            return AST::VariableFlavor::Const;
        case TokenType::KeywordLet:
            return AST::VariableFlavor::Let;
        case TokenType::KeywordOverride:
            return AST::VariableFlavor::Override;
        default:
            ASSERT(token.m_type == TokenType::KeywordVar);
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

    std::unique_ptr<AST::VariableQualifier> maybeQualifier = nullptr;
    if (current().m_type == TokenType::Lt) {
        PARSE(variableQualifier, VariableQualifier);
        maybeQualifier = WTF::makeUnique<AST::VariableQualifier>(WTFMove(variableQualifier));
    }

    PARSE(name, Identifier);

    AST::TypeName::Ptr maybeType = nullptr;
    if (current().m_type == TokenType::Colon) {
        consume();
        PARSE(TypeName, TypeName);
        maybeType = WTFMove(TypeName);
    }

    std::unique_ptr<AST::Expression> maybeInitializer = nullptr;
    if (current().m_type == TokenType::Equal) {
        consume();
        PARSE(initializerExpr, Expression);
        maybeInitializer = initializerExpr.moveToUniquePtr();
    }

    RETURN_NODE_UNIQUE_REF(Variable, varFlavor, WTFMove(name), WTFMove(maybeQualifier), WTFMove(maybeType), WTFMove(maybeInitializer), WTFMove(attributes));
}

template<typename Lexer>
Result<AST::VariableQualifier> Parser<Lexer>::parseVariableQualifier()
{
    START_PARSE();

    CONSUME_TYPE(Lt);
    PARSE(storageClass, StorageClass);

    // FIXME: verify that Read is the correct default in all cases.
    AST::AccessMode accessMode = AST::AccessMode::Read;
    if (current().m_type == TokenType::Comma) {
        consume();
        PARSE(actualAccessMode, AccessMode);
        accessMode = actualAccessMode;
    }

    CONSUME_TYPE(Gt);

    RETURN_NODE(VariableQualifier, storageClass, accessMode);
}

template<typename Lexer>
Result<AST::StorageClass> Parser<Lexer>::parseStorageClass()
{
    START_PARSE();

    if (current().m_type == TokenType::KeywordFunction) {
        consume();
        return { AST::StorageClass::Function };
    }
    if (current().m_type == TokenType::KeywordPrivate) {
        consume();
        return { AST::StorageClass::Private };
    }
    if (current().m_type == TokenType::KeywordWorkgroup) {
        consume();
        return { AST::StorageClass::Workgroup };
    }
    if (current().m_type == TokenType::KeywordUniform) {
        consume();
        return { AST::StorageClass::Uniform };
    }
    if (current().m_type == TokenType::KeywordStorage) {
        consume();
        return { AST::StorageClass::Storage };
    }

    FAIL("Expected one of 'function'/'private'/'storage'/'uniform'/'workgroup'"_s);
}

template<typename Lexer>
Result<AST::AccessMode> Parser<Lexer>::parseAccessMode()
{
    START_PARSE();

    if (current().m_type == TokenType::KeywordRead) {
        consume();
        return { AST::AccessMode::Read };
    }
    if (current().m_type == TokenType::KeywordWrite) {
        consume();
        return { AST::AccessMode::Write };
    }
    if (current().m_type == TokenType::KeywordReadWrite) {
        consume();
        return { AST::AccessMode::ReadWrite };
    }

    FAIL("Expected one of 'read'/'write'/'read_write'"_s);
}

template<typename Lexer>
Result<AST::Function> Parser<Lexer>::parseFunction(AST::Attribute::List&& attributes)
{
    START_PARSE();

    CONSUME_TYPE(KeywordFn);
    PARSE(name, Identifier);

    CONSUME_TYPE(ParenLeft);
    AST::Parameter::List parameters;
    while (current().m_type != TokenType::ParenRight) {
        PARSE(parameter, Parameter);
        parameters.append(makeUniqueRef<AST::Parameter>(WTFMove(parameter)));
        if (current().m_type == TokenType::Comma)
            consume();
        else
            break;
    }
    CONSUME_TYPE(ParenRight);

    AST::Attribute::List returnAttributes;
    AST::TypeName::Ptr maybeReturnType = nullptr;
    if (current().m_type == TokenType::Arrow) {
        consume();
        PARSE(parsedReturnAttributes, Attributes);
        returnAttributes = WTFMove(parsedReturnAttributes);
        PARSE(type, TypeName);
        maybeReturnType = WTFMove(type);
    }

    PARSE(body, CompoundStatement);

    RETURN_NODE(Function, WTFMove(name), WTFMove(parameters), WTFMove(maybeReturnType), WTFMove(body), WTFMove(attributes), WTFMove(returnAttributes));
}

template<typename Lexer>
Result<AST::Parameter> Parser<Lexer>::parseParameter()
{
    START_PARSE();

    PARSE(attributes, Attributes);
    PARSE(name, Identifier)
    CONSUME_TYPE(Colon);
    PARSE(type, TypeName);

    RETURN_NODE(Parameter, WTFMove(name), WTFMove(type), WTFMove(attributes), AST::ParameterRole::UserDefined);
}

template<typename Lexer>
Result<AST::Statement::Ref> Parser<Lexer>::parseStatement()
{
    START_PARSE();

    while (current().m_type == TokenType::Semicolon)
        consume();

    switch (current().m_type) {
    case TokenType::BraceLeft: {
        PARSE(compoundStmt, CompoundStatement);
        return { makeUniqueRef<AST::CompoundStatement>(WTFMove(compoundStmt)) };
    }
    case TokenType::KeywordReturn: {
        PARSE(returnStmt, ReturnStatement);
        CONSUME_TYPE(Semicolon);
        return { makeUniqueRef<AST::ReturnStatement>(WTFMove(returnStmt)) };
    }
    case TokenType::KeywordConst:
    case TokenType::KeywordLet:
    case TokenType::KeywordVar: {
        PARSE(variable, Variable);
        CONSUME_TYPE(Semicolon);
        return { makeUniqueRef<AST::VariableStatement>(CURRENT_SOURCE_SPAN(), WTFMove(variable)) };
    }
    case TokenType::Identifier: {
        // FIXME: there will be other cases here eventually for function calls
        PARSE(lhs, LHSExpression);
        CONSUME_TYPE(Equal);
        PARSE(rhs, Expression);
        CONSUME_TYPE(Semicolon);
        RETURN_NODE_UNIQUE_REF(AssignmentStatement, WTFMove(lhs), WTFMove(rhs));
    }
    default:
        FAIL("Not a valid statement"_s);
    }
}

template<typename Lexer>
Result<AST::CompoundStatement> Parser<Lexer>::parseCompoundStatement()
{
    START_PARSE();

    CONSUME_TYPE(BraceLeft);

    AST::Statement::List statements;
    while (current().m_type != TokenType::BraceRight) {
        PARSE(stmt, Statement);
        statements.append(WTFMove(stmt));
    }

    CONSUME_TYPE(BraceRight);

    RETURN_NODE(CompoundStatement, WTFMove(statements));
}

template<typename Lexer>
Result<AST::ReturnStatement> Parser<Lexer>::parseReturnStatement()
{
    START_PARSE();

    CONSUME_TYPE(KeywordReturn);

    if (current().m_type == TokenType::Semicolon) {
        RETURN_NODE(ReturnStatement, { });
    }

    PARSE(expr, Expression);
    RETURN_NODE(ReturnStatement, expr.moveToUniquePtr());
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseShortCircuitExpression(AST::Expression::Ref&& lhs, TokenType continuingToken, AST::BinaryOperation op)
{
    START_PARSE();
    while (current().m_type == continuingToken) {
        consume();
        PARSE(rhs, RelationalExpression);
        lhs = MAKE_NODE_UNIQUE_REF(BinaryExpression, WTFMove(lhs), WTFMove(rhs), op);
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
        lhs = MAKE_NODE_UNIQUE_REF(BinaryExpression, WTFMove(lhs), WTFMove(rhs), op);
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
    switch (current().m_type) {
    case TokenType::GtGt: {
        consume();
        PARSE(rhs, UnaryExpression);
        RETURN_NODE_UNIQUE_REF(BinaryExpression, WTFMove(lhs), WTFMove(rhs), AST::BinaryOperation::RightShift);
    }

    case TokenType::LtLt: {
        consume();
        PARSE(rhs, UnaryExpression);
        RETURN_NODE_UNIQUE_REF(BinaryExpression, WTFMove(lhs), WTFMove(rhs), AST::BinaryOperation::LeftShift);
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
        ASSERT(current().m_type == TokenType::Plus || current().m_type == TokenType::Minus);
        const auto op = toBinaryOperation(current());
        consume();
        PARSE(unary, UnaryExpression);
        PARSE(rhs, MultiplicativeExpressionPostUnary, WTFMove(unary));
        lhs = MAKE_NODE_UNIQUE_REF(BinaryExpression, WTFMove(lhs), WTFMove(rhs), op);
    }

    return WTFMove(lhs);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseBitwiseExpressionPostUnary(AST::Expression::Ref&& lhs)
{
    START_PARSE();
    const auto op = toBinaryOperation(current());
    const TokenType continuingToken = current().m_type;
    while (current().m_type == continuingToken) {
        consume();
        PARSE(rhs, UnaryExpression);
        lhs = MAKE_NODE_UNIQUE_REF(BinaryExpression, WTFMove(lhs), WTFMove(rhs), op);
    }

    return WTFMove(lhs);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseMultiplicativeExpressionPostUnary(AST::Expression::Ref&& lhs)
{
    START_PARSE();
    while (canContinueMultiplicativeExpression(current())) {
        auto op = AST::BinaryOperation::Multiply;
        switch (current().m_type) {
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
        lhs = MAKE_NODE_UNIQUE_REF(BinaryExpression, WTFMove(lhs), WTFMove(rhs), op);
    }

    return WTFMove(lhs);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseUnaryExpression()
{
    START_PARSE();

    if (canBeginUnaryExpression(current())) {
        auto op = toUnaryOperation(current());
        consume();
        PARSE(expression, SingularExpression);
        RETURN_NODE_UNIQUE_REF(UnaryExpression, WTFMove(expression), op);
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
Result<AST::Expression::Ref> Parser<Lexer>::parsePostfixExpression(UniqueRef<AST::Expression>&& base, SourcePosition startPosition)
{
    START_PARSE();

    UniqueRef<AST::Expression> expr = WTFMove(base);
    // FIXME: add the case for array/vector/matrix access

    for (;;) {
        switch (current().m_type) {
        case TokenType::BracketLeft: {
            consume();
            PARSE(arrayIndex, Expression);
            CONSUME_TYPE(BracketRight);
            // FIXME: Replace with NODE_REF(...)
            SourceSpan span(startPosition, m_lexer.currentPosition());
            expr = makeUniqueRef<AST::IndexAccessExpression>(span, WTFMove(expr), WTFMove(arrayIndex));
            break;
        }

        case TokenType::Period: {
            consume();
            PARSE(fieldName, Identifier);
            // FIXME: Replace with NODE_REF(...)
            SourceSpan span(startPosition, m_lexer.currentPosition());
            expr = makeUniqueRef<AST::FieldAccessExpression>(span, WTFMove(expr), WTFMove(fieldName));
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

    switch (current().m_type) {
    // paren_expression
    case TokenType::ParenLeft: {
        consume();
        PARSE(expr, Expression);
        CONSUME_TYPE(ParenRight);
        return { WTFMove(expr) };
    }
    case TokenType::Identifier: {
        PARSE(ident, Identifier);
        // FIXME: WGSL grammar has an ambiguity when trying to distinguish the
        // use of < as either the less-than operator or the beginning of a
        // template-parameter list. Here we are checking for vector or matrix
        // type names. Alternatively, those names could be turned into keywords
        auto typePrefix = AST::ParameterizedTypeName::stringViewToKind(ident.id());
        if ((typePrefix && current().m_type == TokenType::Lt) || current().m_type == TokenType::ParenLeft) {
            PARSE(type, TypeNameAfterIdentifier, WTFMove(ident), _startOfElementPosition);
            PARSE(arguments, ArgumentExpressionList);
            RETURN_NODE_UNIQUE_REF(CallExpression, WTFMove(type), WTFMove(arguments));
        }
        RETURN_NODE_UNIQUE_REF(IdentifierExpression, WTFMove(ident));
    }
    case TokenType::KeywordArray: {
        PARSE(arrayType, ArrayType);
        PARSE(arguments, ArgumentExpressionList);
        RETURN_NODE_UNIQUE_REF(CallExpression, WTFMove(arrayType), WTFMove(arguments));
    }

    // const_literal
    case TokenType::LiteralTrue:
        consume();
        RETURN_NODE_UNIQUE_REF(BoolLiteral, true);
    case TokenType::LiteralFalse:
        consume();
        RETURN_NODE_UNIQUE_REF(BoolLiteral, false);
    case TokenType::IntegerLiteral: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteral);
        RETURN_NODE_UNIQUE_REF(AbstractIntegerLiteral, lit.m_literalValue);
    }
    case TokenType::IntegerLiteralSigned: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteralSigned);
        RETURN_NODE_UNIQUE_REF(Signed32Literal, lit.m_literalValue);
    }
    case TokenType::IntegerLiteralUnsigned: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteralUnsigned);
        RETURN_NODE_UNIQUE_REF(Unsigned32Literal, lit.m_literalValue);
    }
    case TokenType::DecimalFloatLiteral: {
        CONSUME_TYPE_NAMED(lit, DecimalFloatLiteral);
        RETURN_NODE_UNIQUE_REF(AbstractFloatLiteral, lit.m_literalValue);
    }
    case TokenType::HexFloatLiteral: {
        CONSUME_TYPE_NAMED(lit, HexFloatLiteral);
        RETURN_NODE_UNIQUE_REF(AbstractFloatLiteral, lit.m_literalValue);
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
    // FIXME: Fill in
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

    // FIXME: Add the possibility of a prefix
    PARSE(base, CoreLHSExpression);
    return parsePostfixExpression(WTFMove(base), _startOfElementPosition);
}

template<typename Lexer>
Result<AST::Expression::Ref> Parser<Lexer>::parseCoreLHSExpression()
{
    START_PARSE();

    switch (current().m_type) {
    case TokenType::ParenLeft: {
        consume();
        PARSE(expr, LHSExpression);
        CONSUME_TYPE(ParenRight);
        return { WTFMove(expr) };
    }
    case TokenType::Identifier: {
        PARSE(ident, Identifier);
        RETURN_NODE_UNIQUE_REF(IdentifierExpression, WTFMove(ident));
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
    CONSUME_TYPE(ParenLeft);

    AST::Expression::List arguments;
    while (current().m_type != TokenType::ParenRight) {
        PARSE(expr, Expression);
        arguments.append(WTFMove(expr));
        if (current().m_type != TokenType::ParenRight) {
            CONSUME_TYPE(Comma);
        }
    }

    CONSUME_TYPE(ParenRight);
    return { WTFMove(arguments) };
}

} // namespace WGSL
