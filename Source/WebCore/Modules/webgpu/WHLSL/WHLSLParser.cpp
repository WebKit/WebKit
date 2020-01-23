/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WHLSLParser.h"

#if ENABLE(WEBGPU)

#include "WHLSLAddressSpace.h"
#include "WHLSLEntryPointType.h"
#include "WHLSLProgram.h"
#include <wtf/dtoa.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

namespace WHLSL {

#define PARSE(name, element, ...) \
    auto name = parse##element(__VA_ARGS__); \
    if (!name) \
        return makeUnexpected(name.error()); \

#define CONSUME_TYPE(name, type) \
    auto name = consumeType(Token::Type::type); \
    if (!name) \
        return makeUnexpected(name.error());

#define PEEK(name) \
    auto name = peek(); \
    if (!name) \
        return makeUnexpected(name.error());

#define PEEK_FURTHER(name) \
    auto name = peekFurther(); \
    if (!name) \
        return makeUnexpected(name.error());

auto Parser::parse(Program& program, StringView stringView, ParsingMode mode, AST::NameSpace nameSpace) -> Expected<void, Error>
{
    m_lexer = Lexer(stringView, nameSpace);
    m_mode = mode;

    while (!m_lexer.isFullyConsumed()) {
        auto token = m_lexer.peek();
        switch (token.type) {
        case Token::Type::Invalid:
            return { };
        case Token::Type::Semicolon:
            m_lexer.consumeToken();
            continue;
        case Token::Type::Typedef: {
            auto typeDefinition = parseTypeDefinition();
            if (!typeDefinition)
                return makeUnexpected(typeDefinition.error());
            auto appendResult = program.append(WTFMove(*typeDefinition));
            if (!appendResult)
                return makeUnexpected(appendResult.error());
            continue;
        }
        case Token::Type::Struct: {
            auto structureDefinition = parseStructureDefinition();
            if (!structureDefinition)
                return makeUnexpected(structureDefinition.error());
            auto appendResult = program.append(WTFMove(*structureDefinition));
            if (!appendResult)
                return makeUnexpected(appendResult.error());
            continue;
        }
        case Token::Type::Enum: {
            auto enumerationDefinition = parseEnumerationDefinition();
            if (!enumerationDefinition)
                return makeUnexpected(enumerationDefinition.error());
            auto appendResult = program.append(WTFMove(*enumerationDefinition));
            if (!appendResult)
                return makeUnexpected(appendResult.error());
            continue;
        }
        case Token::Type::Native: {
            if (m_mode != ParsingMode::StandardLibrary)
                return fail(makeString("'native' can't exist outside of the standard library."));
            auto furtherToken = peekFurther();
            if (!furtherToken)
                return { };
            if (furtherToken->type == Token::Type::Typedef) {
                auto nativeTypeDeclaration = parseNativeTypeDeclaration();
                if (!nativeTypeDeclaration)
                    return makeUnexpected(nativeTypeDeclaration.error());
                auto appendResult = program.append(WTFMove(*nativeTypeDeclaration));
                if (!appendResult)
                    return makeUnexpected(appendResult.error());
                continue;
            }
            auto nativeFunctionDeclaration = parseNativeFunctionDeclaration();
            if (!nativeFunctionDeclaration)
                return makeUnexpected(nativeFunctionDeclaration.error());
            auto appendResult = program.append(WTFMove(*nativeFunctionDeclaration));
            if (!appendResult)
                return makeUnexpected(appendResult.error());
            continue;
        }
        default: {
            auto functionDefinition = parseFunctionDefinition();
            if (!functionDefinition)
                return makeUnexpected(functionDefinition.error());
            auto appendResult = program.append(WTFMove(*functionDefinition));
            if (!appendResult)
                return makeUnexpected(appendResult.error());
            continue;
        }
        }
    }

    return { };
}

auto Parser::fail(const String& message, TryToPeek tryToPeek) -> Unexpected<Error>
{
    if (tryToPeek == TryToPeek::Yes) {
        if (auto nextToken = peek())
            return makeUnexpected(Error(m_lexer.errorString(*nextToken, message)));
    }
    return makeUnexpected(Error(makeString("Cannot lex: ", message)));
}

auto Parser::peek() -> Expected<Token, Error>
{
    auto token = m_lexer.peek();
    if (token.type != Token::Type::Invalid && token.type != Token::Type::EndOfFile)
        return { token };
    return fail("Cannot consume token"_str, TryToPeek::No);
}

auto Parser::peekFurther() -> Expected<Token, Error>
{
    auto token = m_lexer.peekFurther();
    if (token.type != Token::Type::Invalid && token.type != Token::Type::EndOfFile)
        return { token };
    return fail("Cannot consume two tokens"_str, TryToPeek::No);
}

template <Token::Type t, Token::Type... ts>
struct Types {
    static bool includes(Token::Type type)
    {
        return t == type || Types<ts...>::includes(type);
    }

    static void appendNameTo(StringBuilder& builder)
    {
        builder.append(Token::typeName(t), ", ");
        Types<ts...>::appendNameTo(builder);
    }
};
template <Token::Type t>
struct Types<t> {
    static bool includes(Token::Type type)
    {
        return t == type;
    }

