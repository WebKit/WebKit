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
struct TemplateTypes {
    static bool includes(TokenType tokenType)
    {
        return TT == tokenType || TemplateTypes<TTs...>::includes(tokenType);
    }

    static void appendNameTo(StringBuilder& builder)
    {
        builder.append(toString(TT), ", ");
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
    auto _startOfElementPosition = m_lexer.currentPosition();

#define CURRENT_SOURCE_SPAN() \
    SourceSpan(_startOfElementPosition, m_lexer.currentPosition())

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
        TemplateTypes<__VA_ARGS__>::appendNameTo(builder); \
        builder.append("], but got a "); \
        builder.append(toString(name##Expected.error())); \
        FAIL(builder.toString()); \
    } \
    auto& name = *name##Expected;

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
Expected<Token, TokenType> Parser<Lexer>::consumeType(TokenType type)
{
    if (current().type == type) {
        Expected<Token, TokenType> result = { m_current };
        m_current = m_lexer.lex();
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
        m_current = m_lexer.lex();
        return { token };
    }
    return makeUnexpected(token.type);
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

    while (current().type != TokenType::EndOfFile) {
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

    return AST::Identifier::makeWithSpan(CURRENT_SOURCE_SPAN(), WTFMove(name.ident));
}

template<typename Lexer>
Result<void> Parser<Lexer>::parseGlobalDecl()
{
    START_PARSE();

    while (current().type == TokenType::Semicolon)
        consume();

    if (current().type == TokenType::KeywordConst) {
        PARSE(variable, Variable);
        CONSUME_TYPE(Semicolon);
        m_shaderModule.variables().append(WTFMove(variable));
        return { };
    }

    PARSE(attributes, Attributes);

    switch (current().type) {
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
        m_shaderModule.functions().append(WTFMove(fn));
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
    CONSUME_TYPE_NAMED(ident, Identifier);

    if (ident.ident == "group"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(group, Expression);
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(GroupAttribute, WTFMove(group));
    }

    if (ident.ident == "binding"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(binding, Expression);
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(BindingAttribute, WTFMove(binding));
    }

    if (ident.ident == "location"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(location, Expression);
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(LocationAttribute, WTFMove(location));
    }

    if (ident.ident == "builtin"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(name, Identifier);
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(BuiltinAttribute, WTFMove(name));
    }

    if (ident.ident == "workgroup_size"_s) {
        CONSUME_TYPE(ParenLeft);
        // FIXME: should more kinds of literals be accepted here?
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
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(AlignAttribute, WTFMove(alignment));
    }

    if (ident.ident == "size"_s) {
        CONSUME_TYPE(ParenLeft);
        PARSE(size, Expression);
        CONSUME_TYPE(ParenRight);
        RETURN_ARENA_NODE(SizeAttribute, WTFMove(size));
    }

    // https://gpuweb.github.io/gpuweb/wgsl/#pipeline-stage-attributes
    if (ident.ident == "vertex"_s)
        RETURN_ARENA_NODE(StageAttribute, AST::StageAttribute::Stage::Vertex);
    if (ident.ident == "compute"_s)
        RETURN_ARENA_NODE(StageAttribute, AST::StageAttribute::Stage::Compute);
    if (ident.ident == "fragment"_s)
        RETURN_ARENA_NODE(StageAttribute, AST::StageAttribute::Stage::Fragment);

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
    while (current().type != TokenType::BraceRight) {
        PARSE(member, StructureMember);
        members.append(member);
        if (current().type == TokenType::Comma)
            consume();
        else
            break;
    }

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
Result<AST::TypeName::Ref> Parser<Lexer>::parseTypeName()
{
    START_PARSE();

    if (current().type == TokenType::KeywordArray)
        return parseArrayType();
    if (current().type == TokenType::KeywordI32) {
        consume();
        RETURN_ARENA_NODE(NamedTypeName, AST::Identifier::makeWithSpan(CURRENT_SOURCE_SPAN(), StringView { "i32"_s }));
    }
    if (current().type == TokenType::KeywordF32) {
        consume();
        RETURN_ARENA_NODE(NamedTypeName, AST::Identifier::makeWithSpan(CURRENT_SOURCE_SPAN(), StringView { "f32"_s }));
    }
    if (current().type == TokenType::KeywordU32) {
        consume();
        RETURN_ARENA_NODE(NamedTypeName, AST::Identifier::makeWithSpan(CURRENT_SOURCE_SPAN(), StringView { "u32"_s }));
    }
    if (current().type == TokenType::KeywordBool) {
        consume();
        RETURN_ARENA_NODE(NamedTypeName, AST::Identifier::makeWithSpan(CURRENT_SOURCE_SPAN(), StringView { "bool"_s }));
    }
    if (current().type == TokenType::Identifier) {
        PARSE(name, Identifier);
        return parseTypeNameAfterIdentifier(WTFMove(name), _startOfElementPosition);
    }

    FAIL("Tried parsing a type and it did not start with an identifier"_s);
}

template<typename Lexer>
Result<AST::TypeName::Ref> Parser<Lexer>::parseTypeNameAfterIdentifier(AST::Identifier&& name, SourcePosition _startOfElementPosition) // NOLINT
{
    auto kind = AST::ParameterizedTypeName::stringViewToKind(name.id());
    if (kind && current().type == TokenType::Lt) {
        CONSUME_TYPE(Lt);
        PARSE(elementType, TypeName);
        CONSUME_TYPE(Gt);
        RETURN_ARENA_NODE(ParameterizedTypeName, *kind, WTFMove(elementType));
    }
    RETURN_ARENA_NODE(NamedTypeName, WTFMove(name));
}

template<typename Lexer>
Result<AST::TypeName::Ref> Parser<Lexer>::parseArrayType()
{
    START_PARSE();

    CONSUME_TYPE(KeywordArray);

    AST::TypeName::Ptr maybeElementType = nullptr;
    AST::Expression::Ptr maybeElementCount = nullptr;

    if (current().type == TokenType::Lt) {
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
            PARSE(elementCountLHS, UnaryExpression);
            PARSE(elementCount, AdditiveExpressionPostUnary, WTFMove(elementCountLHS));
            maybeElementCount = &elementCount.get();
        }
        CONSUME_TYPE(Gt);
    }

    RETURN_ARENA_NODE(ArrayTypeName, maybeElementType, maybeElementCount);
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
    if (current().type == TokenType::Lt) {
        PARSE(variableQualifier, VariableQualifier);
        maybeQualifier = &variableQualifier.get();
    }

    PARSE(name, Identifier);

    AST::TypeName::Ptr maybeType = nullptr;
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

    CONSUME_TYPE(Lt);
    PARSE(storageClass, StorageClass);

    AST::AccessMode accessMode;
    if (current().type == TokenType::Comma) {
        consume();
        PARSE(actualAccessMode, AccessMode);
        accessMode = actualAccessMode;
    } else {
        // Default access mode based on address space
        // https://www.w3.org/TR/WGSL/#address-space
        switch (storageClass) {
        case AST::StorageClass::Function:
        case AST::StorageClass::Private:
        case AST::StorageClass::Workgroup:
            accessMode = AST::AccessMode::ReadWrite;
            break;
        case AST::StorageClass::Uniform:
        case AST::StorageClass::Storage:
            accessMode = AST::AccessMode::Read;
            break;
        }
    }

    CONSUME_TYPE(Gt);
    RETURN_ARENA_NODE(VariableQualifier, storageClass, accessMode);
}

template<typename Lexer>
Result<AST::StorageClass> Parser<Lexer>::parseStorageClass()
{
    START_PARSE();

    if (current().type == TokenType::KeywordFunction) {
        consume();
        return { AST::StorageClass::Function };
    }
    if (current().type == TokenType::KeywordPrivate) {
        consume();
        return { AST::StorageClass::Private };
    }
    if (current().type == TokenType::KeywordWorkgroup) {
        consume();
        return { AST::StorageClass::Workgroup };
    }
    if (current().type == TokenType::KeywordUniform) {
        consume();
        return { AST::StorageClass::Uniform };
    }
    if (current().type == TokenType::KeywordStorage) {
        consume();
        return { AST::StorageClass::Storage };
    }

    FAIL("Expected one of 'function'/'private'/'storage'/'uniform'/'workgroup'"_s);
}

template<typename Lexer>
Result<AST::AccessMode> Parser<Lexer>::parseAccessMode()
{
    START_PARSE();

    if (current().type == TokenType::KeywordRead) {
        consume();
        return { AST::AccessMode::Read };
    }
    if (current().type == TokenType::KeywordWrite) {
        consume();
        return { AST::AccessMode::Write };
    }
    if (current().type == TokenType::KeywordReadWrite) {
        consume();
        return { AST::AccessMode::ReadWrite };
    }

    FAIL("Expected one of 'read'/'write'/'read_write'"_s);
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
        if (current().type == TokenType::Comma)
            consume();
        else
            break;
    }
    CONSUME_TYPE(ParenRight);

    AST::Attribute::List returnAttributes;
    AST::TypeName::Ptr maybeReturnType = nullptr;
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

    while (current().type == TokenType::Semicolon)
        consume();

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
        // FIXME: there will be other cases here eventually for function calls
        PARSE(variableUpdatingStatement, VariableUpdatingStatement);
        CONSUME_TYPE(Semicolon);
        return { variableUpdatingStatement };
    }
    case TokenType::KeywordFor: {
        // FIXME: Handle attributes attached to statement.
        return parseForStatement();
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
    case TokenType::Underbar : {
        consume();
        CONSUME_TYPE(Equal);
        PARSE(rhs, Expression);
        CONSUME_TYPE(Semicolon);
        RETURN_ARENA_NODE(PhonyAssignmentStatement, WTFMove(rhs));
    }
    default:
        FAIL("Not a valid statement"_s);
    }
}

template<typename Lexer>
Result<AST::CompoundStatement::Ref> Parser<Lexer>::parseCompoundStatement()
{
    START_PARSE();

    CONSUME_TYPE(BraceLeft);

    AST::Statement::List statements;
    while (current().type != TokenType::BraceRight) {
        PARSE(stmt, Statement);
        statements.append(WTFMove(stmt));
    }

    CONSUME_TYPE(BraceRight);

    RETURN_ARENA_NODE(CompoundStatement, WTFMove(statements));
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
    START_PARSE();
    PARSE(lhs, LHSExpression);

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

    if (canBeginUnaryExpression(current())) {
        auto op = toUnaryOperation(current());
        consume();
        PARSE(expression, SingularExpression);
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
    // FIXME: add the case for array/vector/matrix access

    for (;;) {
        switch (current().type) {
        case TokenType::BracketLeft: {
            consume();
            PARSE(arrayIndex, Expression);
            CONSUME_TYPE(BracketRight);
            // FIXME: Replace with NODE_REF(...)
            SourceSpan span(startPosition, m_lexer.currentPosition());
            expr = m_builder.construct<AST::IndexAccessExpression>(span, WTFMove(expr), WTFMove(arrayIndex));
            break;
        }

        case TokenType::Period: {
            consume();
            PARSE(fieldName, Identifier);
            // FIXME: Replace with NODE_REF(...)
            SourceSpan span(startPosition, m_lexer.currentPosition());
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
        // FIXME: WGSL grammar has an ambiguity when trying to distinguish the
        // use of < as either the less-than operator or the beginning of a
        // template-parameter list. Here we are checking for vector or matrix
        // type names. Alternatively, those names could be turned into keywords
        auto typePrefix = AST::ParameterizedTypeName::stringViewToKind(ident.id());
        if ((typePrefix && current().type == TokenType::Lt) || current().type == TokenType::ParenLeft) {
            PARSE(type, TypeNameAfterIdentifier, WTFMove(ident), _startOfElementPosition);
            PARSE(arguments, ArgumentExpressionList);
            RETURN_ARENA_NODE(CallExpression, WTFMove(type), WTFMove(arguments));
        }
        RETURN_ARENA_NODE(IdentifierExpression, WTFMove(ident));
    }
    case TokenType::KeywordI32:
    case TokenType::KeywordU32:
    case TokenType::KeywordF32:
    case TokenType::KeywordBool: {
        PARSE(type, TypeName);
        PARSE(arguments, ArgumentExpressionList);
        RETURN_ARENA_NODE(CallExpression, WTFMove(type), WTFMove(arguments));
    }
    case TokenType::KeywordArray: {
        PARSE(arrayType, ArrayType);
        PARSE(arguments, ArgumentExpressionList);
        RETURN_ARENA_NODE(CallExpression, WTFMove(arrayType), WTFMove(arguments));
    }

    // const_literal
    case TokenType::LiteralTrue:
        consume();
        RETURN_ARENA_NODE(BoolLiteral, true);
    case TokenType::LiteralFalse:
        consume();
        RETURN_ARENA_NODE(BoolLiteral, false);
    case TokenType::IntegerLiteral: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteral);
        RETURN_ARENA_NODE(AbstractIntegerLiteral, lit.literalValue);
    }
    case TokenType::IntegerLiteralSigned: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteralSigned);
        RETURN_ARENA_NODE(Signed32Literal, lit.literalValue);
    }
    case TokenType::IntegerLiteralUnsigned: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteralUnsigned);
        RETURN_ARENA_NODE(Unsigned32Literal, lit.literalValue);
    }
    case TokenType::AbstractFloatLiteral: {
        CONSUME_TYPE_NAMED(lit, AbstractFloatLiteral);
        RETURN_ARENA_NODE(AbstractFloatLiteral, lit.literalValue);
    }
    case TokenType::FloatLiteral: {
        CONSUME_TYPE_NAMED(lit, FloatLiteral);
        RETURN_ARENA_NODE(Float32Literal, lit.literalValue);
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
