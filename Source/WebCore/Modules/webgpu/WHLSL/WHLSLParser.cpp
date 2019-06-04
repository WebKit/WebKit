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
#include <wtf/dtoa.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

namespace WHLSL {

#define PARSE(name, element, ...) \
    auto name = parse##element(__VA_ARGS__); \
    if (!name) \
        return Unexpected<Error>(name.error());

#define CONSUME_TYPE(name, type) \
    auto name = consumeType(Lexer::Token::Type::type); \
    if (!name) \
        return Unexpected<Error>(name.error());

#define PEEK(name) \
    auto name = peek(); \
    if (!name) \
        return Unexpected<Error>(name.error());

#define PEEK_FURTHER(name) \
    auto name = peekFurther(); \
    if (!name) \
        return Unexpected<Error>(name.error());

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=195682 Return a better error code from this, and report it to JavaScript.
auto Parser::parse(Program& program, StringView stringView, Mode mode) -> Optional<Error>
{
    m_lexer = Lexer(stringView);
    m_mode = mode;

    while (!m_lexer.isFullyConsumed()) {
        auto token = m_lexer.peek();
        if (!token)
            break;
        switch (token->type) {
        case Lexer::Token::Type::Semicolon:
            m_lexer.consumeToken();
            continue;
        case Lexer::Token::Type::Typedef: {
            auto typeDefinition = parseTypeDefinition();
            if (!typeDefinition)
                return typeDefinition.error();
            program.append(WTFMove(*typeDefinition));
            continue;
        }
        case Lexer::Token::Type::Struct: {
            auto structureDefinition = parseStructureDefinition();
            if (!structureDefinition)
                return structureDefinition.error();
            program.append(WTFMove(*structureDefinition));
            continue;
        }
        case Lexer::Token::Type::Enum: {
            auto enumerationDefinition = parseEnumerationDefinition();
            if (!enumerationDefinition)
                return enumerationDefinition.error();
            program.append(WTFMove(*enumerationDefinition));
            continue;
        }
        case Lexer::Token::Type::Native: {
            ASSERT(m_mode == Mode::StandardLibrary);
            auto furtherToken = peekFurther();
            if (!furtherToken)
                return WTF::nullopt;
            if (furtherToken->type == Lexer::Token::Type::Typedef) {
                auto nativeTypeDeclaration = parseNativeTypeDeclaration();
                if (!nativeTypeDeclaration)
                    return nativeTypeDeclaration.error();
                program.append(WTFMove(*nativeTypeDeclaration));
                continue;
            }
            auto nativeFunctionDeclaration = parseNativeFunctionDeclaration();
            if (!nativeFunctionDeclaration)
                return nativeFunctionDeclaration.error();
            program.append(WTFMove(*nativeFunctionDeclaration));
            continue;
        }
        default: {
            auto functionDefinition = parseFunctionDefinition();
            if (!functionDefinition)
                return functionDefinition.error();
            program.append(WTFMove(*functionDefinition));
            continue;
        }
        }
    }

    return WTF::nullopt;
}

auto Parser::fail(const String& message, TryToPeek tryToPeek) -> Unexpected<Error>
{
    if (tryToPeek == TryToPeek::Yes) {
        if (auto nextToken = peek())
            return Unexpected<Error>(Error(m_lexer.errorString(*nextToken, message)));
    }
    return Unexpected<Error>(Error(makeString("Cannot lex: ", message)));
}

auto Parser::peek() -> Expected<Lexer::Token, Error>
{
    if (auto token = m_lexer.peek()) {
        return *token;
    }
    return fail("Cannot consume token"_str, TryToPeek::No);
}

auto Parser::peekFurther() -> Expected<Lexer::Token, Error>
{
    if (auto token = m_lexer.peekFurther()) {
        return *token;
    }
    return fail("Cannot consume two tokens"_str, TryToPeek::No);
}

bool Parser::peekTypes(const Vector<Lexer::Token::Type>& types)
{
    if (auto token = m_lexer.peek())
        return std::find(types.begin(), types.end(), token->type) != types.end();
    return false;
}

Optional<Lexer::Token> Parser::tryType(Lexer::Token::Type type)
{
    if (auto token = m_lexer.peek()) {
        if (token->type == type)
            return m_lexer.consumeToken();
    }
    return WTF::nullopt;
}

Optional<Lexer::Token> Parser::tryTypes(const Vector<Lexer::Token::Type>& types)
{
    if (auto token = m_lexer.peek()) {
        if (std::find(types.begin(), types.end(), token->type) != types.end())
            return m_lexer.consumeToken();
    }
    return WTF::nullopt;
}

auto Parser::consumeType(Lexer::Token::Type type) -> Expected<Lexer::Token, Error>
{
    if (auto token = m_lexer.consumeToken()) {
        if (token->type == type)
            return *token;
        return fail(makeString("Unexpected token (expected ", Lexer::Token::typeName(type), " got ", Lexer::Token::typeName(token->type), ")"));
    }
    return fail(makeString("Cannot consume token (expected ", Lexer::Token::typeName(type), ")"));
}

auto Parser::consumeTypes(const Vector<Lexer::Token::Type>& types) -> Expected<Lexer::Token, Error>
{
    auto buildExpectedString = [&]() -> String {
        StringBuilder builder;
        builder.append("[");
        for (unsigned i = 0; i < types.size(); ++i) {
            if (i > 0)
                builder.append(", ");
            builder.append(Lexer::Token::typeName(types[i]));
        }
        builder.append("]");
        return builder.toString();
    };

    if (auto token = m_lexer.consumeToken()) {
        if (std::find(types.begin(), types.end(), token->type) != types.end())
            return *token;
        return fail(makeString("Unexpected token (expected one of ", buildExpectedString(), " got ", Lexer::Token::typeName(token->type), ")"));
    }
    return fail(makeString("Cannot consume token (expected ", buildExpectedString(), ")"));
}

static int digitValue(UChar character)
{
    if (character >= '0' && character <= '9')
        return character - '0';
    if (character >= 'a' && character <= 'f')
        return character - 'a' + 10;
    return character - 'A' + 10;
}

static Expected<int, Parser::Error> intLiteralToInt(StringView text)
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
            return Unexpected<Parser::Error>(Parser::Error(makeString("int literal ", text, " is out of bounds")));
    }
    if (negate) {
        static_assert(sizeof(int64_t) > sizeof(unsigned) && sizeof(int64_t) > sizeof(int), "This code would be wrong otherwise");
        int64_t intResult = -static_cast<int64_t>(result);
        if (intResult < static_cast<int64_t>(std::numeric_limits<int>::min()))
            return Unexpected<Parser::Error>(Parser::Error(makeString("int literal ", text, " is out of bounds")));
        return { static_cast<int>(intResult) };
    }
    if (result > static_cast<unsigned>(std::numeric_limits<int>::max()))
        return Unexpected<Parser::Error>(Parser::Error(makeString("int literal ", text, " is out of bounds")));
    return { static_cast<int>(result) };
}

static Expected<unsigned, Parser::Error> uintLiteralToUint(StringView text)
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
            return Unexpected<Parser::Error>(Parser::Error(makeString("uint literal ", text, " is out of bounds")));
    }
    return { result };
}

static Expected<float, Parser::Error> floatLiteralToFloat(StringView text)
{
    size_t parsedLength;
    auto result = parseDouble(text, parsedLength);
    if (parsedLength != text.length())
        return Unexpected<Parser::Error>(Parser::Error(makeString("Cannot parse float ", text)));
    return static_cast<float>(result);
}

auto Parser::consumeIntegralLiteral() -> Expected<Variant<int, unsigned>, Error>
{
    auto integralLiteralToken = consumeTypes({ Lexer::Token::Type::IntLiteral, Lexer::Token::Type::UintLiteral });
    if (!integralLiteralToken)
        return Unexpected<Error>(integralLiteralToken.error());

    switch (integralLiteralToken->type) {
    case Lexer::Token::Type::IntLiteral: {
        auto result = intLiteralToInt(integralLiteralToken->stringView);
        if (result)
            return {{ *result }};
        return Unexpected<Error>(result.error());
    }
    default: {
        ASSERT(integralLiteralToken->type == Lexer::Token::Type::UintLiteral);
        auto result = uintLiteralToUint(integralLiteralToken->stringView);
        if (result)
            return {{ *result }};
        return Unexpected<Error>(result.error());
    }
    }
}