    static void appendNameTo(StringBuilder& builder)
    {
        builder.append(Token::typeName(t));
    }
};


bool Parser::peekType(Token::Type type)
{
    auto token = m_lexer.peek();
    return token.type == type;
}

template <Token::Type... types>
bool Parser::peekTypes()
{
    auto token = m_lexer.peek();
    return Types<types...>::includes(token.type);
}

Optional<Token> Parser::tryType(Token::Type type)
{
    auto token = m_lexer.peek();
    if (token.type == type)
        return { m_lexer.consumeToken() };
    return WTF::nullopt;
}

template <Token::Type... types>
Optional<Token> Parser::tryTypes()
{
    auto token = m_lexer.peek();
    if (Types<types...>::includes(token.type))
        return { m_lexer.consumeToken() };
    return WTF::nullopt;
}

auto Parser::consumeType(Token::Type type) -> Expected<Token, Error>
{
    auto token = m_lexer.consumeToken();
    if (token.type == type)
        return { token };
    return fail(makeString("Unexpected token (expected ", Token::typeName(type), " got ", Token::typeName(token.type), ")"));
}

template <Token::Type... types>
auto Parser::consumeTypes() -> Expected<Token, Error>
{
    auto buildExpectedString = [&]() -> String {
        StringBuilder builder;
        builder.append("[");
        Types<types...>::appendNameTo(builder);
        builder.append("]");
        return builder.toString();
    };

    auto token = m_lexer.consumeToken();
    if (Types<types...>::includes(token.type))
        return { token };
    return fail(makeString("Unexpected token (expected one of ", buildExpectedString(), " got ", Token::typeName(token.type), ")"));
}

static int digitValue(UChar character)
{
    if (character >= '0' && character <= '9')
        return character - '0';
    if (character >= 'a' && character <= 'f')
        return character - 'a' + 10;
    return character - 'A' + 10;
}

static Expected<int, Error> intLiteralToInt(StringView text)
{
    bool negate = false;
    if (text.startsWith("-"_str)) {
        negate = true;
        text = text.substring(1);
    }
    int base = 10;
    if (text.startsWith("0x"_str)) {
        text = text.substring(2);
        base = 16;
    }

    unsigned result = 0;
    for (auto codePoint : text.codePoints()) {
        unsigned digit = digitValue(codePoint);
        auto previous = result;
        result = result * base + digit;
        if (result < previous)
            return makeUnexpected(Error(makeString("int literal ", text, " is out of bounds")));
    }
    if (negate) {
        static_assert(sizeof(int64_t) > sizeof(unsigned) && sizeof(int64_t) > sizeof(int), "This code would be wrong otherwise");
        int64_t intResult = -static_cast<int64_t>(result);
        if (intResult < static_cast<int64_t>(std::numeric_limits<int>::min()))
            return makeUnexpected(Error(makeString("int literal ", text, " is out of bounds")));
        return { static_cast<int>(intResult) };
    }
    if (result > static_cast<unsigned>(std::numeric_limits<int>::max()))
        return makeUnexpected(Error(makeString("int literal ", text, " is out of bounds")));
    return { static_cast<int>(result) };
}

static Expected<unsigned, Error> uintLiteralToUint(StringView text)
{
    unsigned base = 10;
    if (text.startsWith("0x"_str)) {
        text = text.substring(2);
        base = 16;
    }
    ASSERT(text.endsWith("u"));
    text = text.substring(0, text.length() - 1);
    unsigned result = 0;
    for (auto codePoint : text.codePoints()) {
        unsigned digit = digitValue(codePoint);
        auto previous = result;
        result = result * base + digit;
        if (result < previous)
            return makeUnexpected(Error(makeString("uint literal ", text, " is out of bounds")));
    }
    return { result };
}

static Expected<float, Error> floatLiteralToFloat(StringView text)
{
    size_t parsedLength;
    auto result = parseDouble(text, parsedLength);
    if (parsedLength != text.length())
        return makeUnexpected(Error(makeString("Cannot parse float ", text)));
    return static_cast<float>(result);
}

auto Parser::consumeIntegralLiteral() -> Expected<Variant<int, unsigned>, Error>
{
    auto integralLiteralToken = consumeTypes<Token::Type::IntLiteral, Token::Type::UintLiteral>();
    if (!integralLiteralToken)
        return makeUnexpected(integralLiteralToken.error());

    switch (integralLiteralToken->type) {
    case Token::Type::IntLiteral: {
        auto result = intLiteralToInt(integralLiteralToken->stringView(m_lexer));
        if (result)
            return {{ *result }};
        return makeUnexpected(result.error());
    }
    default: {
        ASSERT(integralLiteralToken->type == Token::Type::UintLiteral);
        auto result = uintLiteralToUint(integralLiteralToken->stringView(m_lexer));
        if (result)
            return {{ *result }};
        return makeUnexpected(result.error());
    }
    }
}

auto Parser::consumeNonNegativeIntegralLiteral() -> Expected<unsigned, Error>
{
    auto integralLiteral = consumeIntegralLiteral();
    if (!integralLiteral)
        return makeUnexpected(integralLiteral.error());
    auto result = WTF::visit(WTF::makeVisitor([](int x) -> Optional<unsigned> {
        if (x < 0)
            return WTF::nullopt;
        return x;
    }, [](unsigned x) -> Optional<unsigned> {
        return x;
    }), *integralLiteral);
    if (result)
        return *result;
    return fail("int literal is negative"_str);
}

static Expected<unsigned, Error> recognizeSimpleUnsignedInteger(StringView stringView)
{
    unsigned result = 0;
    if (stringView.length() < 1)
        return makeUnexpected(Error(makeString("Simple unsigned literal ", stringView, " is too short")));
    for (auto codePoint : stringView.codePoints()) {
        if (codePoint < '0' || codePoint > '9')
            return makeUnexpected(Error(makeString("Simple unsigned literal ", stringView, " isn't of the form [0-9]+")));
        auto previous = result;
        result = result * 10 + (codePoint - '0');
        if (result < previous)
            return makeUnexpected(Error(makeString("Simple unsigned literal ", stringView, " is out of bounds")));
    }
    return result;
}

auto Parser::parseConstantExpression() -> Expected<AST::ConstantExpression, Error>
{
    auto type = consumeTypes<
        Token::Type::IntLiteral,
        Token::Type::UintLiteral,
        Token::Type::FloatLiteral,
        Token::Type::Null,
        Token::Type::True,
        Token::Type::False,
        Token::Type::Identifier>();
    if (!type)
        return makeUnexpected(type.error());

    switch (type->type) {
    case Token::Type::IntLiteral: {
        auto value = intLiteralToInt(type->stringView(m_lexer));
        if (!value)
            return makeUnexpected(value.error());
        return {{ AST::IntegerLiteral({ *type }, *value) }};
    }
    case Token::Type::UintLiteral: {
        auto value = uintLiteralToUint(type->stringView(m_lexer));
        if (!value)
            return makeUnexpected(value.error());
        return {{ AST::UnsignedIntegerLiteral({ *type }, *value) }};
    }
    case Token::Type::FloatLiteral: {
        auto value = floatLiteralToFloat(type->stringView(m_lexer));
        if (!value)
            return makeUnexpected(value.error());
        return {{ AST::FloatLiteral({ *type }, *value) }};
    }
    case Token::Type::True:
        return { AST::BooleanLiteral(WTFMove(*type), true) };
    case Token::Type::False:
        return { AST::BooleanLiteral(WTFMove(*type), false) };
    default: {
        ASSERT(type->type == Token::Type::Identifier);
        CONSUME_TYPE(fullStop, FullStop);
        CONSUME_TYPE(next, Identifier);
        return { AST::EnumerationMemberLiteral({ *type, *next }, type->stringView(m_lexer).toString(), next->stringView(m_lexer).toString()) };
    }
    }
}

auto Parser::parseTypeArgument() -> Expected<AST::TypeArgument, Error>
{
    PEEK(nextToken);
    PEEK_FURTHER(furtherToken);
    if (nextToken->type != Token::Type::Identifier || furtherToken->type == Token::Type::FullStop) {
        PARSE(constantExpression, ConstantExpression);
        return AST::TypeArgument(WTFMove(*constantExpression));
    }
    CONSUME_TYPE(result, Identifier);
    CodeLocation location(*result);
    return AST::TypeArgument(AST::TypeReference::create(location, result->stringView(m_lexer).toString(), AST::TypeArguments()));
}

auto Parser::parseTypeArguments() -> Expected<AST::TypeArguments, Error>
{
    AST::TypeArguments typeArguments;
    auto lessThanSign = tryType(Token::Type::LessThanSign);
    if (!lessThanSign)
        return typeArguments;

    auto greaterThanSign = tryType(Token::Type::GreaterThanSign);
    if (greaterThanSign)
        return typeArguments;

    PARSE(typeArgument, TypeArgument);
    typeArguments.append(WTFMove(*typeArgument));

    while (true) {
        auto greaterThanSign = tryType(Token::Type::GreaterThanSign);
        if (greaterThanSign)
            break;

        CONSUME_TYPE(comma, Comma);
        PARSE(typeArgument, TypeArgument);
        typeArguments.append(WTFMove(*typeArgument));
    }

    return typeArguments;
}

auto Parser::parseTypeSuffixAbbreviated() -> Expected<TypeSuffixAbbreviated, Error>
{
    auto token = consumeTypes<
        Token::Type::Star,
        Token::Type::SquareBracketPair,
        Token::Type::LeftSquareBracket>();
    if (!token)
        return makeUnexpected(token.error());
    if (token->type == Token::Type::LeftSquareBracket) {
        auto numElements = consumeNonNegativeIntegralLiteral();
        if (!numElements)
            return makeUnexpected(numElements.error());
        CONSUME_TYPE(rightSquareBracket, RightSquareBracket);
        return {{ { *token, *rightSquareBracket }, *token, *numElements }};
    }
    return {{ { *token }, *token, WTF::nullopt }};
}

auto Parser::parseTypeSuffixNonAbbreviated() -> Expected<TypeSuffixNonAbbreviated, Error>
{
    auto token = consumeTypes<
        Token::Type::Star,
        Token::Type::SquareBracketPair,
        Token::Type::LeftSquareBracket>();
    if (!token)
        return makeUnexpected(token.error());
    if (token->type == Token::Type::LeftSquareBracket) {
        auto numElements = consumeNonNegativeIntegralLiteral();
        if (!numElements)
            return makeUnexpected(numElements.error());
        CONSUME_TYPE(rightSquareBracket, RightSquareBracket);
        return {{ { *token, *rightSquareBracket }, *token, WTF::nullopt, *numElements }};
    }
    auto addressSpaceToken = consumeTypes<
        Token::Type::Constant,
        Token::Type::Device,
        Token::Type::Threadgroup,
        Token::Type::Thread>();
    if (!addressSpaceToken)
        return makeUnexpected(addressSpaceToken.error());
    AST::AddressSpace addressSpace;
    switch (addressSpaceToken->type) {
    case Token::Type::Constant:
        addressSpace = AST::AddressSpace::Constant;
        break;
    case Token::Type::Device:
        addressSpace = AST::AddressSpace::Device;
        break;
    case Token::Type::Threadgroup:
        addressSpace = AST::AddressSpace::Threadgroup;
        break;
    default:
        ASSERT(addressSpaceToken->type == Token::Type::Thread);
        addressSpace = AST::AddressSpace::Thread;
        break;
    }
    return {{ { *token }, *token, { addressSpace }, WTF::nullopt }};
}

auto Parser::parseType() -> Expected<Ref<AST::UnnamedType>, Error>
{
    auto addressSpaceToken = tryTypes<
        Token::Type::Constant,
        Token::Type::Device,
        Token::Type::Threadgroup,
        Token::Type::Thread>();

    CONSUME_TYPE(name, Identifier);
    PARSE(typeArguments, TypeArguments);

    if (addressSpaceToken) {
        AST::AddressSpace addressSpace;
        switch (addressSpaceToken->type) {
        case Token::Type::Constant:
            addressSpace = AST::AddressSpace::Constant;
            break;
        case Token::Type::Device:
            addressSpace = AST::AddressSpace::Device;
            break;
        case Token::Type::Threadgroup:
            addressSpace = AST::AddressSpace::Threadgroup;
            break;
        default:
            ASSERT(addressSpaceToken->type == Token::Type::Thread);
            addressSpace = AST::AddressSpace::Thread;
            break;
        }
        auto constructTypeFromSuffixAbbreviated = [&](const TypeSuffixAbbreviated& typeSuffixAbbreviated, Ref<AST::UnnamedType>&& previous) -> Ref<AST::UnnamedType> {
            CodeLocation location(*addressSpaceToken, typeSuffixAbbreviated.location);
            switch (typeSuffixAbbreviated.token.type) {
            case Token::Type::Star:
                return { AST::PointerType::create(location, addressSpace, WTFMove(previous)) };
            case Token::Type::SquareBracketPair:
                return { AST::ArrayReferenceType::create(location, addressSpace, WTFMove(previous)) };
            default:
                ASSERT(typeSuffixAbbreviated.token.type == Token::Type::LeftSquareBracket);
                return { AST::ArrayType::create(location, WTFMove(previous), *typeSuffixAbbreviated.numElements) };
            }
        };
        PARSE(firstTypeSuffixAbbreviated, TypeSuffixAbbreviated);
        Ref<AST::UnnamedType> result = AST::TypeReference::create(WTFMove(*addressSpaceToken), name->stringView(m_lexer).toString(), WTFMove(*typeArguments));
        auto next = constructTypeFromSuffixAbbreviated(*firstTypeSuffixAbbreviated, WTFMove(result));
        result = WTFMove(next);
        while (true) {
            PEEK(nextToken);
            if (nextToken->type != Token::Type::Star
                && nextToken->type != Token::Type::SquareBracketPair
                && nextToken->type != Token::Type::LeftSquareBracket) {
                break;
            }
            PARSE(typeSuffixAbbreviated, TypeSuffixAbbreviated);
            // FIXME: The nesting here might be in the wrong order.
            next = constructTypeFromSuffixAbbreviated(*typeSuffixAbbreviated, WTFMove(result));
            result = WTFMove(next);
        }
        return WTFMove(result);
    }

    auto constructTypeFromSuffixNonAbbreviated = [&](const TypeSuffixNonAbbreviated& typeSuffixNonAbbreviated, Ref<AST::UnnamedType>&& previous) -> Ref<AST::UnnamedType> {
        CodeLocation location(*name, typeSuffixNonAbbreviated.location);
        switch (typeSuffixNonAbbreviated.token.type) {
        case Token::Type::Star:
            return { AST::PointerType::create(location, *typeSuffixNonAbbreviated.addressSpace, WTFMove(previous)) };
        case Token::Type::SquareBracketPair:
            return { AST::ArrayReferenceType::create(location, *typeSuffixNonAbbreviated.addressSpace, WTFMove(previous)) };
        default:
            ASSERT(typeSuffixNonAbbreviated.token.type == Token::Type::LeftSquareBracket);
            return { AST::ArrayType::create(location, WTFMove(previous), *typeSuffixNonAbbreviated.numElements) };
        }
    };
    Ref<AST::UnnamedType> result = AST::TypeReference::create(*name, name->stringView(m_lexer).toString(), WTFMove(*typeArguments));
    while (true) {
        PEEK(nextToken);
        if (nextToken->type != Token::Type::Star
            && nextToken->type != Token::Type::SquareBracketPair
            && nextToken->type != Token::Type::LeftSquareBracket) {
            break;
        }
        PARSE(typeSuffixNonAbbreviated, TypeSuffixNonAbbreviated);
        // FIXME: The nesting here might be in the wrong order.
        auto next = constructTypeFromSuffixNonAbbreviated(*typeSuffixNonAbbreviated, WTFMove(result));
        result = WTFMove(next);
    }
    return WTFMove(result);
}

auto Parser::parseTypeDefinition() -> Expected<AST::TypeDefinition, Error>
{
    CONSUME_TYPE(origin, Typedef);
    CONSUME_TYPE(name, Identifier);
    CONSUME_TYPE(equals, EqualsSign);
    PARSE(type, Type);
    CONSUME_TYPE(semicolon, Semicolon);
    return AST::TypeDefinition({ *origin, *semicolon }, name->stringView(m_lexer).toString(), WTFMove(*type));
}

auto Parser::parseBuiltInSemantic() -> Expected<AST::BuiltInSemantic, Error>
{
    auto origin = consumeTypes<
        Token::Type::SVInstanceID,
        Token::Type::SVVertexID,
        Token::Type::PSize,
        Token::Type::SVPosition,
        Token::Type::SVIsFrontFace,
        Token::Type::SVSampleIndex,
        Token::Type::SVInnerCoverage,
        Token::Type::SVTarget,
        Token::Type::SVDepth,
        Token::Type::SVCoverage,
        Token::Type::SVDispatchThreadID,
        Token::Type::SVGroupID,
        Token::Type::SVGroupIndex,
        Token::Type::SVGroupThreadID>();
    if (!origin)
        return makeUnexpected(origin.error());

    switch (origin->type) {
    case Token::Type::SVInstanceID:
        return AST::BuiltInSemantic({ *origin }, AST::BuiltInSemantic::Variable::SVInstanceID);
    case Token::Type::SVVertexID:
        return AST::BuiltInSemantic({ *origin }, AST::BuiltInSemantic::Variable::SVVertexID);
    case Token::Type::PSize:
        return AST::BuiltInSemantic({ *origin }, AST::BuiltInSemantic::Variable::PSize);
    case Token::Type::SVPosition:
        return AST::BuiltInSemantic({ *origin }, AST::BuiltInSemantic::Variable::SVPosition);
    case Token::Type::SVIsFrontFace:
        return AST::BuiltInSemantic({ *origin }, AST::BuiltInSemantic::Variable::SVIsFrontFace);
    case Token::Type::SVSampleIndex:
        return AST::BuiltInSemantic({ *origin }, AST::BuiltInSemantic::Variable::SVSampleIndex);
    case Token::Type::SVInnerCoverage:
        return AST::BuiltInSemantic({ *origin }, AST::BuiltInSemantic::Variable::SVInnerCoverage);
    case Token::Type::SVTarget: {
        auto target = consumeNonNegativeIntegralLiteral(); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195807 Make this work with strings like "SV_Target0".
        if (!target)
            return makeUnexpected(target.error());
        return AST::BuiltInSemantic({ *origin }, AST::BuiltInSemantic::Variable::SVTarget, *target);
    }
    case Token::Type::SVDepth:
        return AST::BuiltInSemantic({ *origin }, AST::BuiltInSemantic::Variable::SVDepth);
    case Token::Type::SVCoverage:
        return AST::BuiltInSemantic({ *origin }, AST::BuiltInSemantic::Variable::SVCoverage);
    case Token::Type::SVDispatchThreadID:
        return AST::BuiltInSemantic({ *origin }, AST::BuiltInSemantic::Variable::SVDispatchThreadID);
    case Token::Type::SVGroupID:
        return AST::BuiltInSemantic({ *origin }, AST::BuiltInSemantic::Variable::SVGroupID);
    case Token::Type::SVGroupIndex:
        return AST::BuiltInSemantic({ *origin }, AST::BuiltInSemantic::Variable::SVGroupIndex);
    default:
        ASSERT(origin->type == Token::Type::SVGroupThreadID);
        return AST::BuiltInSemantic({ *origin }, AST::BuiltInSemantic::Variable::SVGroupThreadID);
    }
}

auto Parser::parseResourceSemantic() -> Expected<AST::ResourceSemantic, Error>
{
    CONSUME_TYPE(origin, Register);
    CONSUME_TYPE(leftParenthesis, LeftParenthesis);

    CONSUME_TYPE(info, Identifier);
    auto infoStringView = info->stringView(m_lexer);
    if (infoStringView.length() < 2 || (infoStringView[0] != 'u'
        && infoStringView[0] != 't'
        && infoStringView[0] != 'b'
        && infoStringView[0] != 's'))
        return makeUnexpected(Error(makeString(infoStringView.substring(0, 1), " is not a known resource type ('u', 't', 'b', or 's')")));

    AST::ResourceSemantic::Mode mode;
    switch (infoStringView[0]) {
    case 'u':
        mode = AST::ResourceSemantic::Mode::UnorderedAccessView;
        break;
    case 't':
        mode = AST::ResourceSemantic::Mode::Texture;
        break;
    case 'b':
        mode = AST::ResourceSemantic::Mode::Buffer;
        break;
    case 's':
        mode = AST::ResourceSemantic::Mode::Sampler;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    auto index = recognizeSimpleUnsignedInteger(infoStringView.substring(1));
    if (!index)
        return makeUnexpected(index.error());

    unsigned space = 0;
    if (tryType(Token::Type::Comma)) {
        CONSUME_TYPE(spaceToken, Identifier);
        auto spaceTokenStringView = spaceToken->stringView(m_lexer);
        StringView prefix { "space" };
        if (!spaceTokenStringView.startsWith(prefix))
            return makeUnexpected(Error(makeString("Second argument to resource semantic ", spaceTokenStringView, " needs be of the form 'space0'")));
        if (spaceTokenStringView.length() <= prefix.length())
            return makeUnexpected(Error(makeString("Second argument to resource semantic ", spaceTokenStringView, " needs be of the form 'space0'")));
        auto spaceValue = recognizeSimpleUnsignedInteger(spaceTokenStringView.substring(prefix.length()));
        if (!spaceValue)
            return makeUnexpected(spaceValue.error());
        space = *spaceValue;
    }

    CONSUME_TYPE(rightParenthesis, RightParenthesis);

    return AST::ResourceSemantic({ *origin, *rightParenthesis }, mode, *index, space);
}

auto Parser::parseSpecializationConstantSemantic() -> Expected<AST::SpecializationConstantSemantic, Error>
{
    CONSUME_TYPE(origin, Specialized);
    return AST::SpecializationConstantSemantic(*origin);
}

auto Parser::parseStageInOutSemantic() -> Expected<AST::StageInOutSemantic, Error>
{
    CONSUME_TYPE(origin, Attribute);
    CONSUME_TYPE(leftParenthesis, LeftParenthesis);

    auto index = consumeNonNegativeIntegralLiteral();
    if (!index)
        return makeUnexpected(index.error());

    CONSUME_TYPE(rightParenthesis, RightParenthesis);

    return AST::StageInOutSemantic({ *origin, *rightParenthesis }, *index);
}

auto Parser::parseSemantic() -> Expected<std::unique_ptr<AST::Semantic>, Error>
{
    if (!tryType(Token::Type::Colon))
        return { nullptr };

    PEEK(token);
    switch (token->type) {
    case Token::Type::Attribute: {
        PARSE(result, StageInOutSemantic);
        return { makeUnique<AST::Semantic>(WTFMove(*result)) };
    }
    case Token::Type::Specialized:  {
        PARSE(result, SpecializationConstantSemantic);
        return { makeUnique<AST::Semantic>(WTFMove(*result)) };
    }
    case Token::Type::Register:  {
        PARSE(result, ResourceSemantic);
        return { makeUnique<AST::Semantic>(WTFMove(*result)) };
    }
    default:  {
        PARSE(result, BuiltInSemantic);
        return { makeUnique<AST::Semantic>(WTFMove(*result)) };
    }
    }
}
AST::Qualifiers Parser::parseQualifiers()
{
    AST::Qualifiers qualifiers;
    while (auto next = tryType(Token::Type::Qualifier)) {
        auto nextStringView = next->stringView(m_lexer);
        if ("nointerpolation" == nextStringView)
            qualifiers.append(AST::Qualifier::Nointerpolation);
        else if ("noperspective" == nextStringView)
            qualifiers.append(AST::Qualifier::Noperspective);
        else if ("uniform" == nextStringView)
            qualifiers.append(AST::Qualifier::Uniform);
        else if ("centroid" == nextStringView)
            qualifiers.append(AST::Qualifier::Centroid);
        else {
            ASSERT("sample" == nextStringView);
            qualifiers.append(AST::Qualifier::Sample);
        }
    }
    return qualifiers;
}

auto Parser::parseStructureElement() -> Expected<AST::StructureElement, Error>
{
    PEEK(origin);

    AST::Qualifiers qualifiers = parseQualifiers();

    PARSE(type, Type);
    CONSUME_TYPE(name, Identifier);
    PARSE(semantic, Semantic);
    CONSUME_TYPE(semicolon, Semicolon);

    return AST::StructureElement({ *origin, *semicolon }, WTFMove(qualifiers), WTFMove(*type), name->stringView(m_lexer).toString(), WTFMove(*semantic));
}

auto Parser::parseStructureDefinition() -> Expected<AST::StructureDefinition, Error>
{
    CONSUME_TYPE(origin, Struct);
    CONSUME_TYPE(name, Identifier);
    CONSUME_TYPE(leftCurlyBracket, LeftCurlyBracket);

    AST::StructureElements structureElements;
    while (!peekType(Token::Type::RightCurlyBracket)) {
        PARSE(structureElement, StructureElement);
        structureElements.append(WTFMove(*structureElement));
    }

    auto rightCurlyBracket = m_lexer.consumeToken();

    return AST::StructureDefinition({ *origin, rightCurlyBracket }, name->stringView(m_lexer).toString(), WTFMove(structureElements));
}

auto Parser::parseEnumerationDefinition() -> Expected<AST::EnumerationDefinition, Error>
{
    CONSUME_TYPE(origin, Enum);
    CONSUME_TYPE(name, Identifier);

    auto type = ([&]() -> Expected<Ref<AST::UnnamedType>, Error> {
        if (tryType(Token::Type::Colon)) {
            PARSE(parsedType, Type);
            return WTFMove(*parsedType);
        }
        return { AST::TypeReference::create(*origin, "int"_str, AST::TypeArguments()) };
    })();
    if (!type)
        return makeUnexpected(type.error());

    CONSUME_TYPE(leftCurlyBracket, LeftCurlyBracket);

    int64_t nextValue = 0;
    PARSE(firstEnumerationMember, EnumerationMember, nextValue);
    nextValue = firstEnumerationMember->value() + 1;

    AST::EnumerationDefinition result({ }, name->stringView(m_lexer).toString(), WTFMove(*type));
    auto success = result.add(WTFMove(*firstEnumerationMember));
    if (!success)
        return fail("Cannot add enumeration member"_str);

    while (tryType(Token::Type::Comma)) {
        PARSE(member, EnumerationMember, nextValue);
        nextValue = member->value() + 1;
        success = result.add(WTFMove(*member));
        if (!success)
            return fail("Cannot add enumeration member"_str);
    }

    CONSUME_TYPE(rightCurlyBracket, RightCurlyBracket);
    result.updateCodeLocation({ *origin, *rightCurlyBracket});

    return WTFMove(result);
}

auto Parser::parseEnumerationMember(int64_t defaultValue) -> Expected<AST::EnumerationMember, Error>
{
    CONSUME_TYPE(identifier, Identifier);
    auto name = identifier->stringView(m_lexer).toString();

    if (tryType(Token::Type::EqualsSign)) {
        PARSE(constantExpression, ConstantExpression);

        Optional<int64_t> value;
        constantExpression->visit(WTF::makeVisitor([&](AST::IntegerLiteral& integerLiteral) {
            value = integerLiteral.value();
        }, [&](AST::UnsignedIntegerLiteral& unsignedIntegerLiteral) {
            value = unsignedIntegerLiteral.value();
        }, [&](AST::FloatLiteral&) {
        }, [&](AST::BooleanLiteral&) {
        }, [&](AST::EnumerationMemberLiteral&) {
        }));

        if (!value)
            return makeUnexpected(Error("enum initialization values can only be an int or uint constant."));
        return AST::EnumerationMember(*identifier, WTFMove(name), *value);
    }
    return AST::EnumerationMember(*identifier, WTFMove(name), defaultValue);
}

auto Parser::parseNativeTypeDeclaration() -> Expected<AST::NativeTypeDeclaration, Error>
{
    CONSUME_TYPE(origin, Native);
    CONSUME_TYPE(parsedTypedef, Typedef);
    CONSUME_TYPE(name, Identifier);
    PARSE(typeArguments, TypeArguments);
    CONSUME_TYPE(semicolon, Semicolon);

    return AST::NativeTypeDeclaration({ *origin, *semicolon }, name->stringView(m_lexer).toString(), WTFMove(*typeArguments));
}

auto Parser::parseNumThreadsFunctionAttribute() -> Expected<AST::NumThreadsFunctionAttribute, Error>
{
    CONSUME_TYPE(origin, NumThreads);
    CONSUME_TYPE(leftParenthesis, LeftParenthesis);

    auto width = consumeNonNegativeIntegralLiteral();
    if (!width)
        return makeUnexpected(width.error());

    CONSUME_TYPE(comma, Comma);

    auto height = consumeNonNegativeIntegralLiteral();
    if (!height)
        return makeUnexpected(height.error());

    CONSUME_TYPE(secondComma, Comma);

    auto depth = consumeNonNegativeIntegralLiteral();
    if (!depth)
        return makeUnexpected(depth.error());

    CONSUME_TYPE(rightParenthesis, RightParenthesis);

    return AST::NumThreadsFunctionAttribute({ *origin, *rightParenthesis }, *width, *height, *depth);
}

auto Parser::parseAttributeBlock() -> Expected<AST::AttributeBlock, Error>
{
    CONSUME_TYPE(leftSquareBracket, LeftSquareBracket);

    AST::AttributeBlock result;

    while (!tryType(Token::Type::RightSquareBracket)) {
        PARSE(numThreadsFunctionAttribute, NumThreadsFunctionAttribute);
        result.append(WTFMove(*numThreadsFunctionAttribute));
    }

    return WTFMove(result);
}

auto Parser::parseParameter() -> Expected<AST::VariableDeclaration, Error>
{
    auto startOffset = m_lexer.peek().startOffset();

    AST::Qualifiers qualifiers = parseQualifiers();
    PARSE(type, Type);

    String name;
    if (auto token = tryType(Token::Type::Identifier))
        name = token->stringView(m_lexer).toString();

    PARSE(semantic, Semantic);

    auto endOffset = m_lexer.peek().startOffset();

    return AST::VariableDeclaration({ startOffset, endOffset, m_lexer.nameSpace() }, WTFMove(qualifiers), { WTFMove(*type) }, WTFMove(name), WTFMove(*semantic), nullptr);
}

auto Parser::parseParameters() -> Expected<AST::VariableDeclarations, Error>
{
    AST::VariableDeclarations parameters;

    CONSUME_TYPE(leftParenthesis, LeftParenthesis);

    if (tryType(Token::Type::RightParenthesis))
        return WTFMove(parameters);

    PARSE(firstParameter, Parameter);
    parameters.append(makeUniqueRef<AST::VariableDeclaration>(WTFMove(*firstParameter)));

    while (tryType(Token::Type::Comma)) {
        PARSE(parameter, Parameter);
        parameters.append(makeUniqueRef<AST::VariableDeclaration>(WTFMove(*parameter)));
    }

    CONSUME_TYPE(rightParenthesis, RightParenthesis);

    return WTFMove(parameters);
}

auto Parser::parseFunctionDefinition() -> Expected<AST::FunctionDefinition, Error>
{
    PARSE(functionDeclaration, FunctionDeclaration);
    PARSE(block, Block);
    return AST::FunctionDefinition(WTFMove(*functionDeclaration), WTFMove(*block));
}

auto Parser::parseComputeFunctionDeclaration() -> Expected<AST::FunctionDeclaration, Error>
{
    PEEK(origin);

    PARSE(attributeBlock, AttributeBlock);
    CONSUME_TYPE(compute, Compute);
    PARSE(type, Type);
    CONSUME_TYPE(name, Identifier);
    PARSE(parameters, Parameters);
    PARSE(semantic, Semantic);

    auto endOffset = m_lexer.peek().startOffset();

    bool isOperator = false;
    return AST::FunctionDeclaration({ origin->startOffset(), endOffset, m_lexer.nameSpace() }, WTFMove(*attributeBlock), AST::EntryPointType::Compute, WTFMove(*type), name->stringView(m_lexer).toString(), WTFMove(*parameters), WTFMove(*semantic), isOperator, m_mode);
}

auto Parser::parseVertexOrFragmentFunctionDeclaration() -> Expected<AST::FunctionDeclaration, Error>
{
    auto entryPoint = consumeTypes<Token::Type::Vertex, Token::Type::Fragment>();
    if (!entryPoint)
        return makeUnexpected(entryPoint.error());
    auto entryPointType = (entryPoint->type == Token::Type::Vertex) ? AST::EntryPointType::Vertex : AST::EntryPointType::Fragment;

    PARSE(type, Type);
    CONSUME_TYPE(name, Identifier);
    PARSE(parameters, Parameters);
    PARSE(semantic, Semantic);

    auto endOffset = m_lexer.peek().startOffset();

    bool isOperator = false;
    return AST::FunctionDeclaration({ entryPoint->startOffset(), endOffset, m_lexer.nameSpace() }, { }, entryPointType, WTFMove(*type), name->stringView(m_lexer).toString(), WTFMove(*parameters), WTFMove(*semantic), isOperator, m_mode);
}

auto Parser::parseRegularFunctionDeclaration() -> Expected<AST::FunctionDeclaration, Error>
{
    PEEK(origin);

    PARSE(type, Type);

    auto name = consumeTypes<Token::Type::Identifier, Token::Type::OperatorName>();
    if (!name)
        return makeUnexpected(name.error());
    auto isOperator = name->type == Token::Type::OperatorName;

    PARSE(parameters, Parameters);
    PARSE(semantic, Semantic);

    auto endOffset = m_lexer.peek().startOffset();

    return AST::FunctionDeclaration({ origin->startOffset(), endOffset, m_lexer.nameSpace() }, { }, WTF::nullopt, WTFMove(*type), name->stringView(m_lexer).toString(), WTFMove(*parameters), WTFMove(*semantic), isOperator, m_mode);
}

auto Parser::parseOperatorFunctionDeclaration() -> Expected<AST::FunctionDeclaration, Error>
{
    CONSUME_TYPE(origin, Operator);
    PARSE(type, Type);
    PARSE(parameters, Parameters);
    PARSE(semantic, Semantic);

    auto endOffset = m_lexer.peek().startOffset();

    bool isOperator = true;
    return AST::FunctionDeclaration({ origin->startOffset(), endOffset, m_lexer.nameSpace() }, { }, WTF::nullopt, WTFMove(*type), "operator cast"_str, WTFMove(*parameters), WTFMove(*semantic), isOperator, m_mode);
}

auto Parser::parseFunctionDeclaration() -> Expected<AST::FunctionDeclaration, Error>
{
    PEEK(token);
    switch (token->type) {
    case Token::Type::Operator:
        return parseOperatorFunctionDeclaration();
    case Token::Type::Vertex:
    case Token::Type::Fragment:
        return parseVertexOrFragmentFunctionDeclaration();
    case Token::Type::LeftSquareBracket:
        return parseComputeFunctionDeclaration();
    default:
        return parseRegularFunctionDeclaration();
    }
}

auto Parser::parseNativeFunctionDeclaration() -> Expected<AST::NativeFunctionDeclaration, Error>
{
    CONSUME_TYPE(native, Native);
    PARSE(functionDeclaration, FunctionDeclaration);
    CONSUME_TYPE(semicolon, Semicolon);

    return AST::NativeFunctionDeclaration(WTFMove(*functionDeclaration));
}

auto Parser::parseBlock() -> Expected<AST::Block, Error>
{
    CONSUME_TYPE(origin, LeftCurlyBracket);
    PARSE(result, BlockBody);
    CONSUME_TYPE(rightCurlyBracket, RightCurlyBracket);
    result->updateCodeLocation({ *origin, *rightCurlyBracket });
    return WTFMove(*result);
}

auto Parser::parseBlockBody() -> Expected<AST::Block, Error>
{
    auto startOffset = m_lexer.peek().startOffset();

    AST::Statements statements;
    while (!peekTypes<Token::Type::RightCurlyBracket, Token::Type::Case, Token::Type::Default>()) {
        PARSE(statement, Statement);
        statements.append(WTFMove(*statement));
    }

    auto endOffset = m_lexer.peek().startOffset();

    return AST::Block({ startOffset, endOffset, m_lexer.nameSpace() }, WTFMove(statements));
}

auto Parser::parseIfStatement() -> Expected<AST::IfStatement, Error>
{
    CONSUME_TYPE(origin, If);
    CONSUME_TYPE(leftParenthesis, LeftParenthesis);
    PARSE(conditional, Expression);
    CONSUME_TYPE(rightParenthesis, RightParenthesis);
    PARSE(body, Statement);

    std::unique_ptr<AST::Statement> elseBody(nullptr);
    if (tryType(Token::Type::Else)) {
        PARSE(parsedElseBody, Statement);
        elseBody = (*parsedElseBody).moveToUniquePtr();
    }

    auto endOffset = m_lexer.peek().startOffset();

    Vector<UniqueRef<AST::Expression>> castArguments;
    castArguments.append(WTFMove(*conditional));
    auto boolCast = makeUniqueRef<AST::CallExpression>(Token(*origin), "bool"_str, WTFMove(castArguments));
    return AST::IfStatement({ origin->startOffset(), endOffset, m_lexer.nameSpace() }, WTFMove(boolCast), WTFMove(*body), WTFMove(elseBody));
}

auto Parser::parseSwitchStatement() -> Expected<AST::SwitchStatement, Error>
{
    CONSUME_TYPE(origin, Switch);
    CONSUME_TYPE(leftParenthesis, LeftParenthesis);
    PARSE(value, Expression);
    CONSUME_TYPE(rightParenthesis, RightParenthesis);
    CONSUME_TYPE(leftCurlyBracket, LeftCurlyBracket);

    Vector<AST::SwitchCase> switchCases;
    PEEK(nextToken);
    while (nextToken->type != Token::Type::RightCurlyBracket) {
        PARSE(switchCase, SwitchCase);
        switchCases.append(WTFMove(*switchCase));
        PEEK(nextTokenInLoop);
        nextToken = nextTokenInLoop;
    }

    auto endToken = m_lexer.consumeToken();

    return AST::SwitchStatement({ *origin, endToken }, WTFMove(*value), WTFMove(switchCases));
}

auto Parser::parseSwitchCase() -> Expected<AST::SwitchCase, Error>
{
    auto origin = consumeTypes<Token::Type::Case, Token::Type::Default>();
    if (!origin)
        return makeUnexpected(origin.error());

    switch (origin->type) {
    case Token::Type::Case: {
        PARSE(value, ConstantExpression);
        CONSUME_TYPE(colon, Colon);

        PARSE(block, BlockBody);

        return AST::SwitchCase({ origin->codeLocation,  block->codeLocation()}, WTFMove(*value), WTFMove(*block));
    }
    default: {
        ASSERT(origin->type == Token::Type::Default);
        CONSUME_TYPE(colon, Colon);

        PARSE(block, BlockBody);

        return AST::SwitchCase({ origin->codeLocation,  block->codeLocation()}, WTF::nullopt, WTFMove(*block));
    }
    }
}

auto Parser::parseForLoop() -> Expected<AST::ForLoop, Error>
{
    CONSUME_TYPE(origin, For);
    CONSUME_TYPE(leftParenthesis, LeftParenthesis);

    auto parseRemainder = [&](UniqueRef<AST::Statement>&& initialization) -> Expected<AST::ForLoop, Error> {
        CONSUME_TYPE(semicolon, Semicolon);

        std::unique_ptr<AST::Expression> condition(nullptr);
        if (!tryType(Token::Type::Semicolon)) {
            if (auto expression = parseExpression())
                condition = (*expression).moveToUniquePtr();
            else
                return makeUnexpected(expression.error());
            CONSUME_TYPE(secondSemicolon, Semicolon);
        }

        std::unique_ptr<AST::Expression> increment(nullptr);
        if (!tryType(Token::Type::RightParenthesis)) {
            if (auto expression = parseExpression())
                increment = (*expression).moveToUniquePtr();
            else
                return makeUnexpected(expression.error());
            CONSUME_TYPE(rightParenthesis, RightParenthesis);
        }

        PARSE(body, Statement);
        CodeLocation location(origin->codeLocation, (*body)->codeLocation());
        return AST::ForLoop(location, WTFMove(initialization), WTFMove(condition), WTFMove(increment), WTFMove(*body));
    };

    auto variableDeclarations = backtrackingScope<Expected<AST::VariableDeclarationsStatement, Error>>([&]() {
        return parseVariableDeclarations();
    });
    if (variableDeclarations) {
        UniqueRef<AST::Statement> declarationStatement = makeUniqueRef<AST::VariableDeclarationsStatement>(WTFMove(*variableDeclarations));
        return parseRemainder(WTFMove(declarationStatement));
    }

    PARSE(effectfulExpression, EffectfulExpression);

    return parseRemainder(WTFMove(*effectfulExpression));
}

auto Parser::parseWhileLoop() -> Expected<AST::WhileLoop, Error>
{
    CONSUME_TYPE(origin, While);
    CONSUME_TYPE(leftParenthesis, LeftParenthesis);
    PARSE(conditional, Expression);
    CONSUME_TYPE(rightParenthesis, RightParenthesis);
    PARSE(body, Statement);

    CodeLocation location(origin->codeLocation,  (*body)->codeLocation());
    return AST::WhileLoop(location, WTFMove(*conditional), WTFMove(*body));
}

auto Parser::parseDoWhileLoop() -> Expected<AST::DoWhileLoop, Error>
{
    CONSUME_TYPE(origin, Do);
    PARSE(body, Statement);
    CONSUME_TYPE(whileKeyword, While);
    CONSUME_TYPE(leftParenthesis, LeftParenthesis);
    PARSE(conditional, Expression);
    CONSUME_TYPE(rightParenthesis, RightParenthesis);
    CONSUME_TYPE(semicolon, Semicolon);

    return AST::DoWhileLoop({ *origin, *semicolon}, WTFMove(*body), WTFMove(*conditional));
}

auto Parser::parseVariableDeclaration(Ref<AST::UnnamedType>&& type) -> Expected<AST::VariableDeclaration, Error>
{
    PEEK(origin);

    auto qualifiers = parseQualifiers();

    CONSUME_TYPE(name, Identifier);
    PARSE(semantic, Semantic);

    std::unique_ptr<AST::Expression> initializer = nullptr;
    if (tryType(Token::Type::EqualsSign)) {
        PARSE(initializingExpression, PossibleTernaryConditional);
        initializer = initializingExpression.value().moveToUniquePtr();
    }

    auto endOffset = m_lexer.peek().startOffset();
    return AST::VariableDeclaration({ origin->startOffset(), endOffset, m_lexer.nameSpace() }, WTFMove(qualifiers), { WTFMove(type) }, name->stringView(m_lexer).toString(), WTFMove(*semantic), WTFMove(initializer));
}

auto Parser::parseVariableDeclarations() -> Expected<AST::VariableDeclarationsStatement, Error>
{
    PEEK(origin);

    PARSE(type, Type);

    auto firstVariableDeclaration = parseVariableDeclaration(type->copyRef());
    if (!firstVariableDeclaration)
        return makeUnexpected(firstVariableDeclaration.error());

    Vector<UniqueRef<AST::VariableDeclaration>> result;
    result.append(makeUniqueRef<AST::VariableDeclaration>(WTFMove(*firstVariableDeclaration)));

    while (tryType(Token::Type::Comma)) {
        auto variableDeclaration = parseVariableDeclaration(type->copyRef());
        if (!variableDeclaration)
            return makeUnexpected(variableDeclaration.error());
        result.append(makeUniqueRef<AST::VariableDeclaration>(WTFMove(*variableDeclaration)));
    }

    auto endOffset = m_lexer.peek().startOffset();
    return AST::VariableDeclarationsStatement({ origin->startOffset(), endOffset, m_lexer.nameSpace() }, WTFMove(result));
}

auto Parser::parseStatement() -> Expected<UniqueRef<AST::Statement>, Error>
{
    PEEK(token);
    switch (token->type) {
    case Token::Type::LeftCurlyBracket: {
        PARSE(block, Block);
        return { makeUniqueRef<AST::Block>(WTFMove(*block)) };
    }
    case Token::Type::If: {
        PARSE(ifStatement, IfStatement);
        return { makeUniqueRef<AST::IfStatement>(WTFMove(*ifStatement)) };
    }
    case Token::Type::Switch: {
        PARSE(switchStatement, SwitchStatement);
        return { makeUniqueRef<AST::SwitchStatement>(WTFMove(*switchStatement)) };
    }
    case Token::Type::For: {
        PARSE(forLoop, ForLoop);
        return { makeUniqueRef<AST::ForLoop>(WTFMove(*forLoop)) };
    }
    case Token::Type::While: {
        PARSE(whileLoop, WhileLoop);
        return { makeUniqueRef<AST::WhileLoop>(WTFMove(*whileLoop)) };
    }
    case Token::Type::Do: {
        PARSE(doWhileLoop, DoWhileLoop);
        return { makeUniqueRef<AST::DoWhileLoop>(WTFMove(*doWhileLoop)) };
    }
    case Token::Type::Break: {
        auto breakToken = m_lexer.consumeToken();
        CONSUME_TYPE(semicolon, Semicolon);
        auto breakObject = AST::Break(WTFMove(breakToken));
        return { makeUniqueRef<AST::Break>(WTFMove(breakObject)) };
    }
    case Token::Type::Continue: {
        auto continueToken = m_lexer.consumeToken();
        CONSUME_TYPE(semicolon, Semicolon);
        auto continueObject = AST::Continue(WTFMove(continueToken));
        return { makeUniqueRef<AST::Continue>(WTFMove(continueObject)) };
    }
    case Token::Type::Fallthrough: {
        auto fallthroughToken = m_lexer.consumeToken();
        CONSUME_TYPE(semicolon, Semicolon);
        auto fallthroughObject = AST::Fallthrough(WTFMove(fallthroughToken));
        return { makeUniqueRef<AST::Fallthrough>(WTFMove(fallthroughObject)) };
    }
    case Token::Type::Return: {
        auto returnToken = m_lexer.consumeToken();
        if (auto semicolon = tryType(Token::Type::Semicolon)) {
            auto returnObject = AST::Return(WTFMove(returnToken), nullptr);
            return { makeUniqueRef<AST::Return>(WTFMove(returnObject)) };
        }
        PARSE(expression, Expression);
        CONSUME_TYPE(finalSemicolon, Semicolon);
        auto returnObject = AST::Return(WTFMove(returnToken), (*expression).moveToUniquePtr());
        return { makeUniqueRef<AST::Return>(WTFMove(returnObject)) };
    }
    case Token::Type::Constant:
    case Token::Type::Device:
    case Token::Type::Threadgroup:
    case Token::Type::Thread: {
        PARSE(variableDeclarations, VariableDeclarations);
        CONSUME_TYPE(semicolon, Semicolon);
        return { makeUniqueRef<AST::VariableDeclarationsStatement>(WTFMove(*variableDeclarations)) };
    }
    case Token::Type::Identifier: {
        PEEK_FURTHER(nextToken);
        switch (nextToken->type) {
        case Token::Type::Identifier:
        case Token::Type::LessThanSign:
        case Token::Type::Star:
        case Token::Type::Qualifier: {
            PARSE(variableDeclarations, VariableDeclarations);
            CONSUME_TYPE(semicolon, Semicolon);
            return { makeUniqueRef<AST::VariableDeclarationsStatement>(WTFMove(*variableDeclarations)) };
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }

    {
        auto effectfulExpression = backtrackingScope<Expected<UniqueRef<AST::Statement>, Error>>([&]() -> Expected<UniqueRef<AST::Statement>, Error> {
            PARSE(result, EffectfulExpression);
            CONSUME_TYPE(semicolon, Semicolon);
            return result;
        });
        if (effectfulExpression)
            return WTFMove(*effectfulExpression);
    }

    PARSE(variableDeclarations, VariableDeclarations);
    CONSUME_TYPE(semicolon, Semicolon);
    return { makeUniqueRef<AST::VariableDeclarationsStatement>(WTFMove(*variableDeclarations)) };
}

auto Parser::parseEffectfulExpression() -> Expected<UniqueRef<AST::Statement>, Error>
{
    PEEK(origin);
    if (origin->type == Token::Type::Semicolon)
        return { makeUniqueRef<AST::Block>(*origin, Vector<UniqueRef<AST::Statement>>()) };

    Vector<UniqueRef<AST::Expression>> expressions;
    PARSE(effectfulExpression, EffectfulAssignment);
    expressions.append(WTFMove(*effectfulExpression));

    while (tryType(Token::Type::Comma)) {
        PARSE(expression, EffectfulAssignment);
        expressions.append(WTFMove(*expression));
    }

    if (expressions.size() == 1)
        return { makeUniqueRef<AST::EffectfulExpressionStatement>(WTFMove(expressions[0])) };
    unsigned endOffset = m_lexer.peek().startOffset();
    CodeLocation location(origin->startOffset(), endOffset, m_lexer.nameSpace());
    auto commaExpression = makeUniqueRef<AST::CommaExpression>(location, WTFMove(expressions));
    return { makeUniqueRef<AST::EffectfulExpressionStatement>(WTFMove(commaExpression)) };
}

auto Parser::parseEffectfulAssignment() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PEEK(origin);

    bool isEffectful = false;
    PARSE(expression, PossiblePrefix, &isEffectful);

    if (!isEffectful || peekTypes<
        Token::Type::EqualsSign,
        Token::Type::PlusEquals,
        Token::Type::MinusEquals,
        Token::Type::TimesEquals,
        Token::Type::DivideEquals,
        Token::Type::ModEquals,
        Token::Type::XorEquals,
        Token::Type::AndEquals,
        Token::Type::OrEquals,
        Token::Type::RightShiftEquals,
        Token::Type::LeftShiftEquals
    >()) {
        return completeAssignment(WTFMove(*expression));
    }

    return expression;
}

auto Parser::parseLimitedSuffixOperator(UniqueRef<AST::Expression>&& previous) -> SuffixExpression
{
    auto type = consumeTypes<
        Token::Type::FullStop,
        Token::Type::Arrow,
        Token::Type::LeftSquareBracket>();
    if (!type)
        return SuffixExpression(WTFMove(previous), false);

    switch (type->type) {
    case Token::Type::FullStop: {
        auto identifier = consumeType(Token::Type::Identifier);
        if (!identifier)
            return SuffixExpression(WTFMove(previous), false);
        CodeLocation location(previous->codeLocation(), *identifier);
        return SuffixExpression(makeUniqueRef<AST::DotExpression>(location, WTFMove(previous), identifier->stringView(m_lexer).toString()), true);
    }
    case Token::Type::Arrow: {
        auto identifier = consumeType(Token::Type::Identifier);
        if (!identifier)
            return SuffixExpression(WTFMove(previous), false);
        CodeLocation location(previous->codeLocation(), *identifier);
        return SuffixExpression(makeUniqueRef<AST::DotExpression>(location, makeUniqueRef<AST::DereferenceExpression>(location, WTFMove(previous)), identifier->stringView(m_lexer).toString()), true);
    }
    default: {
        ASSERT(type->type == Token::Type::LeftSquareBracket);
        auto expression = parseExpression();
        if (!expression)
            return SuffixExpression(WTFMove(previous), false);
        if (auto rightSquareBracket = consumeType(Token::Type::RightSquareBracket)) {
            CodeLocation location(previous->codeLocation(), *rightSquareBracket);
            return SuffixExpression(makeUniqueRef<AST::IndexExpression>(location, WTFMove(previous), WTFMove(*expression)), true);
        }
        return SuffixExpression(WTFMove(previous), false);
    }
    }
}

auto Parser::parseSuffixOperator(UniqueRef<AST::Expression>&& previous) -> SuffixExpression
{
    auto suffix = consumeTypes<
        Token::Type::FullStop,
        Token::Type::Arrow,
        Token::Type::LeftSquareBracket,
        Token::Type::PlusPlus,
        Token::Type::MinusMinus>();
    if (!suffix)
        return SuffixExpression(WTFMove(previous), false);

    switch (suffix->type) {
    case Token::Type::FullStop: {
        auto identifier = consumeType(Token::Type::Identifier);
        if (!identifier)
            return SuffixExpression(WTFMove(previous), false);
        CodeLocation location(previous->codeLocation(), *identifier);
        return SuffixExpression(makeUniqueRef<AST::DotExpression>(location, WTFMove(previous), identifier->stringView(m_lexer).toString()), true);
    }
    case Token::Type::Arrow: {
        auto identifier = consumeType(Token::Type::Identifier);
        if (!identifier)
            return SuffixExpression(WTFMove(previous), false);
        CodeLocation location(previous->codeLocation(), *identifier);
        return SuffixExpression(makeUniqueRef<AST::DotExpression>(location, makeUniqueRef<AST::DereferenceExpression>(WTFMove(*suffix), WTFMove(previous)), identifier->stringView(m_lexer).toString()), true);
    }
    case Token::Type::LeftSquareBracket: {
        auto expression = parseExpression();
        if (!expression)
            return SuffixExpression(WTFMove(previous), false);
        if (auto rightSquareBracket = consumeType(Token::Type::RightSquareBracket)) {
            CodeLocation location(previous->codeLocation(), *rightSquareBracket);
            return SuffixExpression(makeUniqueRef<AST::IndexExpression>(location, WTFMove(previous), WTFMove(*expression)), true);
        }
        return SuffixExpression(WTFMove(previous), false);
    }
    case Token::Type::PlusPlus: {
        CodeLocation location(previous->codeLocation(), *suffix);
        auto result = makeUniqueRef<AST::ReadModifyWriteExpression>(location, WTFMove(previous));
        Vector<UniqueRef<AST::Expression>> callArguments;
        callArguments.append(result->oldVariableReference());
        result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(location, "operator++"_str, WTFMove(callArguments)));
        result->setResultExpression(result->oldVariableReference());
        return SuffixExpression(WTFMove(result), true);
    }
    default: {
        ASSERT(suffix->type == Token::Type::MinusMinus);
        CodeLocation location(previous->codeLocation(), *suffix);
        auto result = makeUniqueRef<AST::ReadModifyWriteExpression>(location, WTFMove(previous));
        Vector<UniqueRef<AST::Expression>> callArguments;
        callArguments.append(result->oldVariableReference());
        result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(location, "operator--"_str, WTFMove(callArguments)));
        result->setResultExpression(result->oldVariableReference());
        return SuffixExpression(WTFMove(result), true);
    }
    }
}

auto Parser::parseExpression() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PARSE(first, PossibleTernaryConditional);
    Vector<UniqueRef<AST::Expression>> expressions;
    unsigned startOffset = (*first)->codeLocation().startOffset();
    expressions.append(WTFMove(*first));

    while (tryType(Token::Type::Comma)) {
        PARSE(expression, PossibleTernaryConditional);
        expressions.append(WTFMove(*expression));
    }

    if (expressions.size() == 1)
        return WTFMove(expressions[0]);
    auto endOffset = m_lexer.peek().startOffset();
    CodeLocation location(startOffset, endOffset, m_lexer.nameSpace());
    return { makeUniqueRef<AST::CommaExpression>(location, WTFMove(expressions)) };
}

auto Parser::completeTernaryConditional(UniqueRef<AST::Expression>&& predicate) -> Expected<UniqueRef<AST::Expression>, Error>
{
    CONSUME_TYPE(questionMark, QuestionMark);
    PARSE(bodyExpression, Expression);
    CONSUME_TYPE(colon, Colon);
    PARSE(elseExpression, PossibleTernaryConditional);

    CodeLocation predicateLocation = predicate->codeLocation();
    Vector<UniqueRef<AST::Expression>> castArguments;
    castArguments.append(WTFMove(predicate));
    auto boolCast = makeUniqueRef<AST::CallExpression>(predicateLocation, "bool"_str, WTFMove(castArguments));
    CodeLocation location(predicateLocation, (*elseExpression)->codeLocation());
    return { makeUniqueRef<AST::TernaryExpression>(location, WTFMove(boolCast), WTFMove(*bodyExpression), WTFMove(*elseExpression)) };
}

auto Parser::completeAssignment(UniqueRef<AST::Expression>&& left) -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto assignmentOperator = consumeTypes<
        Token::Type::EqualsSign,
        Token::Type::PlusEquals,
        Token::Type::MinusEquals,
        Token::Type::TimesEquals,
        Token::Type::DivideEquals,
        Token::Type::ModEquals,
        Token::Type::XorEquals,
        Token::Type::AndEquals,
        Token::Type::OrEquals,
        Token::Type::RightShiftEquals,
        Token::Type::LeftShiftEquals>();
    if (!assignmentOperator)
        return makeUnexpected(assignmentOperator.error());

