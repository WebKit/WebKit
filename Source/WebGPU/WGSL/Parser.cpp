/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "Parser.h"
#include "config.h"

#include "AST/Attribute.h"
#include "AST/Expression.h"
#include "AST/Expressions/IdentifierExpression.h"
#include "AST/Expressions/LiteralExpressions.h"
#include "AST/Expressions/StructureAccess.h"
#include "AST/Expressions/TypeConversion.h"
#include "AST/GlobalDecl.h"
#include "AST/Statement.h"
#include "AST/Statements/AssignmentStatement.h"
#include "AST/Statements/CompoundStatement.h"
#include "AST/Statements/ReturnStatement.h"
#include "AST/StructureDecl.h"
#include "Lexer.h"
#include <wtf/text/StringBuilder.h>

namespace WGSL {

#define START_PARSE() \
    auto _startOfElementPosition = m_lexer.currentPosition();

#define CURRENT_SOURCE_SPAN() \
    SourceSpan(_startOfElementPosition, m_lexer.currentPosition())

#define RETURN_NODE(type, ...) \
    do { \
        AST::type astNodeResult(CURRENT_SOURCE_SPAN(), __VA_ARGS__); \
        return { WTFMove(astNodeResult) }; \
    } while (false)

// Passing 0 arguments beyond the type to RETURN_NODE is invalid because of a stupid limitation of the C preprocessor
#define RETURN_NODE_NO_ARGS(type) \
    do { \
        AST::type astNodeResult(CURRENT_SOURCE_SPAN()); \
        return { WTFMove(astNodeResult) }; \
    } while (false)

#define RETURN_NODE_REF(type, ...) \
    return { makeUniqueRef<AST::type>(CURRENT_SOURCE_SPAN(), __VA_ARGS__) };

// Passing 0 arguments beyond the type to RETURN_NODE_REF is invalid because of a stupid limitation of the C preprocessor
#define RETURN_NODE_REF_NO_ARGS(type) \
    return { makeUniqueRef<AST::type>(CURRENT_SOURCE_SPAN()) };

#define FAIL(string) \
    return makeUnexpected(Error(string, CURRENT_SOURCE_SPAN()));

// Warning: cannot use the do..while trick because it defines a new identifier named `name`.
// So do not use after an if/for/while without braces.
#define PARSE(name, element, ...) \
    auto name##Expected = parse##element(__VA_ARGS__); \
    if (!name##Expected) \
        return makeUnexpected(name##Expected.error()); \
    auto& name = *name##Expected;

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

template<typename Lexer>
class Parser {
public:
    Parser(Lexer& lexer)
        : m_lexer(lexer)
        , m_current(lexer.lex())
    {
    }

    Expected<AST::ShaderModule, Error> parseShader();

private:
    // UniqueRef whenever it can return multiple types.
    Expected<UniqueRef<AST::GlobalDecl>, Error> parseGlobalDecl();
    Expected<AST::Attributes, Error> parseAttributes();
    Expected<UniqueRef<AST::Attribute>, Error> parseAttribute();
    Expected<AST::StructDecl, Error> parseStructDecl(AST::Attributes&&);
    Expected<AST::StructMember, Error> parseStructMember();
    Expected<UniqueRef<AST::TypeDecl>, Error> parseTypeDecl();
    Expected<UniqueRef<AST::TypeDecl>, Error> parseTypeDeclAfterIdentifier(StringView&&, SourcePosition start);
    Expected<AST::GlobalVariableDecl, Error> parseGlobalVariableDecl(AST::Attributes&&);
    Expected<AST::VariableQualifier, Error> parseVariableQualifier();
    Expected<AST::StorageClass, Error> parseStorageClass();
    Expected<AST::AccessMode, Error> parseAccessMode();
    Expected<AST::FunctionDecl, Error> parseFunctionDecl(AST::Attributes&&);
    Expected<AST::Parameter, Error> parseParameter();
    Expected<UniqueRef<AST::Statement>, Error> parseStatement();
    Expected<AST::CompoundStatement, Error> parseCompoundStatement();
    Expected<AST::ReturnStatement, Error> parseReturnStatement();
    Expected<UniqueRef<AST::Expression>, Error> parseShortCircuitOrExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseRelationalExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseShiftExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseAdditiveExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseMultiplicativeExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseUnaryExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseSingularExpression();
    Expected<UniqueRef<AST::Expression>, Error> parsePostfixExpression(UniqueRef<AST::Expression>&& base, SourcePosition startPosition);
    Expected<UniqueRef<AST::Expression>, Error> parsePrimaryExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseLHSExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseCoreLHSExpression();
    Expected<Vector<UniqueRef<AST::Expression>>, Error> parseArgumentExpressionList();

    Expected<Token, TokenType> consumeType(TokenType);
    void consume();

    Token& current() { return m_current; }

    Lexer& m_lexer;
    Token m_current;
};

template<typename Lexer>
Expected<AST::ShaderModule, Error> parse(const String& wgsl)
{
    Lexer lexer(wgsl);
    Parser parser(lexer);
    
    return parser.parseShader();
}

Expected<AST::ShaderModule, Error> parseLChar(const String& wgsl)
{
    return parse<Lexer<LChar>>(wgsl);
}

Expected<AST::ShaderModule, Error> parseUChar(const String& wgsl)
{
    return parse<Lexer<UChar>>(wgsl);
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
void Parser<Lexer>::consume()
{
    m_current = m_lexer.lex();
}

template<typename Lexer>
Expected<AST::ShaderModule, Error> Parser<Lexer>::parseShader()
{
    START_PARSE();

    Vector<UniqueRef<AST::GlobalDirective>> directives;
    // FIXME: parse directives here.

    Vector<UniqueRef<AST::GlobalDecl>> decls;
    while (!m_lexer.isAtEndOfFile()) {
        PARSE(globalDecl, GlobalDecl)
        decls.append(WTFMove(globalDecl));
    }

    RETURN_NODE(ShaderModule, WTFMove(directives), WTFMove(decls));
}

template<typename Lexer>
Expected<UniqueRef<AST::GlobalDecl>, Error> Parser<Lexer>::parseGlobalDecl()
{
    START_PARSE();

    PARSE(attributes, Attributes);

    switch (current().m_type) {
    case TokenType::KeywordStruct: {
        PARSE(structDecl, StructDecl, WTFMove(attributes));
        return { makeUniqueRef<AST::StructDecl>(WTFMove(structDecl)) };
    }
    case TokenType::KeywordVar: {
        PARSE(varDecl, GlobalVariableDecl, WTFMove(attributes));
        CONSUME_TYPE(Semicolon);
        return { makeUniqueRef<AST::GlobalVariableDecl>(WTFMove(varDecl)) };
    }
    case TokenType::KeywordFn: {
        PARSE(fn, FunctionDecl, WTFMove(attributes));
        return { makeUniqueRef<AST::FunctionDecl>(WTFMove(fn)) };
    }
    default:
        FAIL("Trying to parse a GlobalDecl, expected 'var', 'fn', or 'struct'."_s);
    }
}

template<typename Lexer>
Expected<AST::Attributes, Error> Parser<Lexer>::parseAttributes()
{
    AST::Attributes attributes;

    while (current().m_type == TokenType::Attribute) {
        PARSE(firstAttribute, Attribute);
        attributes.append(WTFMove(firstAttribute));
    }

    return { WTFMove(attributes) };
}

template<typename Lexer>
Expected<UniqueRef<AST::Attribute>, Error> Parser<Lexer>::parseAttribute()
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
    if (ident.m_ident == "stage"_s) {
        CONSUME_TYPE(ParenLeft);
        CONSUME_TYPE_NAMED(stage, Identifier);
        CONSUME_TYPE(ParenRight);
        if (stage.m_ident == "compute"_s)
            RETURN_NODE_REF(StageAttribute, AST::StageAttribute::Stage::Compute);
        if (stage.m_ident == "vertex"_s)
            RETURN_NODE_REF(StageAttribute, AST::StageAttribute::Stage::Vertex);
        if (stage.m_ident == "fragment"_s)
            RETURN_NODE_REF(StageAttribute, AST::StageAttribute::Stage::Fragment);
        FAIL("Invalid stage attribute, the only options are 'compute', 'vertex', 'fragment'."_s);
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
        CONSUME_TYPE_NAMED(name, Identifier);
        CONSUME_TYPE(ParenRight);
        RETURN_NODE_REF(BuiltinAttribute, name.m_ident);
    }
    FAIL("Unknown attribute, the only supported attributes are 'group', 'binding', 'stage'."_s);
}

template<typename Lexer>
Expected<AST::StructDecl, Error> Parser<Lexer>::parseStructDecl(AST::Attributes&& attributes)
{
    START_PARSE();

    CONSUME_TYPE(KeywordStruct);
    CONSUME_TYPE_NAMED(name, Identifier);
    CONSUME_TYPE(BraceLeft);

    Vector<UniqueRef<AST::StructMember>> members;
    while (current().m_type != TokenType::BraceRight) {
        PARSE(member, StructMember);
        members.append(makeUniqueRef<AST::StructMember>(WTFMove(member)));
    }

    CONSUME_TYPE(BraceRight);

    RETURN_NODE(StructDecl, name.m_ident, WTFMove(members), WTFMove(attributes));
}

template<typename Lexer>
Expected<AST::StructMember, Error> Parser<Lexer>::parseStructMember()
{
    START_PARSE();

    PARSE(attributes, Attributes);
    CONSUME_TYPE_NAMED(name, Identifier);
    CONSUME_TYPE(Colon);
    PARSE(type, TypeDecl);
    CONSUME_TYPE(Semicolon);

    RETURN_NODE(StructMember, name.m_ident, WTFMove(type), WTFMove(attributes));
}

template<typename Lexer>
Expected<UniqueRef<AST::TypeDecl>, Error> Parser<Lexer>::parseTypeDecl()
{
    START_PARSE();

    if (current().m_type == TokenType::KeywordI32) {
        consume();
        RETURN_NODE_REF(NamedType, StringView { "i32"_s });
    }
    if (current().m_type == TokenType::KeywordF32) {
        consume();
        RETURN_NODE_REF(NamedType, StringView { "f32"_s });
    }
    if (current().m_type == TokenType::KeywordU32) {
        consume();
        RETURN_NODE_REF(NamedType, StringView { "u32"_s });
    }
    if (current().m_type == TokenType::KeywordBool) {
        consume();
        RETURN_NODE_REF(NamedType, StringView { "bool"_s });
    }
    if (current().m_type == TokenType::Identifier) {
        CONSUME_TYPE_NAMED(name, Identifier);
        return parseTypeDeclAfterIdentifier(WTFMove(name.m_ident), _startOfElementPosition);
    }

    FAIL("Tried parsing a type and it did not start with an identifier"_s);
}

template<typename Lexer>
Expected<UniqueRef<AST::TypeDecl>, Error> Parser<Lexer>::parseTypeDeclAfterIdentifier(StringView&& name, SourcePosition _startOfElementPosition)
{
    if (auto kind = AST::ParameterizedType::stringViewToKind(name)) {
        CONSUME_TYPE(LT);
        PARSE(elementType, TypeDecl);
        CONSUME_TYPE(GT);
        RETURN_NODE_REF(ParameterizedType, *kind, WTFMove(elementType));
    }
    RETURN_NODE_REF(NamedType, WTFMove(name));
}

template<typename Lexer>
Expected<AST::GlobalVariableDecl, Error> Parser<Lexer>::parseGlobalVariableDecl(AST::Attributes&& attributes)
{
    START_PARSE();

    CONSUME_TYPE(KeywordVar);

    std::unique_ptr<AST::VariableQualifier> maybeQualifier = nullptr;
    if (current().m_type == TokenType::LT) {
        PARSE(variableQualifier, VariableQualifier);
        maybeQualifier = WTF::makeUnique<AST::VariableQualifier>(WTFMove(variableQualifier));
    }

    CONSUME_TYPE_NAMED(name, Identifier);

    std::unique_ptr<AST::TypeDecl> maybeType = nullptr;
    if (current().m_type == TokenType::Colon) {
        consume();
        PARSE(typeDecl, TypeDecl);
        maybeType = typeDecl.moveToUniquePtr();
    }

    std::unique_ptr<AST::Expression> maybeInitializer = nullptr;
    // FIXME: initializer

    RETURN_NODE(GlobalVariableDecl, name.m_ident, WTFMove(maybeQualifier), WTFMove(maybeType), WTFMove(maybeInitializer), WTFMove(attributes));
}

template<typename Lexer>
Expected<AST::VariableQualifier, Error> Parser<Lexer>::parseVariableQualifier()
{
    START_PARSE();

    CONSUME_TYPE(LT);
    PARSE(storageClass, StorageClass);

    // FIXME: verify that Read is the correct default in all cases.
    AST::AccessMode accessMode = AST::AccessMode::Read;
    if (current().m_type == TokenType::Comma) {
        consume();
        PARSE(actualAccessMode, AccessMode);
        accessMode = actualAccessMode;
    }

    CONSUME_TYPE(GT);

    RETURN_NODE(VariableQualifier, storageClass, accessMode);
}

template<typename Lexer>
Expected<AST::StorageClass, Error> Parser<Lexer>::parseStorageClass()
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
Expected<AST::AccessMode, Error> Parser<Lexer>::parseAccessMode()
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
Expected<AST::FunctionDecl, Error> Parser<Lexer>::parseFunctionDecl(AST::Attributes&& attributes)
{
    START_PARSE();

    CONSUME_TYPE(KeywordFn);
    CONSUME_TYPE_NAMED(name, Identifier);

    CONSUME_TYPE(ParenLeft);
    Vector<UniqueRef<AST::Parameter>> parameters;
    while (current().m_type != TokenType::ParenRight) {
        PARSE(parameter, Parameter);
        parameters.append(makeUniqueRef<AST::Parameter>(WTFMove(parameter)));
    }
    CONSUME_TYPE(ParenRight);

    AST::Attributes returnAttributes;
    std::unique_ptr<AST::TypeDecl> maybeReturnType = nullptr;
    if (current().m_type == TokenType::Arrow) {
        consume();
        PARSE(parsedReturnAttributes, Attributes);
        returnAttributes = WTFMove(parsedReturnAttributes);
        PARSE(type, TypeDecl);
        maybeReturnType = type.moveToUniquePtr();
    }

    PARSE(body, CompoundStatement);

    RETURN_NODE(FunctionDecl, name.m_ident, WTFMove(parameters), WTFMove(maybeReturnType), WTFMove(body), WTFMove(attributes), WTFMove(returnAttributes));
}

template<typename Lexer>
Expected<AST::Parameter, Error> Parser<Lexer>::parseParameter()
{
    START_PARSE();

    PARSE(attributes, Attributes);
    CONSUME_TYPE_NAMED(name, Identifier)
    CONSUME_TYPE(Colon);
    PARSE(type, TypeDecl);

    RETURN_NODE(Parameter, name.m_ident, WTFMove(type), WTFMove(attributes));
}

template<typename Lexer>
Expected<UniqueRef<AST::Statement>, Error> Parser<Lexer>::parseStatement()
{
    START_PARSE();

    switch (current().m_type) {
    case TokenType::BraceLeft: {
        PARSE(compoundStmt, CompoundStatement);
        return { makeUniqueRef<AST::CompoundStatement>(WTFMove(compoundStmt)) };
    }
    case TokenType::Semicolon: {
        consume();
        Vector<UniqueRef<AST::Statement>> statements;
        return { makeUniqueRef<AST::CompoundStatement>(CURRENT_SOURCE_SPAN(), WTFMove(statements)) };
    }
    case TokenType::KeywordReturn: {
        PARSE(returnStmt, ReturnStatement);
        CONSUME_TYPE(Semicolon);
        return { makeUniqueRef<AST::ReturnStatement>(WTFMove(returnStmt)) };
    }
    case TokenType::Identifier: {
        // FIXME: there will be other cases here eventually for function calls
        PARSE(lhs, LHSExpression);
        CONSUME_TYPE(Equal);
        PARSE(rhs, Expression);
        CONSUME_TYPE(Semicolon);
        RETURN_NODE_REF(AssignmentStatement, lhs.moveToUniquePtr(), WTFMove(rhs));
    }
    default:
        FAIL("Not a valid statement"_s);
    }
}

template<typename Lexer>
Expected<AST::CompoundStatement, Error> Parser<Lexer>::parseCompoundStatement()
{
    START_PARSE();

    CONSUME_TYPE(BraceLeft);

    Vector<UniqueRef<AST::Statement>> statements;
    while (current().m_type != TokenType::BraceRight) {
        PARSE(stmt, Statement);
        statements.append(WTFMove(stmt));
    }

    CONSUME_TYPE(BraceRight);

    RETURN_NODE(CompoundStatement, WTFMove(statements));
}

template<typename Lexer>
Expected<AST::ReturnStatement, Error> Parser<Lexer>::parseReturnStatement()
{
    START_PARSE();

    CONSUME_TYPE(KeywordReturn);

    if (current().m_type == TokenType::Semicolon) {
        RETURN_NODE(ReturnStatement, { });
    }

    PARSE(expr, ShortCircuitOrExpression);
    RETURN_NODE(ReturnStatement, expr.moveToUniquePtr());
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseShortCircuitOrExpression()
{
    // FIXME: fill in
    return parseRelationalExpression();
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseRelationalExpression()
{
    // FIXME: fill in
    return parseShiftExpression();
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseShiftExpression()
{
    // FIXME: fill in
    return parseAdditiveExpression();
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseAdditiveExpression()
{
    // FIXME: fill in
    return parseMultiplicativeExpression();
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseMultiplicativeExpression()
{
    // FIXME: fill in
    return parseUnaryExpression();
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseUnaryExpression()
{
    // FIXME: fill in
    return parseSingularExpression();
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseSingularExpression()
{
    START_PARSE();
    PARSE(base, PrimaryExpression);
    return parsePostfixExpression(WTFMove(base), _startOfElementPosition);
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parsePostfixExpression(UniqueRef<AST::Expression>&& base, SourcePosition startPosition)
{
    START_PARSE();

    UniqueRef<AST::Expression> expr = WTFMove(base);
    // FIXME: add the case for array/vector/matrix access
    while (current().m_type == TokenType::Period) {
        consume();
        CONSUME_TYPE_NAMED(fieldName, Identifier);
        SourceSpan span(startPosition, m_lexer.currentPosition());
        expr = makeUniqueRef<AST::StructureAccess>(span, WTFMove(expr), fieldName.m_ident);
    }

    return { WTFMove(expr) };
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parsePrimaryExpression()
{
    START_PARSE();

    switch (current().m_type) {
    case TokenType::ParenLeft: {
        consume();
        PARSE(expr, Expression);
        CONSUME_TYPE(ParenRight);
        return { WTFMove(expr) };
    }
    case TokenType::Identifier: {
        CONSUME_TYPE_NAMED(ident, Identifier);
        if (ident.m_ident == "true"_s) {
            RETURN_NODE_REF(BoolLiteral, true);
        }
        if (ident.m_ident == "false"_s) {
            RETURN_NODE_REF(BoolLiteral, false);
        }
        if (current().m_type == TokenType::LT || current().m_type == TokenType::ParenLeft) {
            PARSE(type, TypeDeclAfterIdentifier, WTFMove(ident.m_ident), _startOfElementPosition);
            PARSE(arguments, ArgumentExpressionList);
            RETURN_NODE_REF(TypeConversion, WTFMove(type), WTFMove(arguments));
        }
        RETURN_NODE_REF(IdentifierExpression, ident.m_ident);
    }
    case TokenType::IntegerLiteralSigned: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteralSigned);
        RETURN_NODE_REF(Int32Literal, lit.m_literalValue);
    }
    case TokenType::IntegerLiteralUnsigned: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteralUnsigned);
        RETURN_NODE_REF(Uint32Literal, lit.m_literalValue);
    }
    case TokenType::DecimalFloatLiteral: {
        CONSUME_TYPE_NAMED(lit, DecimalFloatLiteral);
        RETURN_NODE_REF(Float32Literal, lit.m_literalValue);
    }
    // FIXME: HexFloatLiteral and IntegerLiteral
    default:
        break;
    }
    FAIL("Expected one of '(', a literal, or an identifier"_s);
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseExpression()
{
    // FIXME: Fill in
    return parseRelationalExpression();
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseLHSExpression()
{
    START_PARSE();

    // FIXME: Add the possibility of a prefix
    PARSE(base, CoreLHSExpression);
    return parsePostfixExpression(WTFMove(base), _startOfElementPosition);
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseCoreLHSExpression()
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
        CONSUME_TYPE_NAMED(ident, Identifier);
        RETURN_NODE_REF(IdentifierExpression, ident.m_ident);
    }
    default:
        break;
    }

    FAIL("Tried to parse the left-hand side of an assignment and failed"_s);
}

template<typename Lexer>
Expected<Vector<UniqueRef<AST::Expression>>, Error> Parser<Lexer>::parseArgumentExpressionList()
{
    START_PARSE();
    CONSUME_TYPE(ParenLeft);

    Vector<UniqueRef<AST::Expression>> arguments;
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