auto Parser::consumeNonNegativeIntegralLiteral() -> Expected<unsigned, Error>
{
    auto integralLiteral = consumeIntegralLiteral();
    if (!integralLiteral)
        return Unexpected<Error>(integralLiteral.error());
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

static Expected<unsigned, Parser::Error> recognizeSimpleUnsignedInteger(StringView stringView)
{
    unsigned result = 0;
    if (stringView.length() < 1)
        return Unexpected<Parser::Error>(Parser::Error(makeString("Simple unsigned literal ", stringView, " is too short")));
    for (auto codePoint : stringView.codePoints()) {
        if (codePoint < '0' || codePoint > '9')
            return Unexpected<Parser::Error>(Parser::Error(makeString("Simple unsigned literal ", stringView, " isn't of the form [0-9]+")));
        auto previous = result;
        result = result * 10 + (codePoint - '0');
        if (result < previous)
            return Unexpected<Parser::Error>(Parser::Error(makeString("Simple unsigned literal ", stringView, " is out of bounds")));
    }
    return result;
}

auto Parser::parseConstantExpression() -> Expected<AST::ConstantExpression, Error>
{
    auto type = consumeTypes({
        Lexer::Token::Type::IntLiteral,
        Lexer::Token::Type::UintLiteral,
        Lexer::Token::Type::FloatLiteral,
        Lexer::Token::Type::Null,
        Lexer::Token::Type::True,
        Lexer::Token::Type::False,
        Lexer::Token::Type::Identifier,
    });
    if (!type)
        return Unexpected<Error>(type.error());

    switch (type->type) {
    case Lexer::Token::Type::IntLiteral: {
        auto value = intLiteralToInt(type->stringView);
        if (!value)
            return Unexpected<Error>(value.error());
        return {{ AST::IntegerLiteral(WTFMove(*type), *value) }};
    }
    case Lexer::Token::Type::UintLiteral: {
        auto value = uintLiteralToUint(type->stringView);
        if (!value)
            return Unexpected<Error>(value.error());
        return {{ AST::UnsignedIntegerLiteral(WTFMove(*type), *value) }};
    }
    case Lexer::Token::Type::FloatLiteral: {
        auto value = floatLiteralToFloat(type->stringView);
        if (!value)
            return Unexpected<Error>(value.error());
        return {{ AST::FloatLiteral(WTFMove(*type), *value) }};
    }
    case Lexer::Token::Type::Null:
        return { AST::NullLiteral(WTFMove(*type)) };
    case Lexer::Token::Type::True:
        return { AST::BooleanLiteral(WTFMove(*type), true) };
    case Lexer::Token::Type::False:
        return { AST::BooleanLiteral(WTFMove(*type), false) };
    default: {
        ASSERT(type->type == Lexer::Token::Type::Identifier);
        CONSUME_TYPE(origin, FullStop);
        CONSUME_TYPE(next, Identifier);
        return { AST::EnumerationMemberLiteral(WTFMove(*origin), type->stringView.toString(), next->stringView.toString()) };
    }
    }
}

auto Parser::parseTypeArgument() -> Expected<AST::TypeArgument, Error>
{
    PEEK(nextToken);
    PEEK_FURTHER(furtherToken);
    if (nextToken->type != Lexer::Token::Type::Identifier || furtherToken->type == Lexer::Token::Type::FullStop) {
        PARSE(constantExpression, ConstantExpression);
        return AST::TypeArgument(WTFMove(*constantExpression));
    }
    CONSUME_TYPE(result, Identifier);
    return AST::TypeArgument(makeUniqueRef<AST::TypeReference>(Lexer::Token(*result), result->stringView.toString(), AST::TypeArguments()));
}

auto Parser::parseTypeArguments() -> Expected<AST::TypeArguments, Error>
{
    AST::TypeArguments typeArguments;
    auto lessThanSign = tryType(Lexer::Token::Type::LessThanSign);
    if (!lessThanSign)
        return typeArguments;

    auto greaterThanSign = tryType(Lexer::Token::Type::GreaterThanSign);
    if (greaterThanSign)
        return typeArguments;

    PARSE(typeArgument, TypeArgument);
    typeArguments.append(WTFMove(*typeArgument));

    while (true) {
        auto greaterThanSign = tryType(Lexer::Token::Type::GreaterThanSign);
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
    auto token = consumeTypes({ Lexer::Token::Type::Star, Lexer::Token::Type::SquareBracketPair, Lexer::Token::Type::LeftSquareBracket });
    if (!token)
        return Unexpected<Error>(token.error());
    if (token->type == Lexer::Token::Type::LeftSquareBracket) {
        auto numElements = consumeNonNegativeIntegralLiteral();
        if (!numElements)
            return Unexpected<Error>(numElements.error());
        CONSUME_TYPE(rightSquareBracket, RightSquareBracket);
        return {{ *token, *numElements }};
    }
    return {{ *token, WTF::nullopt }};
}

auto Parser::parseTypeSuffixNonAbbreviated() -> Expected<TypeSuffixNonAbbreviated, Error>
{
    auto token = consumeTypes({ Lexer::Token::Type::Star, Lexer::Token::Type::SquareBracketPair, Lexer::Token::Type::LeftSquareBracket });
    if (!token)
        return Unexpected<Error>(token.error());
    if (token->type == Lexer::Token::Type::LeftSquareBracket) {
        auto numElements = consumeNonNegativeIntegralLiteral();
        if (!numElements)
            return Unexpected<Error>(numElements.error());
        CONSUME_TYPE(rightSquareBracket, RightSquareBracket);
        return {{ *token, WTF::nullopt, *numElements }};
    }
    auto addressSpaceToken = consumeTypes({ Lexer::Token::Type::Constant, Lexer::Token::Type::Device, Lexer::Token::Type::Threadgroup, Lexer::Token::Type::Thread});
    if (!addressSpaceToken)
        return Unexpected<Error>(addressSpaceToken.error());
    AST::AddressSpace addressSpace;
    switch (addressSpaceToken->type) {
    case Lexer::Token::Type::Constant:
        addressSpace = AST::AddressSpace::Constant;
        break;
    case Lexer::Token::Type::Device:
        addressSpace = AST::AddressSpace::Device;
        break;
    case Lexer::Token::Type::Threadgroup:
        addressSpace = AST::AddressSpace::Threadgroup;
        break;
    default:
        ASSERT(addressSpaceToken->type == Lexer::Token::Type::Thread);
        addressSpace = AST::AddressSpace::Thread;
        break;
    }
    return {{ *token, { addressSpace }, WTF::nullopt }};
}

auto Parser::parseType() -> Expected<UniqueRef<AST::UnnamedType>, Error>
{
    auto addressSpaceToken = tryTypes({ Lexer::Token::Type::Constant, Lexer::Token::Type::Device, Lexer::Token::Type::Threadgroup, Lexer::Token::Type::Thread});

    CONSUME_TYPE(name, Identifier);
    PARSE(typeArguments, TypeArguments);

    if (addressSpaceToken) {
        AST::AddressSpace addressSpace;
        switch (addressSpaceToken->type) {
        case Lexer::Token::Type::Constant:
            addressSpace = AST::AddressSpace::Constant;
            break;
        case Lexer::Token::Type::Device:
            addressSpace = AST::AddressSpace::Device;
            break;
        case Lexer::Token::Type::Threadgroup:
            addressSpace = AST::AddressSpace::Threadgroup;
            break;
        default:
            ASSERT(addressSpaceToken->type == Lexer::Token::Type::Thread);
            addressSpace = AST::AddressSpace::Thread;
            break;
        }
        auto constructTypeFromSuffixAbbreviated = [&](const TypeSuffixAbbreviated& typeSuffixAbbreviated, UniqueRef<AST::UnnamedType>&& previous) -> UniqueRef<AST::UnnamedType> {
            switch (typeSuffixAbbreviated.token.type) {
            case Lexer::Token::Type::Star:
                return { makeUniqueRef<AST::PointerType>(Lexer::Token(typeSuffixAbbreviated.token), addressSpace, WTFMove(previous)) };
            case Lexer::Token::Type::SquareBracketPair:
                return { makeUniqueRef<AST::ArrayReferenceType>(Lexer::Token(typeSuffixAbbreviated.token), addressSpace, WTFMove(previous)) };
            default:
                ASSERT(typeSuffixAbbreviated.token.type == Lexer::Token::Type::LeftSquareBracket);
                return { makeUniqueRef<AST::ArrayType>(Lexer::Token(typeSuffixAbbreviated.token), WTFMove(previous), *typeSuffixAbbreviated.numElements) };
            }
        };
        PARSE(firstTypeSuffixAbbreviated, TypeSuffixAbbreviated);
        UniqueRef<AST::UnnamedType> result = makeUniqueRef<AST::TypeReference>(WTFMove(*addressSpaceToken), name->stringView.toString(), WTFMove(*typeArguments));
        auto next = constructTypeFromSuffixAbbreviated(*firstTypeSuffixAbbreviated, WTFMove(result));
        result = WTFMove(next);
        while (true) {
            PEEK(nextToken);
            if (nextToken->type != Lexer::Token::Type::Star
                && nextToken->type != Lexer::Token::Type::SquareBracketPair
                && nextToken->type != Lexer::Token::Type::LeftSquareBracket) {
                break;
            }
            PARSE(typeSuffixAbbreviated, TypeSuffixAbbreviated);
            // FIXME: The nesting here might be in the wrong order.
            next = constructTypeFromSuffixAbbreviated(*typeSuffixAbbreviated, WTFMove(result));
            result = WTFMove(next);
        }
        return WTFMove(result);
    }

    auto constructTypeFromSuffixNonAbbreviated = [&](const TypeSuffixNonAbbreviated& typeSuffixNonAbbreviated, UniqueRef<AST::UnnamedType>&& previous) -> UniqueRef<AST::UnnamedType> {
        switch (typeSuffixNonAbbreviated.token.type) {
        case Lexer::Token::Type::Star:
            return { makeUniqueRef<AST::PointerType>(Lexer::Token(typeSuffixNonAbbreviated.token), *typeSuffixNonAbbreviated.addressSpace, WTFMove(previous)) };
        case Lexer::Token::Type::SquareBracketPair:
            return { makeUniqueRef<AST::ArrayReferenceType>(Lexer::Token(typeSuffixNonAbbreviated.token), *typeSuffixNonAbbreviated.addressSpace, WTFMove(previous)) };
        default:
            ASSERT(typeSuffixNonAbbreviated.token.type == Lexer::Token::Type::LeftSquareBracket);
            return { makeUniqueRef<AST::ArrayType>(Lexer::Token(typeSuffixNonAbbreviated.token), WTFMove(previous), *typeSuffixNonAbbreviated.numElements) };
        }
    };
    UniqueRef<AST::UnnamedType> result = makeUniqueRef<AST::TypeReference>(WTFMove(*name), name->stringView.toString(), WTFMove(*typeArguments));
    while (true) {
        PEEK(nextToken);
        if (nextToken->type != Lexer::Token::Type::Star
            && nextToken->type != Lexer::Token::Type::SquareBracketPair
            && nextToken->type != Lexer::Token::Type::LeftSquareBracket) {
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
    return AST::TypeDefinition(WTFMove(*origin), name->stringView.toString(), WTFMove(*type));
}

auto Parser::parseBuiltInSemantic() -> Expected<AST::BuiltInSemantic, Error>
{
    auto origin = consumeTypes({
        Lexer::Token::Type::SVInstanceID,
        Lexer::Token::Type::SVVertexID,
        Lexer::Token::Type::PSize,
        Lexer::Token::Type::SVPosition,
        Lexer::Token::Type::SVIsFrontFace,
        Lexer::Token::Type::SVSampleIndex,
        Lexer::Token::Type::SVInnerCoverage,
        Lexer::Token::Type::SVTarget,
        Lexer::Token::Type::SVDepth,
        Lexer::Token::Type::SVCoverage,
        Lexer::Token::Type::SVDispatchThreadID,
        Lexer::Token::Type::SVGroupID,
        Lexer::Token::Type::SVGroupIndex,
        Lexer::Token::Type::SVGroupThreadID});
    if (!origin)
        return Unexpected<Error>(origin.error());

    switch (origin->type) {
    case Lexer::Token::Type::SVInstanceID:
        return AST::BuiltInSemantic(WTFMove(*origin), AST::BuiltInSemantic::Variable::SVInstanceID);
    case Lexer::Token::Type::SVVertexID:
        return AST::BuiltInSemantic(WTFMove(*origin), AST::BuiltInSemantic::Variable::SVVertexID);
    case Lexer::Token::Type::PSize:
        return AST::BuiltInSemantic(WTFMove(*origin), AST::BuiltInSemantic::Variable::PSize);
    case Lexer::Token::Type::SVPosition:
        return AST::BuiltInSemantic(WTFMove(*origin), AST::BuiltInSemantic::Variable::SVPosition);
    case Lexer::Token::Type::SVIsFrontFace:
        return AST::BuiltInSemantic(WTFMove(*origin), AST::BuiltInSemantic::Variable::SVIsFrontFace);
    case Lexer::Token::Type::SVSampleIndex:
        return AST::BuiltInSemantic(WTFMove(*origin), AST::BuiltInSemantic::Variable::SVSampleIndex);
    case Lexer::Token::Type::SVInnerCoverage:
        return AST::BuiltInSemantic(WTFMove(*origin), AST::BuiltInSemantic::Variable::SVInnerCoverage);
    case Lexer::Token::Type::SVTarget: {
        auto target = consumeNonNegativeIntegralLiteral(); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195807 Make this work with strings like "SV_Target0".
        if (!target)
            return Unexpected<Error>(target.error());
        return AST::BuiltInSemantic(WTFMove(*origin), AST::BuiltInSemantic::Variable::SVTarget, *target);
    }
    case Lexer::Token::Type::SVDepth:
        return AST::BuiltInSemantic(WTFMove(*origin), AST::BuiltInSemantic::Variable::SVDepth);
    case Lexer::Token::Type::SVCoverage:
        return AST::BuiltInSemantic(WTFMove(*origin), AST::BuiltInSemantic::Variable::SVCoverage);
    case Lexer::Token::Type::SVDispatchThreadID:
        return AST::BuiltInSemantic(WTFMove(*origin), AST::BuiltInSemantic::Variable::SVDispatchThreadID);
    case Lexer::Token::Type::SVGroupID:
        return AST::BuiltInSemantic(WTFMove(*origin), AST::BuiltInSemantic::Variable::SVGroupID);
    case Lexer::Token::Type::SVGroupIndex:
        return AST::BuiltInSemantic(WTFMove(*origin), AST::BuiltInSemantic::Variable::SVGroupIndex);
    default:
        ASSERT(origin->type == Lexer::Token::Type::SVGroupThreadID);
        return AST::BuiltInSemantic(WTFMove(*origin), AST::BuiltInSemantic::Variable::SVGroupThreadID);
    }
}

auto Parser::parseResourceSemantic() -> Expected<AST::ResourceSemantic, Error>
{
    CONSUME_TYPE(origin, Register);
    CONSUME_TYPE(leftParenthesis, LeftParenthesis);

    CONSUME_TYPE(info, Identifier);
    if (info->stringView.length() < 2 || (info->stringView[0] != 'u'
        && info->stringView[0] != 't'
        && info->stringView[0] != 'b'
        && info->stringView[0] != 's'))
        return Unexpected<Error>(Error(makeString(info->stringView.substring(0, 1), " is not a known resource type ('u', 't', 'b', or 's')")));

    AST::ResourceSemantic::Mode mode;
    switch (info->stringView[0]) {
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
    }

    auto index = recognizeSimpleUnsignedInteger(info->stringView.substring(1));
    if (!index)
        return Unexpected<Error>(index.error());

    unsigned space = 0;
    if (tryType(Lexer::Token::Type::Comma)) {
        CONSUME_TYPE(spaceToken, Identifier);
        auto prefix = "space"_str;
        if (!spaceToken->stringView.startsWith(StringView(prefix)))
            return Unexpected<Error>(Error(makeString("Second argument to resource semantic ", spaceToken->stringView, " needs be of the form 'space0'")));
        if (spaceToken->stringView.length() <= prefix.length())
            return Unexpected<Error>(Error(makeString("Second argument to resource semantic ", spaceToken->stringView, " needs be of the form 'space0'")));
        auto spaceValue = recognizeSimpleUnsignedInteger(spaceToken->stringView.substring(prefix.length()));
        if (!spaceValue)
            return Unexpected<Error>(spaceValue.error());
        space = *spaceValue;
    }

    CONSUME_TYPE(rightParenthesis, RightParenthesis);

    return AST::ResourceSemantic(WTFMove(*origin), mode, *index, space);
}

auto Parser::parseSpecializationConstantSemantic() -> Expected<AST::SpecializationConstantSemantic, Error>
{
    CONSUME_TYPE(origin, Specialized);
    return AST::SpecializationConstantSemantic(WTFMove(*origin));
}

auto Parser::parseStageInOutSemantic() -> Expected<AST::StageInOutSemantic, Error>
{
    CONSUME_TYPE(origin, Attribute);
    CONSUME_TYPE(leftParenthesis, LeftParenthesis);

    auto index = consumeNonNegativeIntegralLiteral();
    if (!index)
        return Unexpected<Error>(index.error());

    CONSUME_TYPE(rightParenthesis, RightParenthesis);

    return AST::StageInOutSemantic(WTFMove(*origin), *index);
}

auto Parser::parseSemantic() -> Expected<Optional<AST::Semantic>, Error>
{
    if (!tryType(Lexer::Token::Type::Colon))
        return { WTF::nullopt };

    PEEK(token);
    switch (token->type) {
    case Lexer::Token::Type::Attribute: {
        PARSE(result, StageInOutSemantic);
        return { AST::Semantic(WTFMove(*result)) };
    }
    case Lexer::Token::Type::Specialized:  {
        PARSE(result, SpecializationConstantSemantic);
        return { AST::Semantic(WTFMove(*result)) };
    }
    case Lexer::Token::Type::Register:  {
        PARSE(result, ResourceSemantic);
        return { AST::Semantic(WTFMove(*result)) };
    }
    default:  {
        PARSE(result, BuiltInSemantic);
        return { AST::Semantic(WTFMove(*result)) };
    }
    }
}
AST::Qualifiers Parser::parseQualifiers()
{
    AST::Qualifiers qualifiers;
    while (auto next = tryType(Lexer::Token::Type::Qualifier)) {
        if ("nointerpolation" == next->stringView)
            qualifiers.append(AST::Qualifier::Nointerpolation);
        else if ("noperspective" == next->stringView)
            qualifiers.append(AST::Qualifier::Noperspective);
        else if ("uniform" == next->stringView)
            qualifiers.append(AST::Qualifier::Uniform);
        else if ("centroid" == next->stringView)
            qualifiers.append(AST::Qualifier::Centroid);
        else {
            ASSERT("sample" == next->stringView);
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

    return AST::StructureElement(WTFMove(*origin), WTFMove(qualifiers), WTFMove(*type), name->stringView.toString(), WTFMove(*semantic));
}

auto Parser::parseStructureDefinition() -> Expected<AST::StructureDefinition, Error>
{
    CONSUME_TYPE(origin, Struct);
    CONSUME_TYPE(name, Identifier);
    CONSUME_TYPE(leftCurlyBracket, LeftCurlyBracket);

    AST::StructureElements structureElements;
    while (!tryType(Lexer::Token::Type::RightCurlyBracket)) {
        PARSE(structureElement, StructureElement);
        structureElements.append(WTFMove(*structureElement));
    }

    return AST::StructureDefinition(WTFMove(*origin), name->stringView.toString(), WTFMove(structureElements));
}

auto Parser::parseEnumerationDefinition() -> Expected<AST::EnumerationDefinition, Error>
{
    CONSUME_TYPE(origin, Enum);
    CONSUME_TYPE(name, Identifier);

    auto type = ([&]() -> Expected<UniqueRef<AST::UnnamedType>, Error> {
        if (tryType(Lexer::Token::Type::Colon)) {
            PARSE(parsedType, Type);
            return WTFMove(*parsedType);
        }
        return { makeUniqueRef<AST::TypeReference>(Lexer::Token(*origin), "int"_str, AST::TypeArguments()) };
    })();
    if (!type)
        return Unexpected<Error>(type.error());

    CONSUME_TYPE(leftCurlyBracket, LeftCurlyBracket);

    PARSE(firstEnumerationMember, EnumerationMember);

    AST::EnumerationDefinition result(WTFMove(*origin), name->stringView.toString(), WTFMove(*type));
    auto success = result.add(WTFMove(*firstEnumerationMember));
    if (!success)
        return fail("Cannot add enumeration member"_str);

    while (tryType(Lexer::Token::Type::Comma)) {
        PARSE(member, EnumerationMember);
        success = result.add(WTFMove(*member));
        if (!success)
            return fail("Cannot add enumeration member"_str);
    }

    CONSUME_TYPE(rightCurlyBracket, RightCurlyBracket);

    return WTFMove(result);
}

auto Parser::parseEnumerationMember() -> Expected<AST::EnumerationMember, Error>
{
    CONSUME_TYPE(identifier, Identifier);
    auto name = identifier->stringView.toString();

    if (tryType(Lexer::Token::Type::EqualsSign)) {
        PARSE(constantExpression, ConstantExpression);
        return AST::EnumerationMember(Lexer::Token(*identifier), WTFMove(name), WTFMove(*constantExpression));
    }
    return AST::EnumerationMember(Lexer::Token(*identifier), WTFMove(name));
}

auto Parser::parseNativeTypeDeclaration() -> Expected<AST::NativeTypeDeclaration, Error>
{
    CONSUME_TYPE(origin, Native);
    CONSUME_TYPE(parsedTypedef, Typedef);
    CONSUME_TYPE(name, Identifier);
    PARSE(typeArguments, TypeArguments);
    CONSUME_TYPE(semicolon, Semicolon);

    return AST::NativeTypeDeclaration(WTFMove(*origin), name->stringView.toString(), WTFMove(*typeArguments));
}

auto Parser::parseNumThreadsFunctionAttribute() -> Expected<AST::NumThreadsFunctionAttribute, Error>
{
    CONSUME_TYPE(origin, NumThreads);
    CONSUME_TYPE(leftParenthesis, LeftParenthesis);

    auto width = consumeNonNegativeIntegralLiteral();
    if (!width)
        return Unexpected<Error>(width.error());

    CONSUME_TYPE(comma, Comma);

    auto height = consumeNonNegativeIntegralLiteral();
    if (!height)
        return Unexpected<Error>(height.error());

    CONSUME_TYPE(secondComma, Comma);

    auto depth = consumeNonNegativeIntegralLiteral();
    if (!depth)
        return Unexpected<Error>(depth.error());

    CONSUME_TYPE(rightParenthesis, RightParenthesis);

    return AST::NumThreadsFunctionAttribute(WTFMove(*origin), *width, *height, *depth);
}

auto Parser::parseAttributeBlock() -> Expected<AST::AttributeBlock, Error>
{
    CONSUME_TYPE(leftSquareBracket, LeftSquareBracket);

    AST::AttributeBlock result;

    while (true) {
        auto numThreadsFunctionAttribute = backtrackingScope<Expected<AST::NumThreadsFunctionAttribute, Error>>([&]() {
            return parseNumThreadsFunctionAttribute();
        });
        if (numThreadsFunctionAttribute) {
            result.append(WTFMove(*numThreadsFunctionAttribute));
            continue;
        }

        break;
    }

    CONSUME_TYPE(rightSquareBracket, RightSquareBracket);

    return WTFMove(result);
}

auto Parser::parseParameter() -> Expected<AST::VariableDeclaration, Error>
{
    PEEK(origin);

    AST::Qualifiers qualifiers = parseQualifiers();

    PARSE(type, Type);

    String name;
    if (auto token = tryType(Lexer::Token::Type::Identifier))
        name = token->stringView.toString();

    PARSE(semantic, Semantic);

    return AST::VariableDeclaration(WTFMove(*origin), WTFMove(qualifiers), { WTFMove(*type) }, WTFMove(name), WTFMove(*semantic), WTF::nullopt);
}

auto Parser::parseParameters() -> Expected<AST::VariableDeclarations, Error>
{
    AST::VariableDeclarations parameters;

    CONSUME_TYPE(leftParenthesis, LeftParenthesis);

    if (tryType(Lexer::Token::Type::RightParenthesis))
        return WTFMove(parameters);

    PARSE(firstParameter, Parameter);
    parameters.append(makeUniqueRef<AST::VariableDeclaration>(WTFMove(*firstParameter)));

    while (tryType(Lexer::Token::Type::Comma)) {
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
    bool isOperator = false;
    return AST::FunctionDeclaration(WTFMove(*origin), WTFMove(*attributeBlock), AST::EntryPointType::Compute, WTFMove(*type), name->stringView.toString(), WTFMove(*parameters), WTFMove(*semantic), isOperator);
}

auto Parser::parseVertexOrFragmentFunctionDeclaration() -> Expected<AST::FunctionDeclaration, Error>
{
    auto entryPoint = consumeTypes({ Lexer::Token::Type::Vertex, Lexer::Token::Type::Fragment });
    if (!entryPoint)
        return Unexpected<Error>(entryPoint.error());
    auto entryPointType = (entryPoint->type == Lexer::Token::Type::Vertex) ? AST::EntryPointType::Vertex : AST::EntryPointType::Fragment;

    PARSE(type, Type);
    CONSUME_TYPE(name, Identifier);
    PARSE(parameters, Parameters);
    PARSE(semantic, Semantic);

    bool isOperator = false;
    return AST::FunctionDeclaration(WTFMove(*entryPoint), { }, entryPointType, WTFMove(*type), name->stringView.toString(), WTFMove(*parameters), WTFMove(*semantic), isOperator);
}

auto Parser::parseRegularFunctionDeclaration() -> Expected<AST::FunctionDeclaration, Error>
{
    PEEK(origin);

    PARSE(type, Type);

    auto name = consumeTypes({ Lexer::Token::Type::Identifier, Lexer::Token::Type::OperatorName });
    if (!name)
        return Unexpected<Error>(name.error());
    auto isOperator = name->type == Lexer::Token::Type::OperatorName;

    PARSE(parameters, Parameters);
    PARSE(semantic, Semantic);

    return AST::FunctionDeclaration(WTFMove(*origin), { }, WTF::nullopt, WTFMove(*type), name->stringView.toString(), WTFMove(*parameters), WTFMove(*semantic), isOperator);
}

auto Parser::parseOperatorFunctionDeclaration() -> Expected<AST::FunctionDeclaration, Error>
{
    CONSUME_TYPE(origin, Operator);
    PARSE(type, Type);
    PARSE(parameters, Parameters);
    PARSE(semantic, Semantic);

    bool isOperator = true;
    return AST::FunctionDeclaration(WTFMove(*origin), { }, WTF::nullopt, WTFMove(*type), "operator cast"_str, WTFMove(*parameters), WTFMove(*semantic), isOperator);
}

auto Parser::parseFunctionDeclaration() -> Expected<AST::FunctionDeclaration, Error>
{
    PEEK(token);
    switch (token->type) {
    case Lexer::Token::Type::Operator:
        return parseOperatorFunctionDeclaration();
    case Lexer::Token::Type::Vertex:
    case Lexer::Token::Type::Fragment:
        return parseVertexOrFragmentFunctionDeclaration();
    case Lexer::Token::Type::LeftSquareBracket:
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
    PARSE(result, BlockBody, WTFMove(*origin));
    CONSUME_TYPE(rightCurlyBracket, RightCurlyBracket);
    return WTFMove(*result);
}

auto Parser::parseBlockBody(Lexer::Token&& origin) -> Expected<AST::Block, Error>
{
    AST::Statements statements;
    while (!peekTypes({Lexer::Token::Type::RightCurlyBracket, Lexer::Token::Type::Case, Lexer::Token::Type::Default})) {
        PARSE(statement, Statement);
        statements.append(WTFMove(*statement));
    }
    return AST::Block(WTFMove(origin), WTFMove(statements));
}

auto Parser::parseIfStatement() -> Expected<AST::IfStatement, Error>
{
    CONSUME_TYPE(origin, If);
    CONSUME_TYPE(leftParenthesis, LeftParenthesis);
    PARSE(conditional, Expression);
    CONSUME_TYPE(rightParenthesis, RightParenthesis);
    PARSE(body, Statement);

    Optional<UniqueRef<AST::Statement>> elseBody;
    if (tryType(Lexer::Token::Type::Else)) {
        PARSE(parsedElseBody, Statement);
        elseBody = WTFMove(*parsedElseBody);
    }

    Vector<UniqueRef<AST::Expression>> castArguments;
    castArguments.append(WTFMove(*conditional));
    auto boolCast = makeUniqueRef<AST::CallExpression>(Lexer::Token(*origin), "bool"_str, WTFMove(castArguments));
    return AST::IfStatement(WTFMove(*origin), WTFMove(boolCast), WTFMove(*body), WTFMove(elseBody));
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
    while (nextToken->type != Lexer::Token::Type::RightCurlyBracket) {
        PARSE(switchCase, SwitchCase);
        switchCases.append(WTFMove(*switchCase));
    }

    m_lexer.consumeToken();

    return AST::SwitchStatement(WTFMove(*origin), WTFMove(*value), WTFMove(switchCases));
}

auto Parser::parseSwitchCase() -> Expected<AST::SwitchCase, Error>
{
    auto origin = consumeTypes({ Lexer::Token::Type::Case, Lexer::Token::Type::Default });
    if (!origin)
        return Unexpected<Error>(origin.error());

    switch (origin->type) {
    case Lexer::Token::Type::Case: {
        PARSE(value, ConstantExpression);
        CONSUME_TYPE(colon, Colon);

        PARSE(block, BlockBody, Lexer::Token(*origin));

        return AST::SwitchCase(WTFMove(*origin), WTFMove(*value), WTFMove(*block));
    }
    default: {
        ASSERT(origin->type == Lexer::Token::Type::Default);
        CONSUME_TYPE(colon, Colon);

        PARSE(block, BlockBody, Lexer::Token(*origin));

        return AST::SwitchCase(WTFMove(*origin), WTF::nullopt, WTFMove(*block));
    }
    }
}

auto Parser::parseForLoop() -> Expected<AST::ForLoop, Error>
{
    CONSUME_TYPE(origin, For);
    CONSUME_TYPE(leftParenthesis, LeftParenthesis);

    auto parseRemainder = [&](Variant<AST::VariableDeclarationsStatement, UniqueRef<AST::Expression>>&& initialization) -> Expected<AST::ForLoop, Error> {
        CONSUME_TYPE(semicolon, Semicolon);

        Optional<UniqueRef<AST::Expression>> condition = WTF::nullopt;
        if (!tryType(Lexer::Token::Type::Semicolon)) {
            if (auto expression = parseExpression())
                condition = { WTFMove(*expression) };
            else
                return Unexpected<Error>(expression.error());
            CONSUME_TYPE(secondSemicolon, Semicolon);
        }

        Optional<UniqueRef<AST::Expression>> increment = WTF::nullopt;
        if (!tryType(Lexer::Token::Type::RightParenthesis)) {
            if (auto expression = parseExpression())
                increment = { WTFMove(*expression) };
            else
                return Unexpected<Error>(expression.error());
            CONSUME_TYPE(rightParenthesis, RightParenthesis);
        }

        PARSE(body, Statement);

        return AST::ForLoop(WTFMove(*origin), WTFMove(initialization), WTFMove(condition), WTFMove(increment), WTFMove(*body));
    };

    auto variableDeclarations = backtrackingScope<Expected<AST::VariableDeclarationsStatement, Error>>([&]() {
        return parseVariableDeclarations();
    });
    if (variableDeclarations)
        return parseRemainder(WTFMove(*variableDeclarations));

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

    return AST::WhileLoop(WTFMove(*origin), WTFMove(*conditional), WTFMove(*body));
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

    return AST::DoWhileLoop(WTFMove(*origin), WTFMove(*body), WTFMove(*conditional));
}

auto Parser::parseVariableDeclaration(UniqueRef<AST::UnnamedType>&& type) -> Expected<AST::VariableDeclaration, Error>
{
    PEEK(origin);

    auto qualifiers = parseQualifiers();

    CONSUME_TYPE(name, Identifier);
    PARSE(semantic, Semantic);

    if (tryType(Lexer::Token::Type::EqualsSign)) {
        PARSE(initializer, PossibleTernaryConditional);
        return AST::VariableDeclaration(WTFMove(*origin), WTFMove(qualifiers), { WTFMove(type) }, name->stringView.toString(), WTFMove(*semantic), WTFMove(*initializer));
    }

    return AST::VariableDeclaration(WTFMove(*origin), WTFMove(qualifiers), { WTFMove(type) }, name->stringView.toString(), WTFMove(*semantic), WTF::nullopt);
}

auto Parser::parseVariableDeclarations() -> Expected<AST::VariableDeclarationsStatement, Error>
{
    PEEK(origin);

    PARSE(type, Type);

    auto firstVariableDeclaration = parseVariableDeclaration((*type)->clone());
    if (!firstVariableDeclaration)
        return Unexpected<Error>(firstVariableDeclaration.error());

    Vector<UniqueRef<AST::VariableDeclaration>> result;
    result.append(makeUniqueRef<AST::VariableDeclaration>(WTFMove(*firstVariableDeclaration)));

    while (tryType(Lexer::Token::Type::Comma)) {
        auto variableDeclaration = parseVariableDeclaration((*type)->clone());
        if (!variableDeclaration)
            return Unexpected<Error>(variableDeclaration.error());
        result.append(makeUniqueRef<AST::VariableDeclaration>(WTFMove(*variableDeclaration)));
    }

    return AST::VariableDeclarationsStatement(WTFMove(*origin), WTFMove(result));
}

auto Parser::parseStatement() -> Expected<UniqueRef<AST::Statement>, Error>
{
    PEEK(token);
    switch (token->type) {
    case Lexer::Token::Type::LeftCurlyBracket: {
        PARSE(block, Block);
        return { makeUniqueRef<AST::Block>(WTFMove(*block)) };
    }
    case Lexer::Token::Type::If: {
        PARSE(ifStatement, IfStatement);
        return { makeUniqueRef<AST::IfStatement>(WTFMove(*ifStatement)) };
    }
    case Lexer::Token::Type::Switch: {
        PARSE(switchStatement, SwitchStatement);
        return { makeUniqueRef<AST::SwitchStatement>(WTFMove(*switchStatement)) };
    }
    case Lexer::Token::Type::For: {
        PARSE(forLoop, ForLoop);
        return { makeUniqueRef<AST::ForLoop>(WTFMove(*forLoop)) };
    }
    case Lexer::Token::Type::While: {
        PARSE(whileLoop, WhileLoop);
        return { makeUniqueRef<AST::WhileLoop>(WTFMove(*whileLoop)) };
    }
    case Lexer::Token::Type::Do: {
        PARSE(doWhileLoop, DoWhileLoop);
        return { makeUniqueRef<AST::DoWhileLoop>(WTFMove(*doWhileLoop)) };
    }
    case Lexer::Token::Type::Break: {
        auto breakToken = m_lexer.consumeToken();
        CONSUME_TYPE(semicolon, Semicolon);
        auto breakObject = AST::Break(WTFMove(*breakToken));
        return { makeUniqueRef<AST::Break>(WTFMove(breakObject)) };
    }
    case Lexer::Token::Type::Continue: {
        auto continueToken = m_lexer.consumeToken();
        CONSUME_TYPE(semicolon, Semicolon);
        auto continueObject = AST::Continue(WTFMove(*continueToken));
        return { makeUniqueRef<AST::Continue>(WTFMove(continueObject)) };
    }
    case Lexer::Token::Type::Fallthrough: {
        auto fallthroughToken = m_lexer.consumeToken();
        CONSUME_TYPE(semicolon, Semicolon);
        auto fallthroughObject = AST::Fallthrough(WTFMove(*fallthroughToken));
        return { makeUniqueRef<AST::Fallthrough>(WTFMove(fallthroughObject)) };
    }
    case Lexer::Token::Type::Trap: {
        auto trapToken = m_lexer.consumeToken();
        CONSUME_TYPE(semicolon, Semicolon);
        auto trapObject = AST::Trap(WTFMove(*trapToken));
        return { makeUniqueRef<AST::Trap>(WTFMove(trapObject)) };
    }
    case Lexer::Token::Type::Return: {
        auto returnToken = m_lexer.consumeToken();
        if (auto semicolon = tryType(Lexer::Token::Type::Semicolon)) {
            auto returnObject = AST::Return(WTFMove(*returnToken), WTF::nullopt);
            return { makeUniqueRef<AST::Return>(WTFMove(returnObject)) };
        }
        PARSE(expression, Expression);
        CONSUME_TYPE(finalSemicolon, Semicolon);
        auto returnObject = AST::Return(WTFMove(*returnToken), { WTFMove(*expression) });
        return { makeUniqueRef<AST::Return>(WTFMove(returnObject)) };
    }
    case Lexer::Token::Type::Constant:
    case Lexer::Token::Type::Device:
    case Lexer::Token::Type::Threadgroup:
    case Lexer::Token::Type::Thread: {
        PARSE(variableDeclarations, VariableDeclarations);
        CONSUME_TYPE(semicolon, Semicolon);
        return { makeUniqueRef<AST::VariableDeclarationsStatement>(WTFMove(*variableDeclarations)) };
    }
    case Lexer::Token::Type::Identifier: {
        PEEK_FURTHER(nextToken);
        switch (nextToken->type) {
        case Lexer::Token::Type::Identifier:
        case Lexer::Token::Type::LessThanSign:
        case Lexer::Token::Type::Star:
        case Lexer::Token::Type::Qualifier: {
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
        auto effectfulExpression = backtrackingScope<Expected<UniqueRef<AST::Expression>, Error>>([&]() -> Expected<UniqueRef<AST::Expression>, Error> {
            PARSE(result, EffectfulExpression);
            CONSUME_TYPE(semicolon, Semicolon);
            return result;
        });
        if (effectfulExpression)
            return { makeUniqueRef<AST::EffectfulExpressionStatement>(WTFMove(*effectfulExpression)) };
    }

    PARSE(variableDeclarations, VariableDeclarations);
    CONSUME_TYPE(semicolon, Semicolon);
    return { makeUniqueRef<AST::VariableDeclarationsStatement>(WTFMove(*variableDeclarations)) };
}

auto Parser::parseEffectfulExpression() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PEEK(origin);

    Vector<UniqueRef<AST::Expression>> expressions;
    if (origin->type == Lexer::Token::Type::Semicolon)
        return { makeUniqueRef<AST::CommaExpression>(WTFMove(*origin), WTFMove(expressions)) };

    PARSE(effectfulExpression, EffectfulAssignment);
    expressions.append(WTFMove(*effectfulExpression));

    while (tryType(Lexer::Token::Type::Comma)) {
        PARSE(expression, EffectfulAssignment);
        expressions.append(WTFMove(*expression));
    }

    if (expressions.size() == 1)
        return WTFMove(expressions[0]);
    return { makeUniqueRef<AST::CommaExpression>(WTFMove(*origin), WTFMove(expressions)) };
}

auto Parser::parseEffectfulAssignment() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PEEK(origin);

    bool isEffectful = false;
    PARSE(expression, PossiblePrefix, &isEffectful);

    if (!isEffectful || peekTypes({
        Lexer::Token::Type::EqualsSign,
        Lexer::Token::Type::PlusEquals,
        Lexer::Token::Type::MinusEquals,
        Lexer::Token::Type::TimesEquals,
        Lexer::Token::Type::DivideEquals,
        Lexer::Token::Type::ModEquals,
        Lexer::Token::Type::XorEquals,
        Lexer::Token::Type::AndEquals,
        Lexer::Token::Type::OrEquals,
        Lexer::Token::Type::RightShiftEquals,
        Lexer::Token::Type::LeftShiftEquals
    })) {
        return completeAssignment(WTFMove(*origin), WTFMove(*expression));
    }

    return expression;
}

auto Parser::parseLimitedSuffixOperator(UniqueRef<AST::Expression>&& previous) -> SuffixExpression
{
    auto type = consumeTypes({ Lexer::Token::Type::FullStop, Lexer::Token::Type::Arrow, Lexer::Token::Type::LeftSquareBracket });
    if (!type)
        return SuffixExpression(WTFMove(previous), false);

    switch (type->type) {
    case Lexer::Token::Type::FullStop: {
        auto identifier = consumeType(Lexer::Token::Type::Identifier);
        if (!identifier)
            return SuffixExpression(WTFMove(previous), false);
        return SuffixExpression(makeUniqueRef<AST::DotExpression>(WTFMove(*type), WTFMove(previous), identifier->stringView.toString()), true);
    }
    case Lexer::Token::Type::Arrow: {
        auto identifier = consumeType(Lexer::Token::Type::Identifier);
        if (!identifier)
            return SuffixExpression(WTFMove(previous), false);
        return SuffixExpression(makeUniqueRef<AST::DotExpression>(Lexer::Token(*type), makeUniqueRef<AST::DereferenceExpression>(WTFMove(*type), WTFMove(previous)), identifier->stringView.toString()), true);
    }
    default: {
        ASSERT(type->type == Lexer::Token::Type::LeftSquareBracket);
        auto expression = parseExpression();
        if (!expression)
            return SuffixExpression(WTFMove(previous), false);
        if (!consumeType(Lexer::Token::Type::RightSquareBracket))
            return SuffixExpression(WTFMove(previous), false);
        return SuffixExpression(makeUniqueRef<AST::IndexExpression>(WTFMove(*type), WTFMove(previous), WTFMove(*expression)), true);
    }
    }
}

auto Parser::parseSuffixOperator(UniqueRef<AST::Expression>&& previous) -> SuffixExpression
{
    auto suffix = consumeTypes({ Lexer::Token::Type::FullStop, Lexer::Token::Type::Arrow, Lexer::Token::Type::LeftSquareBracket, Lexer::Token::Type::PlusPlus, Lexer::Token::Type::MinusMinus });
    if (!suffix)
        return SuffixExpression(WTFMove(previous), false);

    switch (suffix->type) {
    case Lexer::Token::Type::FullStop: {
        auto identifier = consumeType(Lexer::Token::Type::Identifier);
        if (!identifier)
            return SuffixExpression(WTFMove(previous), false);
        return SuffixExpression(makeUniqueRef<AST::DotExpression>(WTFMove(*suffix), WTFMove(previous), identifier->stringView.toString()), true);
    }
    case Lexer::Token::Type::Arrow: {
        auto identifier = consumeType(Lexer::Token::Type::Identifier);
        if (!identifier)
            return SuffixExpression(WTFMove(previous), false);
        return SuffixExpression(makeUniqueRef<AST::DotExpression>(Lexer::Token(*suffix), makeUniqueRef<AST::DereferenceExpression>(WTFMove(*suffix), WTFMove(previous)), identifier->stringView.toString()), true);
    }
    case Lexer::Token::Type::LeftSquareBracket: {
        auto expression = parseExpression();
        if (!expression)
            return SuffixExpression(WTFMove(previous), false);
        if (!consumeType(Lexer::Token::Type::RightSquareBracket))
            return SuffixExpression(WTFMove(previous), false);
        return SuffixExpression(makeUniqueRef<AST::IndexExpression>(WTFMove(*suffix), WTFMove(previous), WTFMove(*expression)), true);
    }
    case Lexer::Token::Type::PlusPlus: {
        auto result = AST::ReadModifyWriteExpression::create(Lexer::Token(*suffix), WTFMove(previous));
        Vector<UniqueRef<AST::Expression>> callArguments;
        callArguments.append(result->oldVariableReference());
        result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(WTFMove(*suffix), "operator++"_str, WTFMove(callArguments)));
        result->setResultExpression(result->oldVariableReference());
        return SuffixExpression(WTFMove(result), true);
    }
    default: {
        ASSERT(suffix->type == Lexer::Token::Type::MinusMinus);
        auto result = AST::ReadModifyWriteExpression::create(Lexer::Token(*suffix), WTFMove(previous));
        Vector<UniqueRef<AST::Expression>> callArguments;
        callArguments.append(result->oldVariableReference());
        result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(WTFMove(*suffix), "operator--"_str, WTFMove(callArguments)));
        result->setResultExpression(result->oldVariableReference());
        return SuffixExpression(WTFMove(result), true);
    }
    }
}

auto Parser::parseExpression() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PEEK(origin);

    PARSE(first, PossibleTernaryConditional);
    Vector<UniqueRef<AST::Expression>> expressions;
    expressions.append(WTFMove(*first));

    while (tryType(Lexer::Token::Type::Comma)) {
        PARSE(expression, PossibleTernaryConditional);
        expressions.append(WTFMove(*expression));
    }

    if (expressions.size() == 1)
        return WTFMove(expressions[0]);
    return { makeUniqueRef<AST::CommaExpression>(WTFMove(*origin), WTFMove(expressions)) };
}

auto Parser::parseTernaryConditional() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PEEK(origin);

    PARSE(predicate, PossibleLogicalBinaryOperation);

    return completeTernaryConditional(WTFMove(*origin), WTFMove(*predicate));
}

auto Parser::completeTernaryConditional(Lexer::Token&& origin, UniqueRef<AST::Expression>&& predicate) -> Expected<UniqueRef<AST::Expression>, Error>
{
    CONSUME_TYPE(questionMark, QuestionMark);
    PARSE(bodyExpression, Expression);
    CONSUME_TYPE(colon, Colon);
    PARSE(elseExpression, PossibleTernaryConditional);

    Vector<UniqueRef<AST::Expression>> castArguments;
    castArguments.append(WTFMove(predicate));
    auto boolCast = makeUniqueRef<AST::CallExpression>(Lexer::Token(origin), "bool"_str, WTFMove(castArguments));
    return { makeUniqueRef<AST::TernaryExpression>(WTFMove(origin), WTFMove(boolCast), WTFMove(*bodyExpression), WTFMove(*elseExpression)) };
}

auto Parser::parseAssignment() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PEEK(origin);

    PARSE(left, PossiblePrefix);

    return completeAssignment(WTFMove(*origin), WTFMove(*left));
}

auto Parser::completeAssignment(Lexer::Token&& origin, UniqueRef<AST::Expression>&& left) -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto assignmentOperator = consumeTypes({
        Lexer::Token::Type::EqualsSign,
        Lexer::Token::Type::PlusEquals,
        Lexer::Token::Type::MinusEquals,
        Lexer::Token::Type::TimesEquals,
        Lexer::Token::Type::DivideEquals,
        Lexer::Token::Type::ModEquals,
        Lexer::Token::Type::XorEquals,
        Lexer::Token::Type::AndEquals,
        Lexer::Token::Type::OrEquals,
        Lexer::Token::Type::RightShiftEquals,
        Lexer::Token::Type::LeftShiftEquals
    });
    if (!assignmentOperator)
        return Unexpected<Error>(assignmentOperator.error());

    PARSE(right, PossibleTernaryConditional);

    if (assignmentOperator->type == Lexer::Token::Type::EqualsSign)
        return { makeUniqueRef<AST::AssignmentExpression>(WTFMove(origin), WTFMove(left), WTFMove(*right))};

    String name;
    switch (assignmentOperator->type) {
    case Lexer::Token::Type::PlusEquals:
        name = "operator+"_str;
        break;
    case Lexer::Token::Type::MinusEquals:
        name = "operator-"_str;
        break;
    case Lexer::Token::Type::TimesEquals:
        name = "operator*"_str;
        break;
    case Lexer::Token::Type::DivideEquals:
        name = "operator/"_str;
        break;
    case Lexer::Token::Type::ModEquals:
        name = "operator%"_str;
        break;
    case Lexer::Token::Type::XorEquals:
        name = "operator^"_str;
        break;
    case Lexer::Token::Type::AndEquals:
        name = "operator&"_str;
        break;
    case Lexer::Token::Type::OrEquals:
        name = "operator|"_str;
        break;
    case Lexer::Token::Type::RightShiftEquals:
        name = "operator>>"_str;
        break;
    default:
        ASSERT(assignmentOperator->type == Lexer::Token::Type::LeftShiftEquals);
        name = "operator<<"_str;
        break;
    }

    auto result = AST::ReadModifyWriteExpression::create(Lexer::Token(origin), WTFMove(left));
    Vector<UniqueRef<AST::Expression>> callArguments;
    callArguments.append(result->oldVariableReference());
    callArguments.append(WTFMove(*right));
    result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(WTFMove(origin), WTFMove(name), WTFMove(callArguments)));
    result->setResultExpression(result->newVariableReference());
    return { WTFMove(result) };
}

auto Parser::parsePossibleTernaryConditional() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PEEK(origin);

    PARSE(expression, PossiblePrefix);

    if (tryTypes({Lexer::Token::Type::EqualsSign,
        Lexer::Token::Type::PlusEquals,
        Lexer::Token::Type::MinusEquals,
        Lexer::Token::Type::TimesEquals,
        Lexer::Token::Type::DivideEquals,
        Lexer::Token::Type::ModEquals,
        Lexer::Token::Type::XorEquals,
        Lexer::Token::Type::AndEquals,
        Lexer::Token::Type::OrEquals,
        Lexer::Token::Type::RightShiftEquals,
        Lexer::Token::Type::LeftShiftEquals})) {
        return completeAssignment(WTFMove(*origin), WTFMove(*expression));
    }

    expression = completePossibleShift(WTFMove(*expression));
    expression = completePossibleMultiply(WTFMove(*expression));
    expression = completePossibleAdd(WTFMove(*expression));
    expression = completePossibleRelationalBinaryOperation(WTFMove(*expression));
    expression = completePossibleLogicalBinaryOperation(WTFMove(*expression));

    PEEK(nextToken);
    if (nextToken->type == Lexer::Token::Type::QuestionMark)
        return completeTernaryConditional(WTFMove(*origin), WTFMove(*expression));
    return expression;
}

auto Parser::parsePossibleLogicalBinaryOperation() -> Expected<UniqueRef<AST::Expression>, Error>
{
    PARSE(parsedPrevious, PossibleRelationalBinaryOperation);
    return completePossibleLogicalBinaryOperation(WTFMove(*parsedPrevious));
}

auto Parser::completePossibleLogicalBinaryOperation(UniqueRef<AST::Expression>&& previous) -> Expected<UniqueRef<AST::Expression>, Error>
{
    while (auto logicalBinaryOperation = tryTypes({
        Lexer::Token::Type::OrOr,
        Lexer::Token::Type::AndAnd,
        Lexer::Token::Type::Or,
        Lexer::Token::Type::Xor,
        Lexer::Token::Type::And
        })) {
        PARSE(next, PossibleRelationalBinaryOperation);

        switch (logicalBinaryOperation->type) {
        case Lexer::Token::Type::OrOr:
            previous = makeUniqueRef<AST::LogicalExpression>(WTFMove(*logicalBinaryOperation), AST::LogicalExpression::Type::Or, WTFMove(previous), WTFMove(*next));
            break;
        case Lexer::Token::Type::AndAnd:
            previous = makeUniqueRef<AST::LogicalExpression>(WTFMove(*logicalBinaryOperation), AST::LogicalExpression::Type::And, WTFMove(previous), WTFMove(*next));
            break;
        case Lexer::Token::Type::Or: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*logicalBinaryOperation), "operator|"_str, WTFMove(callArguments));
            break;
        }
        case Lexer::Token::Type::Xor: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*logicalBinaryOperation), "operator^"_str, WTFMove(callArguments));
            break;
        }
        default: {
            ASSERT(logicalBinaryOperation->type == Lexer::Token::Type::And);
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*logicalBinaryOperation), "operator&"_str, WTFMove(callArguments));
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
    while (auto relationalBinaryOperation = tryTypes({
        Lexer::Token::Type::LessThanSign,
        Lexer::Token::Type::GreaterThanSign,
        Lexer::Token::Type::LessThanOrEqualTo,
        Lexer::Token::Type::GreaterThanOrEqualTo,
        Lexer::Token::Type::EqualComparison,
        Lexer::Token::Type::NotEqual
        })) {
        PARSE(next, PossibleShift);

        Vector<UniqueRef<AST::Expression>> callArguments;
        callArguments.append(WTFMove(previous));
        callArguments.append(WTFMove(*next));

        switch (relationalBinaryOperation->type) {
        case Lexer::Token::Type::LessThanSign: {
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*relationalBinaryOperation), "operator<"_str, WTFMove(callArguments));
            break;
        }
        case Lexer::Token::Type::GreaterThanSign: {
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*relationalBinaryOperation), "operator>"_str, WTFMove(callArguments));
            break;
        }
        case Lexer::Token::Type::LessThanOrEqualTo: {
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*relationalBinaryOperation), "operator<="_str, WTFMove(callArguments));
            break;
        }
        case Lexer::Token::Type::GreaterThanOrEqualTo: {
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*relationalBinaryOperation), "operator>="_str, WTFMove(callArguments));
            break;
        }
        case Lexer::Token::Type::EqualComparison: {
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*relationalBinaryOperation), "operator=="_str, WTFMove(callArguments));
            break;
        }
        default: {
            ASSERT(relationalBinaryOperation->type == Lexer::Token::Type::NotEqual);
            previous = makeUniqueRef<AST::CallExpression>(Lexer::Token(*relationalBinaryOperation), "operator=="_str, WTFMove(callArguments));
            previous = makeUniqueRef<AST::LogicalNotExpression>(WTFMove(*relationalBinaryOperation), WTFMove(previous));
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
    while (auto shift = tryTypes({
        Lexer::Token::Type::LeftShift,
        Lexer::Token::Type::RightShift
        })) {
        PARSE(next, PossibleAdd);

        Vector<UniqueRef<AST::Expression>> callArguments;
        callArguments.append(WTFMove(previous));
        callArguments.append(WTFMove(*next));

        switch (shift->type) {
        case Lexer::Token::Type::LeftShift: {
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*shift), "operator<<"_str, WTFMove(callArguments));
            break;
        }
        default: {
            ASSERT(shift->type == Lexer::Token::Type::RightShift);
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*shift), "operator>>"_str, WTFMove(callArguments));
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
    while (auto add = tryTypes({
        Lexer::Token::Type::Plus,
        Lexer::Token::Type::Minus
        })) {
        PARSE(next, PossibleMultiply);

        Vector<UniqueRef<AST::Expression>> callArguments;
        callArguments.append(WTFMove(previous));
        callArguments.append(WTFMove(*next));

        switch (add->type) {
        case Lexer::Token::Type::Plus: {
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*add), "operator+"_str, WTFMove(callArguments));
            break;
        }
        default: {
            ASSERT(add->type == Lexer::Token::Type::Minus);
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*add), "operator-"_str, WTFMove(callArguments));
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
    while (auto multiply = tryTypes({
        Lexer::Token::Type::Star,
        Lexer::Token::Type::Divide,
        Lexer::Token::Type::Mod
        })) {
        PARSE(next, PossiblePrefix);

        Vector<UniqueRef<AST::Expression>> callArguments;
        callArguments.append(WTFMove(previous));
        callArguments.append(WTFMove(*next));

        switch (multiply->type) {
        case Lexer::Token::Type::Star: {
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*multiply), "operator*"_str, WTFMove(callArguments));
            break;
        }
        case Lexer::Token::Type::Divide: {
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*multiply), "operator/"_str, WTFMove(callArguments));
            break;
        }
        default: {
            ASSERT(multiply->type == Lexer::Token::Type::Mod);
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*multiply), "operator%"_str, WTFMove(callArguments));
            break;
        }
        }
    }

    return WTFMove(previous);
}