    PARSE(right, PossibleTernaryConditional);
    CodeLocation location = { left->codeLocation(), (*right)->codeLocation() };

    if (assignmentOperator->type == Token::Type::EqualsSign)
        return { makeUniqueRef<AST::AssignmentExpression>(location, WTFMove(left), WTFMove(*right))};

    String name;
    switch (assignmentOperator->type) {
    case Token::Type::PlusEquals:
        name = "operator+"_str;
        break;
    case Token::Type::MinusEquals:
        name = "operator-"_str;
        break;
    case Token::Type::TimesEquals:
        name = "operator*"_str;
        break;
    case Token::Type::DivideEquals:
        name = "operator/"_str;
        break;
    case Token::Type::ModEquals:
        name = "operator%"_str;
        break;
    case Token::Type::XorEquals:
        name = "operator^"_str;
        break;
    case Token::Type::AndEquals:
        name = "operator&"_str;
        break;
    case Token::Type::OrEquals:
        name = "operator|"_str;
        break;
    case Token::Type::RightShiftEquals:
        name = "operator>>"_str;
        break;
    default:
        ASSERT(assignmentOperator->type == Token::Type::LeftShiftEquals);
        name = "operator<<"_str;
        break;
    }

    auto result = makeUniqueRef<AST::ReadModifyWriteExpression>(location, WTFMove(left));
    Vector<UniqueRef<AST::Expression>> callArguments;
    callArguments.append(result->oldVariableReference());
    callArguments.append(WTFMove(*right));
    result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(location, WTFMove(name), WTFMove(callArguments)));
    result->setResultExpression(result->newVariableReference());
    return { WTFMove(result) };
}

