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

#include "config.h"
#include "Parser.h"

#include "AST/Attribute.h"
#include "AST/Expression.h"
#include "AST/Expressions/BinaryExpression.h"
#include "AST/Expressions/CallableExpression.h"
#include "AST/Expressions/IdentifierExpression.h"
#include "AST/Expressions/LiteralExpressions.h"
#include "AST/Expressions/StructureAccess.h"
#include "AST/Expressions/UnaryExpression.h"
#include "AST/GlobalDecl.h"
#include "AST/Statement.h"
#include "AST/Statements/AssignmentStatement.h"
#include "AST/Statements/CompoundStatement.h"
#include "AST/Statements/ReturnStatement.h"
#include "AST/StructureDecl.h"
#include "Lexer.h"
#include "ParserPrivate.h"
#include <wtf/text/StringBuilder.h>

namespace WGSL {

#define START_PARSE() \
    auto _startOfElementPosition = currentStartSourcePosition()

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
    return { makeUniqueRef<AST::type>(CURRENT_SOURCE_SPAN(), ##__VA_ARGS__) };

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
        // Explaining this: the current position is at the end of the current token,
        // and also the start position of the next token.
        // When we lex another token, then this current position() becomes the start position
        // of the current token.
        m_currentStartSourcePosition = m_lexer.currentPosition();
        m_current = m_lexer.lex();
        return result;
    }
    return makeUnexpected(current().m_type);
}

template<typename Lexer>
void Parser<Lexer>::consume()
{
    // Explaining this: the current position is at the end of the current token,
    // and also the start position of the next token.
    // When we lex another token, then this current position() becomes the start position
    // of the current token.
    m_currentStartSourcePosition = m_lexer.currentPosition();
    m_current = m_lexer.lex();
}

// https://gpuweb.github.io/gpuweb/wgsl/#syntax-translation_unit
// translation_unit :
//   | global_directive * global_decl *
// global_decl :
//   | semicolon
//   | global_variable_decl semicolon
//   | global_constant_decl semicolon
//   | type_alias_decl semicolon
//   | struct_decl
//   | function_decl
//   | static_assert_statement semicolon
template<typename Lexer>
Expected<AST::ShaderModule, Error> Parser<Lexer>::parseShader()
{
    START_PARSE();

    // global_directive *
    Vector<AST::GlobalDirective> directives;
    // FIXME: parse directives here.

    // global_decl *
    Vector<AST::StructDecl> structs;
    Vector<AST::VariableDecl> globalVars;
    Vector<AST::FunctionDecl> functions;

    while (!m_lexer.isAtEndOfFile()) {
        // semicolon.
        if (current().m_type == TokenType::Semicolon)
            continue;

        PARSE(attributes, Attributes);

        switch (current().m_type) {
        case TokenType::KeywordVar: {
            PARSE(globalVarDecl, VariableDecl, WTFMove(attributes));
            globalVars.append(WTFMove(globalVarDecl));
            break;
        }

        case TokenType::KeywordFn: {
            PARSE(function, FunctionDecl, WTFMove(attributes));
            functions.append(WTFMove(function));
            break;
        }

        case TokenType::KeywordStruct: {
            if (!attributes.isEmpty())
                FAIL("struct definition does not accept attributes"_s);
            
            PARSE(structure, StructDecl);
            structs.append(WTFMove(structure));
            break;
        }

        // TODO: global_constant_decl semicolon
        // TODO: type_alias_decl semicolon
        // TODO: static_assert_statement semicolon

        default:
            FAIL("Trying to parse a GlobalDecl, expected 'var', 'fn', or 'struct'."_s);
        }
    }

    RETURN_NODE(ShaderModule, WTFMove(directives), WTFMove(structs), WTFMove(globalVars), WTFMove(functions));
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
Expected<AST::StructDecl, Error> Parser<Lexer>::parseStructDecl()
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

    RETURN_NODE(StructDecl, name.m_ident, WTFMove(members));
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

// https://gpuweb.github.io/gpuweb/wgsl/#syntax-type_decl
// type_decl :
//   | ident
//   | bool
//   | float32
//   | float16
//   | int32
//   | uint32
//   | vec_prefix less_than type_decl greater_than
//   | mat_prefix less_than type_decl greater_than
//   | pointer less_than address_space comma type_decl ( comma access_mode ) ? greater_than
//   | array_type_decl
//   | atomic less_than type_decl greater_than
//   | texture_sampler_types
template<typename Lexer>
Expected<UniqueRef<AST::TypeDecl>, Error> Parser<Lexer>::parseTypeDecl()
{
    START_PARSE();

    switch (current().m_type) {
    case TokenType::Identifier: {
        CONSUME_TYPE_NAMED(ident, Identifier);
        RETURN_NODE_REF(NamedType, WTFMove(ident.m_ident));
    }

    case TokenType::KeywordBool:
        consume();
        RETURN_NODE_REF(NamedType, "bool"_s);

    case TokenType::KeywordF32:
        consume();
        RETURN_NODE_REF(NamedType, "f32"_s);

    // TODO: float16

    case TokenType::KeywordI32:
        consume();
        RETURN_NODE_REF(NamedType, "i32"_s);

    case TokenType::KeywordU32:
        consume();
        RETURN_NODE_REF(NamedType, "u32"_s);

    case TokenType::KeywordVec2:
    case TokenType::KeywordVec3:
    case TokenType::KeywordVec4: {
        uint8_t size = 0;
        switch (current().m_type) {
        case TokenType::KeywordVec2:
            size = 2;
            break;
        case TokenType::KeywordVec3:
            size = 3;
            break;
        case TokenType::KeywordVec4:
            size = 4;
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        consume();

        CONSUME_TYPE(LT);
        PARSE(elementType, TypeDecl);
        CONSUME_TYPE(GT);

        RETURN_NODE_REF(VecType, WTFMove(elementType), size);
    }

    // TODO: matrix
    // TODO: ptr<>

    // array_type_decl :
    //   | array less_than type_decl ( comma element_count_expression ) ? greater_than
    case TokenType::KeywordArray: {
        consume();

        CONSUME_TYPE(LT);
        PARSE(elementType, TypeDecl);

        if (current().m_type == TokenType::Comma) {
            CONSUME_TYPE(Comma);
            PARSE(sizeExpression, Expression);
            CONSUME_TYPE(GT);
            RETURN_NODE_REF(ArrayType, WTFMove(elementType), WTFMove(sizeExpression));
        }

        CONSUME_TYPE(GT);
        RETURN_NODE_REF(ArrayType, WTFMove(elementType));
    }

    // TODO: atomic
    // TODO: texture types

    default:
        FAIL("Unable to parse type declaration"_s);
    }
}

// https://gpuweb.github.io/gpuweb/wgsl/#syntax-variable_decl
// Grammar is extended a bit to accomodate initialization expression.
// variable_decl :
//   | var variable_qualifier ? ident ( colon type_decl ) ? ( equal expression )?
template<typename Lexer>
Expected<AST::VariableDecl, Error> Parser<Lexer>::parseVariableDecl(AST::Attributes attributes)
{
    START_PARSE();

    CONSUME_TYPE(KeywordVar);

    std::optional<AST::VariableQualifier> maybeQualifier;
    {
        auto result = parseVariableQualifier();
        if (result)
            maybeQualifier = *result;
    }

    CONSUME_TYPE_NAMED(name, Identifier);

    std::unique_ptr<AST::TypeDecl> maybeType = nullptr;
    if (current().m_type == TokenType::Colon) {
        consume();
        PARSE(typeDecl, TypeDecl);
        maybeType = typeDecl.moveToUniquePtr();
    }

    std::unique_ptr<AST::Expression> maybeInitializer = nullptr;
    if (current().m_type == TokenType::Equal) {
        consume();
        PARSE(initializer, Expression);
        maybeInitializer = initializer.moveToUniquePtr();
    }

    RETURN_NODE(VariableDecl, AST::VariableDecl::Kind::Var, name.m_ident, WTFMove(maybeQualifier), WTFMove(maybeType), WTFMove(maybeInitializer), WTFMove(attributes));
}

// https://gpuweb.github.io/gpuweb/wgsl/#syntax-variable_qualifier
// variable_qualifier :
//   | less_than address_space ( comma access_mode ) ? greater_than
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

// https://gpuweb.github.io/gpuweb/wgsl/#syntax-function_decl
// function_decl :
//   | function_header compound_statement
template<typename Lexer>
Expected<AST::FunctionDecl, Error> Parser<Lexer>::parseFunctionDecl(AST::Attributes attributes)
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

// statement :
//   | semicolon
//   | return_statement semicolon
//   | if_statement
//   | switch_statement
//   | loop_statement
//   | for_statement
//   | while_statement
//   | func_call_statement semicolon
//   | variable_statement semicolon
//   | break_statement semicolon
//   | continue_statement semicolon
//   | discard semicolon
//   | assignment_statement semicolon
//   | compound_statement
//   | increment_statement semicolon
//   | decrement_statement semicolon
//   | static_assert_statement semicolon
template<typename Lexer>
Expected<UniqueRef<AST::Statement>, Error> Parser<Lexer>::parseStatement()
{
    START_PARSE();

    switch (current().m_type) {
    // semicolon
    case TokenType::Semicolon: {
        consume();
        Vector<UniqueRef<AST::Statement>> statements;
        return { makeUniqueRef<AST::CompoundStatement>(CURRENT_SOURCE_SPAN(), WTFMove(statements)) };
    }

    // return_statement semicolon
    case TokenType::KeywordReturn: {
        PARSE(returnStmt, ReturnStatement);
        CONSUME_TYPE(Semicolon);
        return { makeUniqueRef<AST::ReturnStatement>(WTFMove(returnStmt)) };
    }

    // TODO: if_statement
    // TODO: switch_statement
    // TODO: loop_statement
    // TODO: for_statement
    // TODO: while_statement
    // TODO: func_call_statement semicolon

    // variable_statement semicolon
    // TODO: missing let and const.
    case TokenType::KeywordVar: {
        PARSE(decl, VariableDecl, { });
        CONSUME_TYPE(Semicolon);
        RETURN_NODE_REF(VariableStatement, WTFMove(decl));
    }

    // TODO: break_statement semicolon
    // TODO: continue_statement semicolon
    // TODO: discard semicolon

    // assignment_statement semicolon
    case TokenType::Identifier: {
        // FIXME: there will be other cases here eventually for function calls
        PARSE(lhs, LHSExpression);
        CONSUME_TYPE(Equal);
        PARSE(rhs, Expression);
        CONSUME_TYPE(Semicolon);
        RETURN_NODE_REF(AssignmentStatement, lhs.moveToUniquePtr(), WTFMove(rhs));
    }

    // compound_statement
    case TokenType::BraceLeft: {
        PARSE(compoundStmt, CompoundStatement);
        return { makeUniqueRef<AST::CompoundStatement>(WTFMove(compoundStmt)) };
    }

    // TODO: increment_statement semicolon
    // TODO: decrement_statement semicolon
    // TODO: static_assert_statement semicolon

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
    START_PARSE();

    auto initialExpr = parseMultiplicativeExpression();
    if (!initialExpr)
        return makeUnexpected(initialExpr.error());
    
    auto expr = WTFMove(*initialExpr);

    while (true) {
        AST::BinaryExpression::Op op = AST::BinaryExpression::Op::Add;
        switch (current().m_type) {
        case TokenType::Plus:
            op = AST::BinaryExpression::Op::Add;
            break;
        case TokenType::Minus:
            op = AST::BinaryExpression::Op::Subtract;
            break;
        default:
            return { WTFMove(expr) };
        }

        consume();
        
        auto nextExpr = parseMultiplicativeExpression();
        if (!nextExpr)
            return makeUnexpected(nextExpr.error());

        expr = makeUniqueRef<AST::BinaryExpression>(CURRENT_SOURCE_SPAN(), WTFMove(expr), op, WTFMove(*nextExpr));
    }

    return { WTFMove(expr) };
}

// multiplicative_expression := unary_expression ( [*/%] unary_expression )*
template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseMultiplicativeExpression()
{
    START_PARSE();

    auto initialUnaryExpr = parseUnaryExpression();
    if (!initialUnaryExpr)
        return makeUnexpected(initialUnaryExpr.error());
    
    auto expr = WTFMove(*initialUnaryExpr);

    while (true) {
        AST::BinaryExpression::Op op = AST::BinaryExpression::Op::Multiply;
        switch (current().m_type) {
        case TokenType::Star:
            op = AST::BinaryExpression::Op::Multiply;
            break;
        case TokenType::ForwardSlash:
            op = AST::BinaryExpression::Op::Divide;
            break;
        case TokenType::Modulo:
            op = AST::BinaryExpression::Op::Modulo;
            break;
        default:
            return { WTFMove(expr) };
        }

        consume();
        
        auto nextExpr = parseUnaryExpression();
        if (!nextExpr)
            return makeUnexpected(nextExpr.error());

        expr = makeUniqueRef<AST::BinaryExpression>(CURRENT_SOURCE_SPAN(), WTFMove(expr), op, WTFMove(*nextExpr));
    }

    return { WTFMove(expr) };
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseUnaryExpression()
{
    START_PARSE();

    AST::UnaryExpression::Op op = AST::UnaryExpression::Op::Negation;
    switch (current().m_type) {
    case TokenType::Minus:
        op = AST::UnaryExpression::Op::Negation;
        break;
    case TokenType::Bang:
        op = AST::UnaryExpression::Op::LogicalNot;
        break;
    case TokenType::Tilde:
        op = AST::UnaryExpression::Op::BitwiseNot;
        break;
    case TokenType::Star:
        op = AST::UnaryExpression::Op::Indirection;
        break;
    case TokenType::And:
        op = AST::UnaryExpression::Op::AddressOf;
        break;
    default:
        return parseSingularExpression();
    }

    consume();
    auto innerUnaryExpr = parseUnaryExpression();
    if (innerUnaryExpr) {
        RETURN_NODE_REF(UnaryExpression, op, WTFMove(*innerUnaryExpr));
    } else {
        return makeUnexpected(innerUnaryExpr.error());
    }
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseSingularExpression()
{
    START_PARSE();
    PARSE(base, PrimaryExpression);
    return parsePostfixExpression(WTFMove(base), _startOfElementPosition);
}

// postfix_expression:
//   | bracket_left expression bracket_right postfix_expression?
//   | period ident postfix_expression?
// This can be translated to
// postfix_expression 
//    := ( (bracket_left expression bracket_right) | (period ident) ) *
template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parsePostfixExpression(UniqueRef<AST::Expression>&& base, SourcePosition startPosition)
{
    START_PARSE();

    auto expr = WTFMove(base);

    while (true) {
        if (current().m_type == TokenType::Period) {
            // Structure access.

            consume();
            CONSUME_TYPE_NAMED(fieldName, Identifier);
            SourceSpan span(startPosition, m_lexer.currentPosition());
            expr = makeUniqueRef<AST::StructureAccess>(span, WTFMove(expr), fieldName.m_ident);
        } else if (current().m_type == TokenType::BracketLeft) {
            // Subscript (accessing elements in arrays/vectors/matrices)

            consume();
            PARSE(subscriptExpr, Expression);
            expr = makeUniqueRef<AST::BinaryExpression>(CURRENT_SOURCE_SPAN(), WTFMove(expr), AST::BinaryExpression::Op::Subscript, WTFMove(subscriptExpr));
            CONSUME_TYPE(BracketRight);
        } else {
            break;
        }
    }

    return { WTFMove(expr) };
}

// https://gpuweb.github.io/gpuweb/wgsl/#syntax-primary_expression
// primary_expression:
//   | ident
//   | callable argument_expression_list
//   | const_literal
//   | paren_expression
//   | bitcast less_than type_decl greater_than paren_expression
template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parsePrimaryExpression()
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

    // const_literal
    case TokenType::LiteralTrue:
        consume();
        RETURN_NODE_REF(BoolLiteral, true);
    case TokenType::LiteralFalse:
        consume();
        RETURN_NODE_REF(BoolLiteral, false);
    case TokenType::IntegerLiteral: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteral);
        RETURN_NODE_REF(AbstractIntLiteral, lit.m_literalValue);
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
        RETURN_NODE_REF(AbstractFloatLiteral, lit.m_literalValue);
    }
    case TokenType::HexFloatLiteral: {
        CONSUME_TYPE_NAMED(lit, HexFloatLiteral);
        RETURN_NODE_REF(AbstractFloatLiteral, lit.m_literalValue);
    }

    // Token represents a type, therefore the whole expression must be parsed as a
    // type cast.
    case TokenType::KeywordBool:
    case TokenType::KeywordF32:
    case TokenType::KeywordI32:
    case TokenType::KeywordU32:
    // FIXME: support omitting element type in vec2/vec3/vec4 type casts.
    // e.g: "vec2(1.0, 1,0)" instead of "vec2<f32>(1.0, 1.0)"
    case TokenType::KeywordVec2:
    case TokenType::KeywordVec3:
    case TokenType::KeywordVec4:
    case TokenType::KeywordArray: {
        PARSE(type, TypeDecl);
        PARSE(arguments, ArgumentExpressionList);
        RETURN_NODE_REF(CallableExpression, WTFMove(type), WTFMove(arguments));
    }

    // The identifier could either represent a
    //   * type name, in which case this is a type cast,
    //   * function name, in which case this is a function call.
    // Therefore after the identifier, what follows must either be nothing or the argument list.
    // It's illegal to specify type parameters after the type name, like this:
    // ```
    // type YetAnotherArray = array;
    // var a: YetAnotherArray<i32, 10>.
    // ```
    case TokenType::Identifier: {
        CONSUME_TYPE_NAMED(ident, Identifier);

        // If there's a left parentheses, what follows must be an argument list.
        // Therefore the whole expression is a callable expression.
        if (current().m_type == TokenType::ParenLeft) {
            auto target = makeUniqueRef<AST::NamedType>(CURRENT_SOURCE_SPAN(), WTFMove(ident.m_ident));
            PARSE(arguments, ArgumentExpressionList);
            RETURN_NODE_REF(CallableExpression, WTFMove(target), WTFMove(arguments));
        }

        // Otherwise, it's just an identifier.
        RETURN_NODE_REF(IdentifierExpression, ident.m_ident);
    }

    // TODO: bitcast expression

    default:
        break;
    }

    FAIL("Expected one of '(', a literal, a type declaration, or an identifier"_s);
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

// https://gpuweb.github.io/gpuweb/wgsl/#syntax-variable_statement
// variable_statement :
//   | variable_decl
//   | variable_decl equal expression
//   | let ( ident | variable_ident_decl ) equal expression
//   | const ( ident | variable_ident_decl ) equal expression
template<typename Lexer>
Expected<AST::VariableStatement, Error> Parser<Lexer>::parseVariableStatement()
{
    START_PARSE();
    
    PARSE(variableDecl, VariableDecl);
    
    RETURN_NODE(VariableStatement, WTFMove(variableDecl));
}

} // namespace WGSL