auto Parser::parsePossiblePrefix(bool *isEffectful) -> Expected<UniqueRef<AST::Expression>, Error>
{
    if (auto prefix = tryTypes({
        Lexer::Token::Type::PlusPlus,
        Lexer::Token::Type::MinusMinus,
        Lexer::Token::Type::Plus,
        Lexer::Token::Type::Minus,
        Lexer::Token::Type::Tilde,
        Lexer::Token::Type::ExclamationPoint,
        Lexer::Token::Type::And,
        Lexer::Token::Type::At,
        Lexer::Token::Type::Star
    })) {
        PARSE(next, PossiblePrefix);

        switch (prefix->type) {
        case Lexer::Token::Type::PlusPlus: {
            if (isEffectful)
                *isEffectful = true;
            auto result = AST::ReadModifyWriteExpression::create(Lexer::Token(*prefix), WTFMove(*next));
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(result->oldVariableReference());
            result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(Lexer::Token(*prefix), "operator++"_str, WTFMove(callArguments)));
            result->setResultExpression(result->newVariableReference());
            return { WTFMove(result) };
        }
        case Lexer::Token::Type::MinusMinus: {
            if (isEffectful)
                *isEffectful = true;
            auto result = AST::ReadModifyWriteExpression::create(Lexer::Token(*prefix), WTFMove(*next));
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(result->oldVariableReference());
            result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(Lexer::Token(*prefix), "operator--"_str, WTFMove(callArguments)));
            result->setResultExpression(result->newVariableReference());
            return { WTFMove(result) };
        }
        case Lexer::Token::Type::Plus: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(*next));
            return { makeUniqueRef<AST::CallExpression>(Lexer::Token(*prefix), "operator+"_str, WTFMove(callArguments)) };
        }
        case Lexer::Token::Type::Minus: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(*next));
            return { makeUniqueRef<AST::CallExpression>(Lexer::Token(*prefix), "operator-"_str, WTFMove(callArguments)) };
        }
        case Lexer::Token::Type::Tilde: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(*next));
            return { makeUniqueRef<AST::CallExpression>(Lexer::Token(*prefix), "operator~"_str, WTFMove(callArguments)) };
        }
        case Lexer::Token::Type::ExclamationPoint: {
            Vector<UniqueRef<AST::Expression>> castArguments;
            castArguments.append(WTFMove(*next));
            auto boolCast = makeUniqueRef<AST::CallExpression>(Lexer::Token(*prefix), "bool"_str, WTFMove(castArguments));
            return { makeUniqueRef<AST::LogicalNotExpression>(Lexer::Token(*prefix), WTFMove(boolCast)) };
        }
        case Lexer::Token::Type::And:
            return { makeUniqueRef<AST::MakePointerExpression>(Lexer::Token(*prefix), WTFMove(*next)) };
        case Lexer::Token::Type::At:
            return { makeUniqueRef<AST::MakeArrayReferenceExpression>(Lexer::Token(*prefix), WTFMove(*next)) };
        default:
            ASSERT(prefix->type == Lexer::Token::Type::Star);
            return { makeUniqueRef<AST::DereferenceExpression>(Lexer::Token(*prefix), WTFMove(*next)) };
        }
    }

    return parsePossibleSuffix(isEffectful);
}