auto Parser::parsePossibleTernaryConditional() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PARSE(expression, PossiblePrefix);

    if (peekTypes<Token::Type::EqualsSign,
        Token::Type::PlusEquals,
        Token::Type::MinusEquals,
        Token::Type::TimesEquals,
        Token::Type::DivideEquals,
        Token::Type::ModEquals,
        Token::Type::XorEquals,
        Token::Type::AndEquals,
        Token::Type::OrEquals,
        Token::Type::RightShiftEquals,
        Token::Type::LeftShiftEquals>()) {
        return completeAssignment(WTFMove(*expression));
    }

    expression = completePossibleShift(WTFMove(*expression));
    if (!expression)
        return makeUnexpected(expression.error());

    expression = completePossibleMultiply(WTFMove(*expression));
    if (!expression)
        return makeUnexpected(expression.error());

    expression = completePossibleAdd(WTFMove(*expression));
    if (!expression)
        return makeUnexpected(expression.error());

    expression = completePossibleRelationalBinaryOperation(WTFMove(*expression));
    if (!expression)
        return makeUnexpected(expression.error());

    expression = completePossibleLogicalBinaryOperation(WTFMove(*expression));
    if (!expression)
        return makeUnexpected(expression.error());

    PEEK(nextToken);
    if (nextToken->type == Token::Type::QuestionMark)
        return completeTernaryConditional(WTFMove(*expression));
    return expression;
}

auto Parser::parsePossibleLogicalBinaryOperation() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PARSE(parsedPrevious, PossibleRelationalBinaryOperation);
    return completePossibleLogicalBinaryOperation(WTFMove(*parsedPrevious));
}

auto Parser::completePossibleLogicalBinaryOperation(UniqueRef<AST::Expression>&& previous) -> Expected<UniqueRef<AST::Expression>, Error>
{
    while (auto logicalBinaryOperation = tryTypes<
        Token::Type::OrOr,
        Token::Type::AndAnd,
        Token::Type::Or,
        Token::Type::Xor,
        Token::Type::And
        >()) {
        PARSE(next, PossibleRelationalBinaryOperation);
        CodeLocation location(previous->codeLocation(), (*next)->codeLocation());

        switch (logicalBinaryOperation->type) {
        case Token::Type::OrOr:
            previous = makeUniqueRef<AST::LogicalExpression>(location, AST::LogicalExpression::Type::Or, WTFMove(previous), WTFMove(*next));
            break;
        case Token::Type::AndAnd:
            previous = makeUniqueRef<AST::LogicalExpression>(location, AST::LogicalExpression::Type::And, WTFMove(previous), WTFMove(*next));
            break;
        case Token::Type::Or: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(location, "operator|"_str, WTFMove(callArguments));
            break;
        }
        case Token::Type::Xor: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(location, "operator^"_str, WTFMove(callArguments));
            break;
        }
        default: {
            ASSERT(logicalBinaryOperation->type == Token::Type::And);
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(location, "operator&"_str, WTFMove(callArguments));
            break;
        }
        }
    }

    return { WTFMove(previous) };
}