auto Parser::parsePossibleSuffix(bool *isEffectful) -> Expected<UniqueRef<AST::Expression>, Error>
{
    PEEK(token);
    PEEK_FURTHER(nextToken);

    if (token->type == Lexer::Token::Type::Identifier && nextToken->type == Lexer::Token::Type::LeftParenthesis) {
        PARSE(expression, CallExpression);
        if (isEffectful)
            *isEffectful = true;
        while (true) {
            PEEK(suffixToken);
            if (suffixToken->type != Lexer::Token::Type::FullStop && suffixToken->type != Lexer::Token::Type::Arrow && suffixToken->type != Lexer::Token::Type::LeftSquareBracket)
                break;
            auto result = parseLimitedSuffixOperator(WTFMove(*expression));
            expression = WTFMove(result.result);
        }
        return expression;
    }

    if (token->type == Lexer::Token::Type::LeftParenthesis && isEffectful)
        *isEffectful = true;

    PARSE(expression, Term);
    bool isLastSuffixTokenEffectful = false;
    while (true) {
        PEEK(suffixToken);
        if (suffixToken->type != Lexer::Token::Type::FullStop
            && suffixToken->type != Lexer::Token::Type::Arrow
            && suffixToken->type != Lexer::Token::Type::LeftSquareBracket
            && suffixToken->type != Lexer::Token::Type::PlusPlus
            && suffixToken->type != Lexer::Token::Type::MinusMinus) {
            break;
        }
        isLastSuffixTokenEffectful = suffixToken->type == Lexer::Token::Type::PlusPlus || suffixToken->type == Lexer::Token::Type::MinusMinus;
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
    auto callName = name->stringView.toString();

    CONSUME_TYPE(leftParenthesis, LeftParenthesis);

    Vector<UniqueRef<AST::Expression>> arguments;
    if (tryType(Lexer::Token::Type::RightParenthesis))
        return { makeUniqueRef<AST::CallExpression>(WTFMove(*name), WTFMove(callName), WTFMove(arguments)) };

    PARSE(firstArgument, PossibleTernaryConditional);
    arguments.append(WTFMove(*firstArgument));
    while (tryType(Lexer::Token::Type::Comma)) {
        PARSE(argument, PossibleTernaryConditional);
        arguments.append(WTFMove(*argument));
    }

    CONSUME_TYPE(rightParenthesis, RightParenthesis);

    return { makeUniqueRef<AST::CallExpression>(WTFMove(*name), WTFMove(callName), WTFMove(arguments)) };
}

auto Parser::parseTerm() -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto type = consumeTypes({
        Lexer::Token::Type::IntLiteral,
        Lexer::Token::Type::UintLiteral,
        Lexer::Token::Type::FloatLiteral,
        Lexer::Token::Type::Null,
        Lexer::Token::Type::True,
        Lexer::Token::Type::False,
        Lexer::Token::Type::Identifier,
        Lexer::Token::Type::LeftParenthesis
    });
    if (!type)
        return Unexpected<Error>(type.error());

    switch (type->type) {
    case Lexer::Token::Type::IntLiteral: {
        auto value = intLiteralToInt(type->stringView);
        if (!value)
            return Unexpected<Error>(value.error());
        return { makeUniqueRef<AST::IntegerLiteral>(WTFMove(*type), *value) };
    }
    case Lexer::Token::Type::UintLiteral: {
        auto value = uintLiteralToUint(type->stringView);
        if (!value)
            return Unexpected<Error>(value.error());
        return { makeUniqueRef<AST::UnsignedIntegerLiteral>(WTFMove(*type), *value) };
    }
    case Lexer::Token::Type::FloatLiteral: {
        auto value = floatLiteralToFloat(type->stringView);
        if (!value)
            return Unexpected<Error>(value.error());
        return { makeUniqueRef<AST::FloatLiteral>(WTFMove(*type), *value) };
    }
    case Lexer::Token::Type::Null:
        return { makeUniqueRef<AST::NullLiteral>(WTFMove(*type)) };
    case Lexer::Token::Type::True:
        return { makeUniqueRef<AST::BooleanLiteral>(WTFMove(*type), true) };
    case Lexer::Token::Type::False:
        return { makeUniqueRef<AST::BooleanLiteral>(WTFMove(*type), false) };
    case Lexer::Token::Type::Identifier: {
        auto name = type->stringView.toString();
        return { makeUniqueRef<AST::VariableReference>(WTFMove(*type), WTFMove(name)) };
    }
    default: {
        ASSERT(type->type == Lexer::Token::Type::LeftParenthesis);
        PARSE(expression, Expression);
        CONSUME_TYPE(rightParenthesis, RightParenthesis);

        return { WTFMove(*expression) };
    }
    }
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