auto Parser::parsePossibleRelationalBinaryOperation() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PARSE(parsedPrevious, PossibleShift);
    return completePossibleRelationalBinaryOperation(WTFMove(*parsedPrevious));
}

auto Parser::completePossibleRelationalBinaryOperation(UniqueRef<AST::Expression>&& previous) -> Expected<UniqueRef<AST::Expression>, Error>
{
    while (auto relationalBinaryOperation = tryTypes<
        Token::Type::LessThanSign,
        Token::Type::GreaterThanSign,
        Token::Type::LessThanOrEqualTo,
        Token::Type::GreaterThanOrEqualTo,
        Token::Type::EqualComparison,
        Token::Type::NotEqual
        >()) {
        PARSE(next, PossibleShift);
        CodeLocation location(previous->codeLocation(), (*next)->codeLocation());

        Vector<UniqueRef<AST::Expression>> callArguments;
        callArguments.append(WTFMove(previous));
        callArguments.append(WTFMove(*next));

        switch (relationalBinaryOperation->type) {
        case Token::Type::LessThanSign: {
            previous = makeUniqueRef<AST::CallExpression>(location, "operator<"_str, WTFMove(callArguments));
            break;
        }
        case Token::Type::GreaterThanSign: {
            previous = makeUniqueRef<AST::CallExpression>(location, "operator>"_str, WTFMove(callArguments));
            break;
        }
        case Token::Type::LessThanOrEqualTo: {
            previous = makeUniqueRef<AST::CallExpression>(location, "operator<="_str, WTFMove(callArguments));
            break;
        }
        case Token::Type::GreaterThanOrEqualTo: {
            previous = makeUniqueRef<AST::CallExpression>(location, "operator>="_str, WTFMove(callArguments));
            break;
        }
        case Token::Type::EqualComparison: {
            previous = makeUniqueRef<AST::CallExpression>(location, "operator=="_str, WTFMove(callArguments));
            break;
        }
        default: {
            ASSERT(relationalBinaryOperation->type == Token::Type::NotEqual);
            previous = makeUniqueRef<AST::CallExpression>(location, "operator=="_str, WTFMove(callArguments));
            previous = makeUniqueRef<AST::LogicalNotExpression>(location, WTFMove(previous));
            break;
        }
        }
    }

    return WTFMove(previous);
}

auto Parser::parsePossibleShift() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PARSE(parsedPrevious, PossibleAdd);
    return completePossibleShift(WTFMove(*parsedPrevious));
}

auto Parser::completePossibleShift(UniqueRef<AST::Expression>&& previous) -> Expected<UniqueRef<AST::Expression>, Error>
{
    while (auto shift = tryTypes<
        Token::Type::LeftShift,
        Token::Type::RightShift
        >()) {
        PARSE(next, PossibleAdd);
        CodeLocation location(previous->codeLocation(), (*next)->codeLocation());

        Vector<UniqueRef<AST::Expression>> callArguments;
        callArguments.append(WTFMove(previous));
        callArguments.append(WTFMove(*next));

        switch (shift->type) {
        case Token::Type::LeftShift: {
            previous = makeUniqueRef<AST::CallExpression>(location, "operator<<"_str, WTFMove(callArguments));
            break;
        }
        default: {
            ASSERT(shift->type == Token::Type::RightShift);
            previous = makeUniqueRef<AST::CallExpression>(location, "operator>>"_str, WTFMove(callArguments));
            break;
        }
        }
    }

    return WTFMove(previous);
}

auto Parser::parsePossibleAdd() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PARSE(parsedPrevious, PossibleMultiply);
    return completePossibleAdd(WTFMove(*parsedPrevious));
}

auto Parser::completePossibleAdd(UniqueRef<AST::Expression>&& previous) -> Expected<UniqueRef<AST::Expression>, Error>
{
    while (auto add = tryTypes<
        Token::Type::Plus,
        Token::Type::Minus
        >()) {
        PARSE(next, PossibleMultiply);
        CodeLocation location(previous->codeLocation(), (*next)->codeLocation());

        Vector<UniqueRef<AST::Expression>> callArguments;
        callArguments.append(WTFMove(previous));
        callArguments.append(WTFMove(*next));

        switch (add->type) {
        case Token::Type::Plus: {
            previous = makeUniqueRef<AST::CallExpression>(location, "operator+"_str, WTFMove(callArguments));
            break;
        }
        default: {
            ASSERT(add->type == Token::Type::Minus);
            previous = makeUniqueRef<AST::CallExpression>(location, "operator-"_str, WTFMove(callArguments));
            break;
        }
        }
    }

    return WTFMove(previous);
}

auto Parser::parsePossibleMultiply() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PARSE(parsedPrevious, PossiblePrefix);
    return completePossibleMultiply(WTFMove(*parsedPrevious));
}

auto Parser::completePossibleMultiply(UniqueRef<AST::Expression>&& previous) -> Expected<UniqueRef<AST::Expression>, Error>
{
    while (auto multiply = tryTypes<
        Token::Type::Star,
        Token::Type::Divide,
        Token::Type::Mod
        >()) {
        PARSE(next, PossiblePrefix);
        CodeLocation location(previous->codeLocation(), (*next)->codeLocation());

        Vector<UniqueRef<AST::Expression>> callArguments;
        callArguments.append(WTFMove(previous));
        callArguments.append(WTFMove(*next));

        switch (multiply->type) {
        case Token::Type::Star: {
            previous = makeUniqueRef<AST::CallExpression>(location, "operator*"_str, WTFMove(callArguments));
            break;
        }
        case Token::Type::Divide: {
            previous = makeUniqueRef<AST::CallExpression>(location, "operator/"_str, WTFMove(callArguments));
            break;
        }
        default: {
            ASSERT(multiply->type == Token::Type::Mod);
            previous = makeUniqueRef<AST::CallExpression>(location, "operator%"_str, WTFMove(callArguments));
            break;
        }
        }
    }

    return WTFMove(previous);
}

auto Parser::parsePossiblePrefix(bool *isEffectful) -> Expected<UniqueRef<AST::Expression>, Error>
{
    if (auto prefix = tryTypes<
        Token::Type::PlusPlus,
        Token::Type::MinusMinus,
        Token::Type::Plus,
        Token::Type::Minus,
        Token::Type::Tilde,
        Token::Type::ExclamationPoint,
        Token::Type::And,
        Token::Type::At,
        Token::Type::Star
    >()) {
        PARSE(next, PossiblePrefix);
        CodeLocation location(*prefix, (*next)->codeLocation());

        switch (prefix->type) {
        case Token::Type::PlusPlus: {
            if (isEffectful)
                *isEffectful = true;
            auto result = makeUniqueRef<AST::ReadModifyWriteExpression>(location, WTFMove(*next));
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(result->oldVariableReference());
            result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(location, "operator++"_str, WTFMove(callArguments)));
            result->setResultExpression(result->newVariableReference());
            return { WTFMove(result) };
        }
        case Token::Type::MinusMinus: {
            if (isEffectful)
                *isEffectful = true;
            auto result = makeUniqueRef<AST::ReadModifyWriteExpression>(location, WTFMove(*next));
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(result->oldVariableReference());
            result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(location, "operator--"_str, WTFMove(callArguments)));
            result->setResultExpression(result->newVariableReference());
            return { WTFMove(result) };
        }
        case Token::Type::Plus: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(*next));
            return { makeUniqueRef<AST::CallExpression>(location, "operator+"_str, WTFMove(callArguments)) };
        }
        case Token::Type::Minus: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(*next));
            return { makeUniqueRef<AST::CallExpression>(location, "operator-"_str, WTFMove(callArguments)) };
        }
        case Token::Type::Tilde: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(*next));
            return { makeUniqueRef<AST::CallExpression>(location, "operator~"_str, WTFMove(callArguments)) };
        }
        case Token::Type::ExclamationPoint: {
            Vector<UniqueRef<AST::Expression>> castArguments;
            castArguments.append(WTFMove(*next));
            auto boolCast = makeUniqueRef<AST::CallExpression>(location, "bool"_str, WTFMove(castArguments));
            return { makeUniqueRef<AST::LogicalNotExpression>(location, WTFMove(boolCast)) };
        }
        case Token::Type::And:
            return { makeUniqueRef<AST::MakePointerExpression>(location, WTFMove(*next), AST::AddressEscapeMode::Escapes) };
        case Token::Type::At:
            return { makeUniqueRef<AST::MakeArrayReferenceExpression>(location, WTFMove(*next), AST::AddressEscapeMode::Escapes) };
        default:
            ASSERT(prefix->type == Token::Type::Star);
            return { makeUniqueRef<AST::DereferenceExpression>(location, WTFMove(*next)) };
        }
    }

    return parsePossibleSuffix(isEffectful);
}

auto Parser::parsePossibleSuffix(bool *isEffectful) -> Expected<UniqueRef<AST::Expression>, Error>
{
    PEEK(token);
    PEEK_FURTHER(nextToken);

    if (token->type == Token::Type::Identifier && nextToken->type == Token::Type::LeftParenthesis) {
        PARSE(expression, CallExpression);
        if (isEffectful)
            *isEffectful = true;
        while (true) {
            PEEK(suffixToken);
            if (suffixToken->type != Token::Type::FullStop && suffixToken->type != Token::Type::Arrow && suffixToken->type != Token::Type::LeftSquareBracket)
                break;
            auto result = parseLimitedSuffixOperator(WTFMove(*expression));
            expression = WTFMove(result.result);
        }
        return expression;
    }

    if (token->type == Token::Type::LeftParenthesis && isEffectful)
        *isEffectful = true;

    PARSE(expression, Term);
    bool isLastSuffixTokenEffectful = false;
    while (true) {
        PEEK(suffixToken);
        if (suffixToken->type != Token::Type::FullStop
            && suffixToken->type != Token::Type::Arrow
            && suffixToken->type != Token::Type::LeftSquareBracket
            && suffixToken->type != Token::Type::PlusPlus
            && suffixToken->type != Token::Type::MinusMinus) {
            break;
        }
        isLastSuffixTokenEffectful = suffixToken->type == Token::Type::PlusPlus || suffixToken->type == Token::Type::MinusMinus;
        auto result = parseSuffixOperator(WTFMove(*expression));
        expression = WTFMove(result.result);
    }
    if (isLastSuffixTokenEffectful && isEffectful)
        *isEffectful = true;
    return expression;
}

auto Parser::parseCallExpression() -> Expected<UniqueRef<AST::Expression>, Error>
{
    CONSUME_TYPE(name, Identifier);
    auto callName = name->stringView(m_lexer).toString();

    CONSUME_TYPE(leftParenthesis, LeftParenthesis);

    Vector<UniqueRef<AST::Expression>> arguments;
    if (tryType(Token::Type::RightParenthesis))
        return { makeUniqueRef<AST::CallExpression>(WTFMove(*name), WTFMove(callName), WTFMove(arguments)) };

    PARSE(firstArgument, PossibleTernaryConditional);
    arguments.append(WTFMove(*firstArgument));
    while (tryType(Token::Type::Comma)) {
        PARSE(argument, PossibleTernaryConditional);
        arguments.append(WTFMove(*argument));
    }

    CONSUME_TYPE(rightParenthesis, RightParenthesis);
    CodeLocation location(*name, *rightParenthesis);

    return { makeUniqueRef<AST::CallExpression>(location, WTFMove(callName), WTFMove(arguments)) };
}

auto Parser::parseTerm() -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto type = consumeTypes<
        Token::Type::IntLiteral,
        Token::Type::UintLiteral,
        Token::Type::FloatLiteral,
        Token::Type::Null,
        Token::Type::True,
        Token::Type::False,
        Token::Type::Identifier,
        Token::Type::LeftParenthesis>();
    if (!type)
        return makeUnexpected(type.error());

    switch (type->type) {
    case Token::Type::IntLiteral: {
        auto value = intLiteralToInt(type->stringView(m_lexer));
        if (!value)
            return makeUnexpected(value.error());
        return { makeUniqueRef<AST::IntegerLiteral>(*type, *value) };
    }
    case Token::Type::UintLiteral: {
        auto value = uintLiteralToUint(type->stringView(m_lexer));
        if (!value)
            return makeUnexpected(value.error());
        return { makeUniqueRef<AST::UnsignedIntegerLiteral>(*type, *value) };
    }
    case Token::Type::FloatLiteral: {
        auto value = floatLiteralToFloat(type->stringView(m_lexer));
        if (!value)
            return makeUnexpected(value.error());
        return { makeUniqueRef<AST::FloatLiteral>(*type, *value) };
    }
    case Token::Type::Null:
        return makeUnexpected(Error("'null' is a reserved keyword.", type->codeLocation));
    case Token::Type::True:
        return { makeUniqueRef<AST::BooleanLiteral>(*type, true) };
    case Token::Type::False:
        return { makeUniqueRef<AST::BooleanLiteral>(*type, false) };
    case Token::Type::Identifier: {
        auto name = type->stringView(m_lexer).toString();
        return { makeUniqueRef<AST::VariableReference>(*type, WTFMove(name)) };
    }
    default: {
        ASSERT(type->type == Token::Type::LeftParenthesis);
        PARSE(expression, Expression);
        CONSUME_TYPE(rightParenthesis, RightParenthesis);

        return { WTFMove(*expression) };
    }
    }
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
