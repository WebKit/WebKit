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

// FIXME: Return a better error code from this, and report it to JavaScript.
auto Parser::parse(Program& program, StringView stringView, Mode mode) -> Optional<Error>
{
    m_lexer = Lexer(stringView);
    m_mode = mode;

    while (!m_lexer.isFullyConsumed()) {
        if (tryType(Lexer::Token::Type::Semicolon)) {
            m_lexer.consumeToken();
            continue;
        }

        {
            auto typeDefinition = backtrackingScope<Expected<AST::TypeDefinition, Error>>([&]() {
                return parseTypeDefinition();
            });
            if (typeDefinition) {
                auto success = program.append(WTFMove(*typeDefinition));
                if (!success)
                    return WTF::nullopt;
                continue;
            }
        }

        {
            auto structureDefinition = backtrackingScope<Expected<AST::StructureDefinition, Error>>([&]() {
                return parseStructureDefinition();
            });
            if (structureDefinition) {
                auto success = program.append(WTFMove(*structureDefinition));
                if (!success)
                    return WTF::nullopt;
                continue;
            }
        }

        {
            auto enumerationDefinition = backtrackingScope<Expected<AST::EnumerationDefinition, Error>>([&]() {
                return parseEnumerationDefinition();
            });
            if (enumerationDefinition) {
                auto success = program.append(WTFMove(*enumerationDefinition));
                if (!success)
                    return WTF::nullopt;
                continue;
            }
        }

        Optional<Error> error;
        {
            auto functionDefinition = backtrackingScope<Expected<AST::FunctionDefinition, Error>>([&]() {
                return parseFunctionDefinition();
            });
            if (functionDefinition) {
                auto success = program.append(WTFMove(*functionDefinition));
                if (!success)
                    return WTF::nullopt;
                continue;
            }
            error = functionDefinition.error();
        }

        if (m_mode == Mode::StandardLibrary) {
            auto nativeFunctionDeclaration = backtrackingScope<Expected<AST::NativeFunctionDeclaration, Error>>([&]() {
                return parseNativeFunctionDeclaration();
            });
            if (nativeFunctionDeclaration) {
                auto success = program.append(WTFMove(*nativeFunctionDeclaration));
                if (!success)
                    return WTF::nullopt;
                continue;
            }
        }

        if (m_mode == Mode::StandardLibrary) {
            auto nativeTypeDeclaration = backtrackingScope<Expected<AST::NativeTypeDeclaration, Error>>([&]() {
                return parseNativeTypeDeclaration();
            });
            if (nativeTypeDeclaration) {
                auto success = program.append(WTFMove(*nativeTypeDeclaration));
                if (!success)
                    return WTF::nullopt;
                continue;
            }
        }

        return WTFMove(*error);
    }
    return WTF::nullopt;
}

auto Parser::fail(const String& message) -> Unexpected<Error>
{
    if (auto nextToken = peek())
        return Unexpected<Error>(Error(m_lexer.errorString(*nextToken, message)));
    return Unexpected<Error>(Error(makeString("Cannot lex: ", message)));
}

auto Parser::peek() -> Expected<Lexer::Token, Error>
{
    if (auto token = m_lexer.consumeToken()) {
        m_lexer.unconsumeToken(Lexer::Token(*token));
        return *token;
    }
    return fail("Cannot consume token"_str);
}

Optional<Lexer::Token> Parser::tryType(Lexer::Token::Type type)
{
    if (auto token = m_lexer.consumeToken()) {
        if (token->type == type)
            return token;
        m_lexer.unconsumeToken(Lexer::Token(*token));
    }
    return WTF::nullopt;
}

Optional<Lexer::Token> Parser::tryTypes(Vector<Lexer::Token::Type> types)
{
    if (auto token = m_lexer.consumeToken()) {
        if (std::find(types.begin(), types.end(), token->type) != types.end())
            return token;
        m_lexer.unconsumeToken(Lexer::Token(*token));
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

auto Parser::consumeTypes(Vector<Lexer::Token::Type> types) -> Expected<Lexer::Token, Error>
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
        static_assert(std::numeric_limits<long long int>::min() < std::numeric_limits<int>::min(), "long long needs to be bigger than an int");
        if (static_cast<long long>(result) > std::abs(static_cast<long long>(std::numeric_limits<int>::min())))
            return Unexpected<Parser::Error>(Parser::Error(makeString("int literal ", text, " is out of bounds")));
        return { static_cast<int>(static_cast<long long>(result) * 1) };
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
    ASSERT(text.endsWith("u"_str));
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
        auto origin = consumeType(Lexer::Token::Type::FullStop);
        if (!origin)
            return Unexpected<Error>(origin.error());
        auto next = consumeType(Lexer::Token::Type::Identifier);
        if (!next)
            return Unexpected<Error>(next.error());
        return { AST::EnumerationMemberLiteral(WTFMove(*origin), type->stringView.toString(), next->stringView.toString()) };
    }
    }
}

auto Parser::parseTypeArgument() -> Expected<AST::TypeArgument, Error>
{
    auto constantExpression = backtrackingScope<Expected<AST::ConstantExpression, Error>>([&]() {
        return parseConstantExpression();
    });
    if (constantExpression)
        return AST::TypeArgument(WTFMove(*constantExpression));
    auto result = consumeType(Lexer::Token::Type::Identifier);
    if (!result)
        return Unexpected<Error>(result.error());
    return AST::TypeArgument(makeUniqueRef<AST::TypeReference>(Lexer::Token(*result), result->stringView.toString(), AST::TypeArguments()));
}

auto Parser::parseTypeArguments() -> Expected<AST::TypeArguments, Error>
{
    auto typeArguments = backtrackingScope<Optional<AST::TypeArguments>>([&]() -> Optional<AST::TypeArguments> {
        auto lessThanSign = consumeType(Lexer::Token::Type::LessThanSign);
        if (!lessThanSign)
            return WTF::nullopt;
        AST::TypeArguments typeArguments;
        auto typeArgument = parseTypeArgument();
        if (!typeArgument)
            return WTF::nullopt;
        typeArguments.append(WTFMove(*typeArgument));
        while (tryType(Lexer::Token::Type::Comma)) {
            auto typeArgument = parseTypeArgument();
            if (!typeArgument)
                return WTF::nullopt;
            typeArguments.append(WTFMove(*typeArgument));
        }
        auto greaterThanSign = consumeType(Lexer::Token::Type::GreaterThanSign);
        if (!greaterThanSign)
            return WTF::nullopt;
        return typeArguments;
    });
    if (typeArguments)
        return WTFMove(*typeArguments);

    typeArguments = backtrackingScope<Optional<AST::TypeArguments>>([&]() -> Optional<AST::TypeArguments> {
        auto lessThanSign = consumeType(Lexer::Token::Type::LessThanSign);
        if (!lessThanSign)
            return WTF::nullopt;
        auto greaterThanSign = consumeType(Lexer::Token::Type::GreaterThanSign);
        if (!greaterThanSign)
            return WTF::nullopt;
        return {{ }};
    });
    if (typeArguments)
        return WTFMove(*typeArguments);

    return AST::TypeArguments();
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
        auto rightSquareBracket = consumeType(Lexer::Token::Type::RightSquareBracket);
        if (!rightSquareBracket)
            return Unexpected<Error>(rightSquareBracket.error());
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
        auto rightSquareBracket = consumeType(Lexer::Token::Type::RightSquareBracket);
        if (!rightSquareBracket)
            return Unexpected<Error>(rightSquareBracket.error());
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

auto Parser::parseAddressSpaceType() -> Expected<UniqueRef<AST::UnnamedType>, Error>
{
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
    auto name = consumeType(Lexer::Token::Type::Identifier);
    if (!name)
        return Unexpected<Error>(name.error());
    auto typeArguments = parseTypeArguments();
    if (!typeArguments)
        return Unexpected<Error>(typeArguments.error());

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

    auto firstTypeSuffixAbbreviated = parseTypeSuffixAbbreviated();
    if (!firstTypeSuffixAbbreviated)
        return Unexpected<Error>(firstTypeSuffixAbbreviated.error());
    UniqueRef<AST::UnnamedType> result = makeUniqueRef<AST::TypeReference>(WTFMove(*addressSpaceToken), name->stringView.toString(), WTFMove(*typeArguments));
    auto next = constructTypeFromSuffixAbbreviated(*firstTypeSuffixAbbreviated, WTFMove(result));
    result = WTFMove(next);
    while (true) {
        auto typeSuffixAbbreviated = backtrackingScope<Expected<TypeSuffixAbbreviated, Error>>([&]() {
            return parseTypeSuffixAbbreviated();
        });
        if (!typeSuffixAbbreviated)
            break;
        // FIXME: The nesting here might be in the wrong order.
        next = constructTypeFromSuffixAbbreviated(*typeSuffixAbbreviated, WTFMove(result));
        result = WTFMove(next);
    }

    return WTFMove(result);
}

auto Parser::parseNonAddressSpaceType() -> Expected<UniqueRef<AST::UnnamedType>, Error>
{
    auto origin = peek();
    if (!origin)
        return Unexpected<Error>(origin.error());
    auto name = consumeType(Lexer::Token::Type::Identifier);
    if (!name)
        return Unexpected<Error>(name.error());
    auto typeArguments = parseTypeArguments();
    if (!typeArguments)
        return Unexpected<Error>(typeArguments.error());

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

    UniqueRef<AST::UnnamedType> result = makeUniqueRef<AST::TypeReference>(WTFMove(*origin), name->stringView.toString(), WTFMove(*typeArguments));
    while (true) {
        auto typeSuffixNonAbbreviated = backtrackingScope<Expected<TypeSuffixNonAbbreviated, Error>>([&]() {
            return parseTypeSuffixNonAbbreviated();
        });
        if (!typeSuffixNonAbbreviated)
            break;
        // FIXME: The nesting here might be in the wrong order.
        auto next = constructTypeFromSuffixNonAbbreviated(*typeSuffixNonAbbreviated, WTFMove(result));
        result = WTFMove(next);
    }

    return WTFMove(result);
}

auto Parser::parseType() -> Expected<UniqueRef<AST::UnnamedType>, Error>
{
    auto type = backtrackingScope<Expected<UniqueRef<AST::UnnamedType>, Error>>([&]() {
        return parseAddressSpaceType();
    });
    if (type)
        return type;

    type = backtrackingScope<Expected<UniqueRef<AST::UnnamedType>, Error>>([&]() {
        return parseNonAddressSpaceType();
    });
    if (type)
        return type;

    return Unexpected<Error>(type.error());
}

auto Parser::parseTypeDefinition() -> Expected<AST::TypeDefinition, Error>
{
    auto origin = consumeType(Lexer::Token::Type::Typedef);
    if (!origin)
        return Unexpected<Error>(origin.error());
    auto name = consumeType(Lexer::Token::Type::Identifier);
    if (!name)
        return Unexpected<Error>(name.error());
    auto equals = consumeType(Lexer::Token::Type::EqualsSign);
    if (!equals)
        return Unexpected<Error>(equals.error());
    auto type = parseType();
    if (!type)
        return Unexpected<Error>(type.error());
    auto semicolon = consumeType(Lexer::Token::Type::Semicolon);
    if (!semicolon)
        return Unexpected<Error>(semicolon.error());
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
        auto target = consumeNonNegativeIntegralLiteral();
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
    auto origin = consumeType(Lexer::Token::Type::Register);
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto leftParenthesis = consumeType(Lexer::Token::Type::LeftParenthesis);
    if (!leftParenthesis)
        return Unexpected<Error>(leftParenthesis.error());

    auto info = consumeType(Lexer::Token::Type::Identifier);
    if (!info)
        return Unexpected<Error>(info.error());
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
        auto spaceToken = consumeType(Lexer::Token::Type::Identifier);
        if (!spaceToken)
            return Unexpected<Error>(spaceToken.error());
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

    auto rightParenthesis = consumeType(Lexer::Token::Type::RightParenthesis);
    if (!rightParenthesis)
        return Unexpected<Error>(rightParenthesis.error());

    return AST::ResourceSemantic(WTFMove(*origin), mode, *index, space);
}

auto Parser::parseSpecializationConstantSemantic() -> Expected<AST::SpecializationConstantSemantic, Error>
{
    auto origin = consumeType(Lexer::Token::Type::Specialized);
    if (!origin)
        return Unexpected<Error>(origin.error());
    return AST::SpecializationConstantSemantic(WTFMove(*origin));
}

auto Parser::parseStageInOutSemantic() -> Expected<AST::StageInOutSemantic, Error>
{
    auto origin = consumeType(Lexer::Token::Type::Attribute);
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto leftParenthesis = consumeType(Lexer::Token::Type::LeftParenthesis);
    if (!leftParenthesis)
        return Unexpected<Error>(leftParenthesis.error());

    auto index = consumeNonNegativeIntegralLiteral();
    if (!index)
        return Unexpected<Error>(index.error());

    auto rightParenthesis = consumeType(Lexer::Token::Type::RightParenthesis);
    if (!rightParenthesis)
        return Unexpected<Error>(rightParenthesis.error());

    return AST::StageInOutSemantic(WTFMove(*origin), *index);
}

auto Parser::parseSemantic() -> Expected<AST::Semantic, Error>
{
    auto builtInSemantic = backtrackingScope<Expected<AST::BuiltInSemantic, Error>>([&]() {
        return parseBuiltInSemantic();
    });
    if (builtInSemantic)
        return AST::Semantic(WTFMove(*builtInSemantic));

    auto resourceSemantic = backtrackingScope<Expected<AST::ResourceSemantic, Error>>([&]() {
        return parseResourceSemantic();
    });
    if (resourceSemantic)
        return AST::Semantic(WTFMove(*resourceSemantic));

    auto specializationConstantSemantic = backtrackingScope<Expected<AST::SpecializationConstantSemantic, Error>>([&]() {
        return parseSpecializationConstantSemantic();
    });
    if (specializationConstantSemantic)
        return AST::Semantic(WTFMove(*specializationConstantSemantic));

    auto stageInOutSemantic = backtrackingScope<Expected<AST::StageInOutSemantic, Error>>([&]() {
        return parseStageInOutSemantic();
    });
    if (stageInOutSemantic)
        return AST::Semantic(WTFMove(*stageInOutSemantic));

    return Unexpected<Error>(stageInOutSemantic.error());
}
AST::Qualifiers Parser::parseQualifiers()
{
    AST::Qualifiers qualifiers;
    while (true) {
        if (auto next = tryType(Lexer::Token::Type::Qualifier)) {
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
        } else
            break;
    }
    return qualifiers;
}

auto Parser::parseStructureElement() -> Expected<AST::StructureElement, Error>
{
    auto origin = peek();
    if (!origin)
        return Unexpected<Error>(origin.error());

    AST::Qualifiers qualifiers = parseQualifiers();

    auto type = parseType();
    if (!type)
        return Unexpected<Error>(type.error());

    auto name = consumeType(Lexer::Token::Type::Identifier);
    if (!name)
        return Unexpected<Error>(name.error());

    if (tryType(Lexer::Token::Type::Colon)) {
        auto semantic = parseSemantic();
        if (!semantic)
            return Unexpected<Error>(semantic.error());

        auto semicolon = consumeType(Lexer::Token::Type::Semicolon);
        if (!semicolon)
            return Unexpected<Error>(semicolon.error());

        return AST::StructureElement(WTFMove(*origin), WTFMove(qualifiers), WTFMove(*type), name->stringView.toString(), WTFMove(*semantic));
    }

    auto semicolon = consumeType(Lexer::Token::Type::Semicolon);
    if (!semicolon)
        return Unexpected<Error>(semicolon.error());

    return AST::StructureElement(WTFMove(*origin), WTFMove(qualifiers), WTFMove(*type), name->stringView.toString(), WTF::nullopt);
}

auto Parser::parseStructureDefinition() -> Expected<AST::StructureDefinition, Error>
{
    auto origin = consumeType(Lexer::Token::Type::Struct);
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto name = consumeType(Lexer::Token::Type::Identifier);
    if (!name)
        return Unexpected<Error>(name.error());

    auto leftCurlyBracket = consumeType(Lexer::Token::Type::LeftCurlyBracket);
    if (!leftCurlyBracket)
        return Unexpected<Error>(leftCurlyBracket.error());

    AST::StructureElements structureElements;
    while (true) {
        auto structureElement = backtrackingScope<Expected<AST::StructureElement, Error>>([&]() {
            return parseStructureElement();
        });
        if (structureElement)
            structureElements.append(WTFMove(*structureElement));
        else
            break;
    }

    auto rightCurlyBracket = consumeType(Lexer::Token::Type::RightCurlyBracket);
    if (!rightCurlyBracket)
        return Unexpected<Error>(rightCurlyBracket.error());

    return AST::StructureDefinition(WTFMove(*origin), name->stringView.toString(), WTFMove(structureElements));
}

auto Parser::parseEnumerationDefinition() -> Expected<AST::EnumerationDefinition, Error>
{
    auto origin = consumeType(Lexer::Token::Type::Enum);
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto name = consumeType(Lexer::Token::Type::Identifier);
    if (!name)
        return Unexpected<Error>(name.error());

    auto type = ([&]() -> Expected<UniqueRef<AST::UnnamedType>, Error> {
        if (tryType(Lexer::Token::Type::Colon)) {
            auto parsedType = parseType();
            if (!parsedType)
                return Unexpected<Error>(parsedType.error());
            return WTFMove(*parsedType);
        }
        return { makeUniqueRef<AST::TypeReference>(Lexer::Token(*origin), "int"_str, AST::TypeArguments()) };
    })();
    if (!type)
        return Unexpected<Error>(type.error());

    auto leftCurlyBracket = consumeType(Lexer::Token::Type::LeftCurlyBracket);
    if (!leftCurlyBracket)
        return Unexpected<Error>(leftCurlyBracket.error());

    auto firstEnumerationMember = parseEnumerationMember();
    if (!firstEnumerationMember)
        return Unexpected<Error>(firstEnumerationMember.error());

    AST::EnumerationDefinition result(WTFMove(*origin), name->stringView.toString(), WTFMove(*type));
    auto success = result.add(WTFMove(*firstEnumerationMember));
    if (!success)
        return fail("Cannot add enumeration member"_str);

    while (tryType(Lexer::Token::Type::Comma)) {
        auto member = parseEnumerationMember();
        if (!member)
            return Unexpected<Error>(member.error());
        success = result.add(WTFMove(*member));
        if (!success)
            return fail("Cannot add enumeration member"_str);
    }

    auto rightCurlyBracket = consumeType(Lexer::Token::Type::RightCurlyBracket);
    if (!rightCurlyBracket)
        return Unexpected<Error>(rightCurlyBracket.error());

    return WTFMove(result);
}

auto Parser::parseEnumerationMember() -> Expected<AST::EnumerationMember, Error>
{
    auto identifier = consumeType(Lexer::Token::Type::Identifier);
    if (!identifier)
        return Unexpected<Error>(identifier.error());
    auto name = identifier->stringView.toString();

    if (tryType(Lexer::Token::Type::EqualsSign)) {
        auto constantExpression = parseConstantExpression();
        if (!constantExpression)
            return Unexpected<Error>(constantExpression.error());
        return AST::EnumerationMember(Lexer::Token(*identifier), WTFMove(name), WTFMove(*constantExpression));
    }
    return AST::EnumerationMember(Lexer::Token(*identifier), WTFMove(name));
}

auto Parser::parseNativeTypeDeclaration() -> Expected<AST::NativeTypeDeclaration, Error>
{
    auto origin = consumeType(Lexer::Token::Type::Native);
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto parsedTypedef = consumeType(Lexer::Token::Type::Typedef);
    if (!parsedTypedef)
        return Unexpected<Error>(parsedTypedef.error());

    auto name = consumeType(Lexer::Token::Type::Identifier);
    if (!name)
        return Unexpected<Error>(name.error());

    auto typeArguments = parseTypeArguments();
    if (!typeArguments)
        return Unexpected<Error>(typeArguments.error());

    auto semicolon = consumeType(Lexer::Token::Type::Semicolon);
    if (!semicolon)
        return Unexpected<Error>(semicolon.error());

    return AST::NativeTypeDeclaration(WTFMove(*origin), name->stringView.toString(), WTFMove(*typeArguments));
}

auto Parser::parseNumThreadsFunctionAttribute() -> Expected<AST::NumThreadsFunctionAttribute, Error>
{
    auto origin = consumeType(Lexer::Token::Type::NumThreads);
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto leftParenthesis = consumeType(Lexer::Token::Type::LeftParenthesis);
    if (!leftParenthesis)
        return Unexpected<Error>(leftParenthesis.error());

    auto width = consumeNonNegativeIntegralLiteral();
    if (!width)
        return Unexpected<Error>(width.error());

    auto comma = consumeType(Lexer::Token::Type::Comma);
    if (!comma)
        return Unexpected<Error>(comma.error());

    auto height = consumeNonNegativeIntegralLiteral();
    if (!height)
        return Unexpected<Error>(height.error());

    comma = consumeType(Lexer::Token::Type::Comma);
    if (!comma)
        return Unexpected<Error>(comma.error());

    auto depth = consumeNonNegativeIntegralLiteral();
    if (!depth)
        return Unexpected<Error>(depth.error());

    auto rightParenthesis = consumeType(Lexer::Token::Type::RightParenthesis);
    if (!rightParenthesis)
        return Unexpected<Error>(rightParenthesis.error());

    return AST::NumThreadsFunctionAttribute(WTFMove(*origin), *width, *height, *depth);
}

auto Parser::parseAttributeBlock() -> Expected<AST::AttributeBlock, Error>
{
    auto leftSquareBracket = consumeType(Lexer::Token::Type::LeftSquareBracket);
    if (!leftSquareBracket)
        return Unexpected<Error>(leftSquareBracket.error());

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

    auto rightSquareBracket = consumeType(Lexer::Token::Type::RightSquareBracket);
    if (!rightSquareBracket)
        return Unexpected<Error>(rightSquareBracket.error());

    return WTFMove(result);
}

auto Parser::parseParameter() -> Expected<AST::VariableDeclaration, Error>
{
    auto origin = peek();
    if (!origin)
        return Unexpected<Error>(origin.error());

    AST::Qualifiers qualifiers = parseQualifiers();

    auto type = parseType();
    if (!type)
        return Unexpected<Error>(type.error());

    String name;
    if (auto token = tryType(Lexer::Token::Type::Identifier))
        name = token->stringView.toString();

    if (tryType(Lexer::Token::Type::Colon)) {
        auto semantic = parseSemantic();
        if (!semantic)
            return Unexpected<Error>(semantic.error());
        return AST::VariableDeclaration(WTFMove(*origin), WTFMove(qualifiers), Optional<UniqueRef<AST::UnnamedType>>(WTFMove(*type)), WTFMove(name), WTFMove(*semantic), WTF::nullopt);
    }

    return AST::VariableDeclaration(WTFMove(*origin), WTFMove(qualifiers), { WTFMove(*type) }, WTFMove(name), WTF::nullopt, WTF::nullopt);
}

auto Parser::parseParameters() -> Expected<AST::VariableDeclarations, Error>
{
    auto leftParenthesis = consumeType(Lexer::Token::Type::LeftParenthesis);
    if (!leftParenthesis)
        return Unexpected<Error>(leftParenthesis.error());

    AST::VariableDeclarations parameters;
    if (tryType(Lexer::Token::Type::RightParenthesis))
        return WTFMove(parameters);

    auto firstParameter = parseParameter();
    if (!firstParameter)
        return Unexpected<Error>(firstParameter.error());
    parameters.append(WTFMove(*firstParameter));

    while (tryType(Lexer::Token::Type::Comma)) {
        auto parameter = parseParameter();
        if (!parameter)
            return Unexpected<Error>(parameter.error());
        parameters.append(WTFMove(*parameter));
    }

    auto rightParenthesis = consumeType(Lexer::Token::Type::RightParenthesis);
    if (!rightParenthesis)
        return Unexpected<Error>(rightParenthesis.error());

    return WTFMove(parameters);
}

auto Parser::parseFunctionDefinition() -> Expected<AST::FunctionDefinition, Error>
{
    bool restricted = static_cast<bool>(tryType(Lexer::Token::Type::Restricted));

    auto functionDeclaration = parseFunctionDeclaration();
    if (!functionDeclaration)
        return Unexpected<Error>(functionDeclaration.error());

    auto block = parseBlock();
    if (!block)
        return Unexpected<Error>(block.error());

    return AST::FunctionDefinition(WTFMove(*functionDeclaration), WTFMove(*block), restricted);
}

auto Parser::parseEntryPointFunctionDeclaration() -> Expected<AST::FunctionDeclaration, Error>
{
    auto origin = peek();
    if (!origin)
        return Unexpected<Error>(origin.error());

    AST::AttributeBlock attributeBlock;
    AST::EntryPointType entryPointType;

    auto parsedAttributeBlock = backtrackingScope<Expected<AST::AttributeBlock, Error>>([&]() {
        return parseAttributeBlock();
    });
    if (parsedAttributeBlock) {
        auto compute = consumeType(Lexer::Token::Type::Compute);
        if (!compute)
            return Unexpected<Error>(compute.error());
        attributeBlock = WTFMove(*parsedAttributeBlock);
        entryPointType = AST::EntryPointType::Compute;
    } else {
        auto type = consumeTypes({ Lexer::Token::Type::Vertex, Lexer::Token::Type::Fragment });
        if (!type)
            return Unexpected<Error>(type.error());

        switch (origin->type) {
        case Lexer::Token::Type::Vertex:
            entryPointType = AST::EntryPointType::Vertex;
            break;
        default:
            ASSERT(origin->type == Lexer::Token::Type::Fragment);
            entryPointType = AST::EntryPointType::Fragment;
            break;
        }
    }

    auto type = parseType();
    if (!type)
        return Unexpected<Error>(type.error());

    auto name = consumeType(Lexer::Token::Type::Identifier);
    if (!name)
        return Unexpected<Error>(name.error());

    auto parameters = parseParameters();
    if (!parameters)
        return Unexpected<Error>(parameters.error());

    bool isOperator = false;

    if (tryType(Lexer::Token::Type::Colon)) {
        auto semantic = parseSemantic();
        if (!semantic)
            return Unexpected<Error>(semantic.error());
        return AST::FunctionDeclaration(WTFMove(*origin), WTFMove(attributeBlock), entryPointType, WTFMove(*type), name->stringView.toString(), WTFMove(*parameters), WTFMove(*semantic), isOperator);
    }

    return AST::FunctionDeclaration(WTFMove(*origin), WTFMove(attributeBlock), entryPointType, WTFMove(*type), name->stringView.toString(), WTFMove(*parameters), WTF::nullopt, isOperator);
}

auto Parser::parseRegularFunctionDeclaration() -> Expected<AST::FunctionDeclaration, Error>
{
    auto origin = peek();
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto type = parseType();
    if (!type)
        return Unexpected<Error>(type.error());

    auto name = consumeTypes({ Lexer::Token::Type::Identifier, Lexer::Token::Type::OperatorName });
    if (!name)
        return Unexpected<Error>(name.error());
    auto isOperator = name->type == Lexer::Token::Type::OperatorName;

    auto parameters = parseParameters();
    if (!parameters)
        return Unexpected<Error>(parameters.error());

    if (tryType(Lexer::Token::Type::Colon)) {
        auto semantic = parseSemantic();
        if (!semantic)
            return Unexpected<Error>(semantic.error());
        return AST::FunctionDeclaration(WTFMove(*origin), { }, WTF::nullopt, WTFMove(*type), name->stringView.toString(), WTFMove(*parameters), WTFMove(*semantic), isOperator);
    }

    return AST::FunctionDeclaration(WTFMove(*origin), { }, WTF::nullopt, WTFMove(*type), name->stringView.toString(), WTFMove(*parameters), WTF::nullopt, isOperator);
}

auto Parser::parseOperatorFunctionDeclaration() -> Expected<AST::FunctionDeclaration, Error>
{
    auto origin = consumeType(Lexer::Token::Type::Operator);
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto type = parseType();
    if (!type)
        return Unexpected<Error>(type.error());

    auto parameters = parseParameters();
    if (!parameters)
        return Unexpected<Error>(parameters.error());

    bool isOperator = true;

    if (tryType(Lexer::Token::Type::Colon)) {
        auto semantic = parseSemantic();
        if (!semantic)
            return Unexpected<Error>(semantic.error());
        return AST::FunctionDeclaration(WTFMove(*origin), { }, WTF::nullopt, WTFMove(*type), "operator cast"_str, WTFMove(*parameters), WTFMove(*semantic), isOperator);
    }

    return AST::FunctionDeclaration(WTFMove(*origin), { }, WTF::nullopt, WTFMove(*type), "operator cast"_str, WTFMove(*parameters), WTF::nullopt, isOperator);
}

auto Parser::parseFunctionDeclaration() -> Expected<AST::FunctionDeclaration, Error>
{
    auto entryPointFunctionDeclaration = backtrackingScope<Expected<AST::FunctionDeclaration, Error>>([&]() {
        return parseEntryPointFunctionDeclaration();
    });
    if (entryPointFunctionDeclaration)
        return WTFMove(*entryPointFunctionDeclaration);

    auto regularFunctionDeclaration = backtrackingScope<Expected<AST::FunctionDeclaration, Error>>([&]() {
        return parseRegularFunctionDeclaration();
    });
    if (regularFunctionDeclaration)
        return WTFMove(*regularFunctionDeclaration);

    auto operatorFunctionDeclaration = backtrackingScope<Expected<AST::FunctionDeclaration, Error>>([&]() {
        return parseOperatorFunctionDeclaration();
    });
    if (operatorFunctionDeclaration)
        return WTFMove(*operatorFunctionDeclaration);

    return Unexpected<Error>(operatorFunctionDeclaration.error());
}

auto Parser::parseNativeFunctionDeclaration() -> Expected<AST::NativeFunctionDeclaration, Error>
{
    Optional<Lexer::Token> origin;

    bool restricted = false;
    if (auto restrictedValue = tryType(Lexer::Token::Type::Restricted)) {
        origin = *restrictedValue;
        restricted = true;
    }

    auto native = consumeType(Lexer::Token::Type::Native);
    if (!native)
        return Unexpected<Error>(native.error());
    if (!origin)
        origin = *native;

    auto functionDeclaration = parseFunctionDeclaration();
    if (!functionDeclaration)
        return Unexpected<Error>(functionDeclaration.error());

    auto semicolon = consumeType(Lexer::Token::Type::Semicolon);
    if (!semicolon)
        return Unexpected<Error>(semicolon.error());

    return AST::NativeFunctionDeclaration(WTFMove(*functionDeclaration), restricted);
}

auto Parser::parseBlock() -> Expected<AST::Block, Error>
{
    auto origin = consumeType(Lexer::Token::Type::LeftCurlyBracket);
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto result = parseBlockBody(WTFMove(*origin));

    auto rightCurlyBracket = consumeType(Lexer::Token::Type::RightCurlyBracket);
    if (!rightCurlyBracket)
        return Unexpected<Error>(rightCurlyBracket.error());

    return WTFMove(result);
}

AST::Block Parser::parseBlockBody(Lexer::Token&& origin)
{
    AST::Statements statements;
    while (true) {
        auto statement = backtrackingScope<Expected<UniqueRef<AST::Statement>, Error>>([&]() {
            return parseStatement();
        });
        if (statement)
            statements.append(WTFMove(*statement));
        else
            break;
    }
    return AST::Block(WTFMove(origin), WTFMove(statements));
}

auto Parser::parseIfStatement() -> Expected<AST::IfStatement, Error>
{
    auto origin = consumeType(Lexer::Token::Type::If);
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto leftParenthesis = consumeType(Lexer::Token::Type::LeftParenthesis);
    if (!leftParenthesis)
        return Unexpected<Error>(leftParenthesis.error());

    auto conditional = parseExpression();
    if (!conditional)
        return Unexpected<Error>(conditional.error());

    auto rightParenthesis = consumeType(Lexer::Token::Type::RightParenthesis);
    if (!rightParenthesis)
        return Unexpected<Error>(rightParenthesis.error());

    auto body = parseStatement();
    if (!body)
        return Unexpected<Error>(body.error());

    Optional<UniqueRef<AST::Statement>> elseBody;
    if (tryType(Lexer::Token::Type::Else)) {
        auto parsedElseBody = parseStatement();
        if (!parsedElseBody)
            return Unexpected<Error>(parsedElseBody.error());
        elseBody = WTFMove(*parsedElseBody);
    }

    Vector<UniqueRef<AST::Expression>> castArguments;
    castArguments.append(WTFMove(*conditional));
    auto boolCast = makeUniqueRef<AST::CallExpression>(Lexer::Token(*origin), "bool"_str, WTFMove(castArguments));
    return AST::IfStatement(WTFMove(*origin), WTFMove(boolCast), WTFMove(*body), WTFMove(elseBody));
}

auto Parser::parseSwitchStatement() -> Expected<AST::SwitchStatement, Error>
{
    auto origin = consumeType(Lexer::Token::Type::Switch);
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto leftParenthesis = consumeType(Lexer::Token::Type::LeftParenthesis);
    if (!leftParenthesis)
        return Unexpected<Error>(leftParenthesis.error());

    auto value = parseExpression();
    if (!value)
        return Unexpected<Error>(value.error());

    auto rightParenthesis = consumeType(Lexer::Token::Type::RightParenthesis);
    if (!rightParenthesis)
        return Unexpected<Error>(rightParenthesis.error());

    auto leftCurlyBracket = consumeType(Lexer::Token::Type::LeftCurlyBracket);
    if (!leftCurlyBracket)
        return Unexpected<Error>(leftCurlyBracket.error());

    Vector<AST::SwitchCase> switchCases;
    while (true) {
        auto switchCase = backtrackingScope<Expected<AST::SwitchCase, Error>>([&]() {
            return parseSwitchCase();
        });
        if (switchCase)
            switchCases.append(WTFMove(*switchCase));
        else
            break;
    }

    auto rightCurlyBracket = consumeType(Lexer::Token::Type::RightCurlyBracket);
    if (!rightCurlyBracket)
        return Unexpected<Error>(rightCurlyBracket.error());

    return AST::SwitchStatement(WTFMove(*origin), WTFMove(*value), WTFMove(switchCases));
}

auto Parser::parseSwitchCase() -> Expected<AST::SwitchCase, Error>
{
    auto origin = consumeTypes({ Lexer::Token::Type::Case, Lexer::Token::Type::Default });
    if (!origin)
        return Unexpected<Error>(origin.error());

    switch (origin->type) {
    case Lexer::Token::Type::Case: {
        auto value = parseConstantExpression();
        if (!value)
            return Unexpected<Error>(value.error());

        auto origin = consumeType(Lexer::Token::Type::Colon);
        if (!origin)
            return Unexpected<Error>(origin.error());

        auto block = parseBlockBody(Lexer::Token(*origin));

        return AST::SwitchCase(WTFMove(*origin), WTFMove(*value), WTFMove(block));
    }
    default: {
        ASSERT(origin->type == Lexer::Token::Type::Default);
        auto origin = consumeType(Lexer::Token::Type::Colon);
        if (!origin)
            return Unexpected<Error>(origin.error());

        auto block = parseBlockBody(Lexer::Token(*origin));

        return AST::SwitchCase(WTFMove(*origin), WTF::nullopt, WTFMove(block));
    }
    }
}

auto Parser::parseForLoop() -> Expected<AST::ForLoop, Error>
{
    auto origin = consumeType(Lexer::Token::Type::For);
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto parseRemainder = [&](Variant<AST::VariableDeclarationsStatement, UniqueRef<AST::Expression>>&& initialization) -> Expected<AST::ForLoop, Error> {
        auto semicolon = consumeType(Lexer::Token::Type::Semicolon);
        if (!semicolon)
            return Unexpected<Error>(semicolon.error());

        auto condition = backtrackingScope<Optional<UniqueRef<AST::Expression>>>([&]() -> Optional<UniqueRef<AST::Expression>> {
            if (auto expression = parseExpression())
                return { WTFMove(*expression) };
            return WTF::nullopt;
        });

        semicolon = consumeType(Lexer::Token::Type::Semicolon);
        if (!semicolon)
            return Unexpected<Error>(semicolon.error());

        auto increment = backtrackingScope<Optional<UniqueRef<AST::Expression>>>([&]() -> Optional<UniqueRef<AST::Expression>> {
            if (auto expression = parseExpression())
                return { WTFMove(*expression) };
            return WTF::nullopt;
        });

        auto rightParenthesis = consumeType(Lexer::Token::Type::RightParenthesis);
        if (!rightParenthesis)
            return Unexpected<Error>(rightParenthesis.error());

        auto body = parseStatement();
        if (!body)
            return Unexpected<Error>(body.error());

        return AST::ForLoop(WTFMove(*origin), WTFMove(initialization), WTFMove(condition), WTFMove(increment), WTFMove(*body));
    };

    auto leftParenthesis = consumeType(Lexer::Token::Type::LeftParenthesis);
    if (!leftParenthesis)
        return Unexpected<Error>(leftParenthesis.error());

    auto variableDeclarations = backtrackingScope<Expected<AST::VariableDeclarationsStatement, Error>>([&]() {
        return parseVariableDeclarations();
    });
    if (variableDeclarations)
        return parseRemainder(WTFMove(*variableDeclarations));

    auto effectfulExpression = parseEffectfulExpression();
    if (!effectfulExpression)
        return Unexpected<Error>(effectfulExpression.error());

    return parseRemainder(WTFMove(*effectfulExpression));
}

auto Parser::parseWhileLoop() -> Expected<AST::WhileLoop, Error>
{
    auto origin = consumeType(Lexer::Token::Type::While);
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto leftParenthesis = consumeType(Lexer::Token::Type::LeftParenthesis);
    if (!leftParenthesis)
        return Unexpected<Error>(leftParenthesis.error());

    auto conditional = parseExpression();
    if (!conditional)
        return Unexpected<Error>(conditional.error());

    auto rightParenthesis = consumeType(Lexer::Token::Type::RightParenthesis);
    if (!rightParenthesis)
        return Unexpected<Error>(rightParenthesis.error());

    auto body = parseStatement();
    if (!body)
        return Unexpected<Error>(body.error());

    return AST::WhileLoop(WTFMove(*origin), WTFMove(*conditional), WTFMove(*body));
}

auto Parser::parseDoWhileLoop() -> Expected<AST::DoWhileLoop, Error>
{
    auto origin = consumeType(Lexer::Token::Type::Do);
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto body = parseStatement();
    if (!body)
        return Unexpected<Error>(body.error());

    auto whileKeyword = consumeType(Lexer::Token::Type::While);
    if (!whileKeyword)
        return Unexpected<Error>(whileKeyword.error());

    auto leftParenthesis = consumeType(Lexer::Token::Type::LeftParenthesis);
    if (!leftParenthesis)
        return Unexpected<Error>(leftParenthesis.error());

    auto conditional = parseExpression();
    if (!conditional)
        return Unexpected<Error>(conditional.error());

    auto rightParenthesis = consumeType(Lexer::Token::Type::RightParenthesis);
    if (!rightParenthesis)
        return Unexpected<Error>(rightParenthesis.error());

    return AST::DoWhileLoop(WTFMove(*origin), WTFMove(*body), WTFMove(*conditional));
}

auto Parser::parseVariableDeclaration(UniqueRef<AST::UnnamedType>&& type) -> Expected<AST::VariableDeclaration, Error>
{
    auto origin = peek();
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto qualifiers = parseQualifiers();

    auto name = consumeType(Lexer::Token::Type::Identifier);
    if (!name)
        return Unexpected<Error>(name.error());

    if (tryType(Lexer::Token::Type::Colon)) {
        auto semantic = parseSemantic();
        if (!semantic)
            return Unexpected<Error>(semantic.error());

        if (tryType(Lexer::Token::Type::EqualsSign)) {
            auto initializer = parseExpression();
            if (!initializer)
                return Unexpected<Error>(initializer.error());
            return AST::VariableDeclaration(WTFMove(*origin), WTFMove(qualifiers), { WTFMove(type) }, name->stringView.toString(), WTFMove(*semantic), WTFMove(*initializer));
        }

        return AST::VariableDeclaration(WTFMove(*origin), WTFMove(qualifiers), { WTFMove(type) }, name->stringView.toString(), WTFMove(*semantic), WTF::nullopt);
    }

    if (tryType(Lexer::Token::Type::EqualsSign)) {
        auto initializer = parseExpression();
        if (!initializer)
            return Unexpected<Error>(initializer.error());
        return AST::VariableDeclaration(WTFMove(*origin), WTFMove(qualifiers), { WTFMove(type) }, name->stringView.toString(), WTF::nullopt, WTFMove(*initializer));
    }

    return AST::VariableDeclaration(WTFMove(*origin), WTFMove(qualifiers), { WTFMove(type) }, name->stringView.toString(), WTF::nullopt, WTF::nullopt);
}

auto Parser::parseVariableDeclarations() -> Expected<AST::VariableDeclarationsStatement, Error>
{
    auto origin = peek();
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto type = parseType();
    if (!type)
        return Unexpected<Error>(type.error());

    auto firstVariableDeclaration = parseVariableDeclaration((*type)->clone());
    if (!firstVariableDeclaration)
        return Unexpected<Error>(firstVariableDeclaration.error());

    Vector<AST::VariableDeclaration> result;
    result.append(WTFMove(*firstVariableDeclaration));

    while (tryType(Lexer::Token::Type::Comma)) {
        auto variableDeclaration = parseVariableDeclaration((*type)->clone());
        if (!variableDeclaration)
            return Unexpected<Error>(variableDeclaration.error());
        result.append(WTFMove(*variableDeclaration));
    }

    return AST::VariableDeclarationsStatement(WTFMove(*origin), WTFMove(result));
}

auto Parser::parseStatement() -> Expected<UniqueRef<AST::Statement>, Error>
{
    {
        auto block = backtrackingScope<Expected<AST::Block, Error>>([&]() {
            return parseBlock();
        });
        if (block)
            return { makeUniqueRef<AST::Block>(WTFMove(*block)) };
    }

    {
        auto ifStatement = backtrackingScope<Expected<AST::IfStatement, Error>>([&]() {
            return parseIfStatement();
        });
        if (ifStatement)
            return { makeUniqueRef<AST::IfStatement>(WTFMove(*ifStatement)) };
    }

    {
        auto switchStatement = backtrackingScope<Expected<AST::SwitchStatement, Error>>([&]() {
            return parseSwitchStatement();
        });
        if (switchStatement)
            return { makeUniqueRef<AST::SwitchStatement>(WTFMove(*switchStatement)) };
    }

    {
        auto forLoop = backtrackingScope<Expected<AST::ForLoop, Error>>([&]() {
            return parseForLoop();
        });
        if (forLoop)
            return { makeUniqueRef<AST::ForLoop>(WTFMove(*forLoop)) };
    }

    {
        auto whileLoop = backtrackingScope<Expected<AST::WhileLoop, Error>>([&]() {
            return parseWhileLoop();
        });
        if (whileLoop)
            return { makeUniqueRef<AST::WhileLoop>(WTFMove(*whileLoop)) };
    }

    {
        auto doWhileLoop = backtrackingScope<Expected<AST::DoWhileLoop, Error>>([&]() -> Expected<AST::DoWhileLoop, Error> {
            auto result = parseDoWhileLoop();
            if (!result)
                return Unexpected<Error>(result.error());

            auto semicolon = consumeType(Lexer::Token::Type::Semicolon);
            if (!semicolon)
                return Unexpected<Error>(semicolon.error());

            return result;
        });
        if (doWhileLoop)
            return { makeUniqueRef<AST::DoWhileLoop>(WTFMove(*doWhileLoop)) };
    }

    {
        auto breakObject = backtrackingScope<Expected<AST::Break, Error>>([&]() -> Expected<AST::Break, Error> {
            auto origin = consumeType(Lexer::Token::Type::Break);
            if (!origin)
                return Unexpected<Error>(origin.error());

            auto semicolon = consumeType(Lexer::Token::Type::Semicolon);
            if (!semicolon)
                return Unexpected<Error>(semicolon.error());

            return AST::Break(WTFMove(*origin));
        });
        if (breakObject)
            return { makeUniqueRef<AST::Break>(WTFMove(*breakObject)) };
    }

    {
        auto continueObject = backtrackingScope<Expected<AST::Continue, Error>>([&]() -> Expected<AST::Continue, Error> {
            auto origin = consumeType(Lexer::Token::Type::Continue);
            if (!origin)
                return Unexpected<Error>(origin.error());

            auto semicolon = consumeType(Lexer::Token::Type::Semicolon);
            if (!semicolon)
                return Unexpected<Error>(semicolon.error());

            return AST::Continue(WTFMove(*origin));
        });
        if (continueObject)
            return { makeUniqueRef<AST::Continue>(WTFMove(*continueObject)) };
    }

    {
        auto fallthroughObject = backtrackingScope<Expected<AST::Fallthrough, Error>>([&]() -> Expected<AST::Fallthrough, Error> {
            auto origin = consumeType(Lexer::Token::Type::Fallthrough);
            if (!origin)
                return Unexpected<Error>(origin.error());

            auto semicolon = consumeType(Lexer::Token::Type::Semicolon);
            if (!semicolon)
                return Unexpected<Error>(semicolon.error());

            return AST::Fallthrough(WTFMove(*origin));
        });
        if (fallthroughObject)
            return { makeUniqueRef<AST::Fallthrough>(WTFMove(*fallthroughObject)) };
    }

    {
        auto trapObject = backtrackingScope<Expected<AST::Trap, Error>>([&]() -> Expected<AST::Trap, Error> {
            auto origin = consumeType(Lexer::Token::Type::Trap);
            if (!origin)
                return Unexpected<Error>(origin.error());

            auto semicolon = consumeType(Lexer::Token::Type::Semicolon);
            if (!semicolon)
                return Unexpected<Error>(semicolon.error());

            return AST::Trap(WTFMove(*origin));
        });
        if (trapObject)
            return { makeUniqueRef<AST::Trap>(WTFMove(*trapObject)) };
    }

    {
        auto returnObject = backtrackingScope<Expected<AST::Return, Error>>([&]() -> Expected<AST::Return, Error> {
            auto origin = consumeType(Lexer::Token::Type::Return);
            if (!origin)
                return Unexpected<Error>(origin.error());

            if (auto semicolon = tryType(Lexer::Token::Type::Semicolon))
                return AST::Return(WTFMove(*origin), WTF::nullopt);

            auto expression = parseExpression();
            if (!expression)
                return Unexpected<Error>(expression.error());

            auto semicolon = consumeType(Lexer::Token::Type::Semicolon);
            if (!semicolon)
                return Unexpected<Error>(semicolon.error());

            return AST::Return(WTFMove(*origin), { WTFMove(*expression) });
        });
        if (returnObject)
            return { makeUniqueRef<AST::Return>(WTFMove(*returnObject)) };
    }

    {
        auto variableDeclarations = backtrackingScope<Expected<AST::VariableDeclarationsStatement, Error>>([&]() -> Expected<AST::VariableDeclarationsStatement, Error> {
            auto result = parseVariableDeclarations();
            if (!result)
                return Unexpected<Error>(result.error());

            auto semicolon = consumeType(Lexer::Token::Type::Semicolon);
            if (!semicolon)
                return Unexpected<Error>(semicolon.error());

            return result;
        });
        if (variableDeclarations)
            return { makeUniqueRef<AST::VariableDeclarationsStatement>(WTFMove(*variableDeclarations)) };
    }

    auto effectfulExpression = backtrackingScope<Expected<UniqueRef<AST::Expression>, Error>>([&]() -> Expected<UniqueRef<AST::Expression>, Error> {
        auto result = parseEffectfulExpression();
        if (!result)
            return Unexpected<Error>(result.error());

        auto semicolon = consumeType(Lexer::Token::Type::Semicolon);
        if (!semicolon)
            return Unexpected<Error>(semicolon.error());

        return result;
    });
    if (effectfulExpression)
        return { makeUniqueRef<AST::EffectfulExpressionStatement>(WTFMove(*effectfulExpression)) };

    return Unexpected<Error>(effectfulExpression.error());
}

auto Parser::parseEffectfulExpression() -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto origin = peek();
    if (!origin)
        return Unexpected<Error>(origin.error());

    Vector<UniqueRef<AST::Expression>> expressions;

    auto first = backtrackingScope<Optional<UniqueRef<AST::Expression>>>([&]() -> Optional<UniqueRef<AST::Expression>> {
        auto effectfulExpression = parseEffectfulAssignment();
        if (!effectfulExpression)
            return WTF::nullopt;
        return { WTFMove(*effectfulExpression) };
    });
    if (!first)
        return { makeUniqueRef<AST::CommaExpression>(WTFMove(*origin), WTFMove(expressions)) };

    expressions.append(WTFMove(*first));

    while (tryType(Lexer::Token::Type::Comma)) {
        auto expression = parseEffectfulAssignment();
        if (!expression)
            return Unexpected<Error>(expression.error());
        expressions.append(WTFMove(*expression));
    }

    if (expressions.size() == 1)
        return WTFMove(expressions[0]);
    return { makeUniqueRef<AST::CommaExpression>(WTFMove(*origin), WTFMove(expressions)) };
}

auto Parser::parseEffectfulAssignment() -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto assignment = backtrackingScope<Expected<UniqueRef<AST::Expression>, Error>>([&]() {
        return parseAssignment();
    });
    if (assignment)
        return assignment;

    assignment = backtrackingScope<Expected<UniqueRef<AST::Expression>, Error>>([&]() {
        return parseEffectfulPrefix();
    });
    if (assignment)
        return assignment;

    assignment = backtrackingScope<Expected<UniqueRef<AST::Expression>, Error>>([&]() {
        return parseCallExpression();
    });
    if (assignment)
        return assignment;

    return Unexpected<Error>(assignment.error());
}

auto Parser::parseEffectfulPrefix() -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto prefix = consumeTypes({ Lexer::Token::Type::PlusPlus, Lexer::Token::Type::MinusMinus });
    if (!prefix)
        return Unexpected<Error>(prefix.error());

    auto previous = parsePossiblePrefix();
    if (!previous)
        return Unexpected<Error>(previous.error());

    switch (prefix->type) {
    case Lexer::Token::Type::PlusPlus: {
        auto result = AST::ReadModifyWriteExpression::create(Lexer::Token(*prefix), WTFMove(*previous));
        Vector<UniqueRef<AST::Expression>> callArguments;
        callArguments.append(result->oldVariableReference());
        result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(WTFMove(*prefix), "operator++"_str, WTFMove(callArguments)));
        result->setResultExpression(result->newVariableReference());
        return { WTFMove(result) };
    }
    default: {
        ASSERT(prefix->type == Lexer::Token::Type::MinusMinus);
        auto result = AST::ReadModifyWriteExpression::create(Lexer::Token(*prefix), WTFMove(*previous));
        Vector<UniqueRef<AST::Expression>> callArguments;
        callArguments.append(result->oldVariableReference());
        result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(WTFMove(*prefix), "operator--"_str, WTFMove(callArguments)));
        result->setResultExpression(result->newVariableReference());
        return { WTFMove(result) };
    }
    }
}

auto Parser::parseEffectfulSuffix() -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto effectfulSuffix = backtrackingScope<Expected<UniqueRef<AST::Expression>, Error>>([&]() -> Expected<UniqueRef<AST::Expression>, Error> {
        auto previous = parsePossibleSuffix();
        if (!previous)
            return Unexpected<Error>(previous.error());

        auto suffix = consumeTypes({ Lexer::Token::Type::PlusPlus, Lexer::Token::Type::MinusMinus });
        if (!suffix)
            return Unexpected<Error>(suffix.error());

        switch (suffix->type) {
        case Lexer::Token::Type::PlusPlus: {
            auto result = AST::ReadModifyWriteExpression::create(Lexer::Token(*suffix), WTFMove(*previous));
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(result->oldVariableReference());
            result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(WTFMove(*suffix), "operator++"_str, WTFMove(callArguments)));
            result->setResultExpression(result->oldVariableReference());
            return { WTFMove(result) };
        }
        default: {
            ASSERT(suffix->type == Lexer::Token::Type::MinusMinus);
            auto result = AST::ReadModifyWriteExpression::create(Lexer::Token(*suffix), WTFMove(*previous));
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(result->oldVariableReference());
            result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(WTFMove(*suffix), "operator--"_str, WTFMove(callArguments)));
            result->setResultExpression(result->oldVariableReference());
            return { WTFMove(result) };
        }
        }
    });
    if (effectfulSuffix)
        return effectfulSuffix;

    effectfulSuffix = backtrackingScope<Expected<UniqueRef<AST::Expression>, Error>>([&]() {
        return parseCallExpression();
    });
    if (effectfulSuffix)
        return effectfulSuffix;

    effectfulSuffix = backtrackingScope<Expected<UniqueRef<AST::Expression>, Error>>([&]() -> Expected<UniqueRef<AST::Expression>, Error> {
        auto leftParenthesis = consumeType(Lexer::Token::Type::LeftParenthesis);
        if (!leftParenthesis)
            return Unexpected<Error>(leftParenthesis.error());

        auto expression = parseExpression();
        if (!expression)
            return Unexpected<Error>(expression.error());

        auto rightParenthesis = consumeType(Lexer::Token::Type::RightParenthesis);
        if (!rightParenthesis)
            return Unexpected<Error>(rightParenthesis.error());

        return { WTFMove(*expression) };
    });
    if (effectfulSuffix)
        return effectfulSuffix;

    return Unexpected<Error>(effectfulSuffix.error());
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
    auto origin = peek();
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto first = parsePossibleTernaryConditional();
    if (!first)
        return Unexpected<Error>(first.error());

    Vector<UniqueRef<AST::Expression>> expressions;
    expressions.append(WTFMove(*first));

    while (tryType(Lexer::Token::Type::Comma)) {
        auto expression = parsePossibleTernaryConditional();
        if (!expression)
            return Unexpected<Error>(expression.error());
        expressions.append(WTFMove(*expression));
    }

    if (expressions.size() == 1)
        return WTFMove(expressions[0]);
    return { makeUniqueRef<AST::CommaExpression>(WTFMove(*origin), WTFMove(expressions)) };
}

auto Parser::parseTernaryConditional() -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto origin = peek();
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto predicate = parsePossibleLogicalBinaryOperation();
    if (!predicate)
        return Unexpected<Error>(predicate.error());

    auto questionMark = consumeType(Lexer::Token::Type::QuestionMark);
    if (!questionMark)
        return Unexpected<Error>(questionMark.error());

    auto bodyExpression = parseExpression();
    if (!bodyExpression)
        return Unexpected<Error>(bodyExpression.error());

    auto colon = consumeType(Lexer::Token::Type::Colon);
    if (!colon)
        return Unexpected<Error>(colon.error());

    auto elseExpression = parsePossibleTernaryConditional();
    if (!elseExpression)
        return Unexpected<Error>(elseExpression.error());

    Vector<UniqueRef<AST::Expression>> castArguments;
    castArguments.append(WTFMove(*predicate));
    auto boolCast = makeUniqueRef<AST::CallExpression>(Lexer::Token(*origin), "bool"_str, WTFMove(castArguments));
    return { makeUniqueRef<AST::TernaryExpression>(WTFMove(*origin), WTFMove(boolCast), WTFMove(*bodyExpression), WTFMove(*elseExpression)) };
}

auto Parser::parseAssignment() -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto origin = peek();
    if (!origin)
        return Unexpected<Error>(origin.error());

    auto left = parsePossiblePrefix();
    if (!left)
        return Unexpected<Error>(left.error());

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

    auto right = parsePossibleTernaryConditional();
    if (!right)
        return Unexpected<Error>(right.error());

    if (assignmentOperator->type == Lexer::Token::Type::EqualsSign)
        return { makeUniqueRef<AST::AssignmentExpression>(WTFMove(*origin), WTFMove(*left), WTFMove(*right))};

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

    auto result = AST::ReadModifyWriteExpression::create(Lexer::Token(*origin), WTFMove(*left));
    Vector<UniqueRef<AST::Expression>> callArguments;
    callArguments.append(result->oldVariableReference());
    callArguments.append(WTFMove(*right));
    result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(WTFMove(*origin), WTFMove(name), WTFMove(callArguments)));
    result->setResultExpression(result->newVariableReference());
    return { WTFMove(result) };
}

auto Parser::parsePossibleTernaryConditional() -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto ternaryExpression = backtrackingScope<Expected<UniqueRef<AST::Expression>, Error>>([&]() {
        return parseTernaryConditional();
    });
    if (ternaryExpression)
        return ternaryExpression;

    auto assignmentExpression = backtrackingScope<Expected<UniqueRef<AST::Expression>, Error>>([&]() {
        return parseAssignment();
    });
    if (assignmentExpression)
        return assignmentExpression;

    auto binaryOperation = backtrackingScope<Expected<UniqueRef<AST::Expression>, Error>>([&]() {
        return parsePossibleLogicalBinaryOperation();
    });
    if (binaryOperation)
        return binaryOperation;

    return Unexpected<Error>(binaryOperation.error());
}

auto Parser::parsePossibleLogicalBinaryOperation() -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto parsedPrevious = parsePossibleRelationalBinaryOperation();
    if (!parsedPrevious)
        return Unexpected<Error>(parsedPrevious.error());
    UniqueRef<AST::Expression> previous = WTFMove(*parsedPrevious);

    while (auto logicalBinaryOperation = tryTypes({
        Lexer::Token::Type::OrOr,
        Lexer::Token::Type::AndAnd,
        Lexer::Token::Type::Or,
        Lexer::Token::Type::Xor,
        Lexer::Token::Type::And
        })) {
        auto next = parsePossibleRelationalBinaryOperation();
        if (!next)
            return Unexpected<Error>(next.error());

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
    auto parsedPrevious = parsePossibleShift();
    if (!parsedPrevious)
        return Unexpected<Error>(parsedPrevious.error());
    UniqueRef<AST::Expression> previous = WTFMove(*parsedPrevious);

    while (auto relationalBinaryOperation = tryTypes({
        Lexer::Token::Type::LessThanSign,
        Lexer::Token::Type::GreaterThanSign,
        Lexer::Token::Type::LessThanOrEqualTo,
        Lexer::Token::Type::GreaterThanOrEqualTo,
        Lexer::Token::Type::EqualComparison,
        Lexer::Token::Type::NotEqual
        })) {
        auto next = parsePossibleShift();
        if (!next)
            return Unexpected<Error>(next.error());

        switch (relationalBinaryOperation->type) {
        case Lexer::Token::Type::LessThanSign: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*relationalBinaryOperation), "operator<"_str, WTFMove(callArguments));
            break;
        }
        case Lexer::Token::Type::GreaterThanSign: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*relationalBinaryOperation), "operator>"_str, WTFMove(callArguments));
            break;
        }
        case Lexer::Token::Type::LessThanOrEqualTo: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*relationalBinaryOperation), "operator<="_str, WTFMove(callArguments));
            break;
        }
        case Lexer::Token::Type::GreaterThanOrEqualTo: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*relationalBinaryOperation), "operator>="_str, WTFMove(callArguments));
            break;
        }
        case Lexer::Token::Type::EqualComparison: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*relationalBinaryOperation), "operator=="_str, WTFMove(callArguments));
            break;
        }
        default: {
            ASSERT(relationalBinaryOperation->type == Lexer::Token::Type::NotEqual);
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
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
    auto parsedPrevious = parsePossibleAdd();
    if (!parsedPrevious)
        return Unexpected<Error>(parsedPrevious.error());
    UniqueRef<AST::Expression> previous = WTFMove(*parsedPrevious);

    while (auto shift = tryTypes({
        Lexer::Token::Type::LeftShift,
        Lexer::Token::Type::RightShift
        })) {
        auto next = parsePossibleAdd();
        if (!next)
            return Unexpected<Error>(next.error());

        switch (shift->type) {
        case Lexer::Token::Type::LeftShift: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*shift), "operator<<"_str, WTFMove(callArguments));
            break;
        }
        default: {
            ASSERT(shift->type == Lexer::Token::Type::RightShift);
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*shift), "operator>>"_str, WTFMove(callArguments));
            break;
        }
        }
    }

    return WTFMove(previous);
}

auto Parser::parsePossibleAdd() -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto parsedPrevious = parsePossibleMultiply();
    if (!parsedPrevious)
        return Unexpected<Error>(parsedPrevious.error());
    UniqueRef<AST::Expression> previous = WTFMove(*parsedPrevious);

    while (auto add = tryTypes({
        Lexer::Token::Type::Plus,
        Lexer::Token::Type::Minus
        })) {
        auto next = parsePossibleMultiply();
        if (!next)
            return Unexpected<Error>(next.error());

        switch (add->type) {
        case Lexer::Token::Type::Plus: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*add), "operator+"_str, WTFMove(callArguments));
            break;
        }
        default: {
            ASSERT(add->type == Lexer::Token::Type::Minus);
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*add), "operator-"_str, WTFMove(callArguments));
            break;
        }
        }
    }

    return WTFMove(previous);
}

auto Parser::parsePossibleMultiply() -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto parsedPrevious = parsePossiblePrefix();
    if (!parsedPrevious)
        return Unexpected<Error>(parsedPrevious.error());
    UniqueRef<AST::Expression> previous = WTFMove(*parsedPrevious);

    while (auto multiply = tryTypes({
        Lexer::Token::Type::Star,
        Lexer::Token::Type::Divide,
        Lexer::Token::Type::Mod
        })) {
        auto next = parsePossiblePrefix();
        if (!next)
            return Unexpected<Error>(next.error());

        switch (multiply->type) {
        case Lexer::Token::Type::Star: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*multiply), "operator*"_str, WTFMove(callArguments));
            break;
        }
        case Lexer::Token::Type::Divide: {
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*multiply), "operator/"_str, WTFMove(callArguments));
            break;
        }
        default: {
            ASSERT(multiply->type == Lexer::Token::Type::Mod);
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(WTFMove(previous));
            callArguments.append(WTFMove(*next));
            previous = makeUniqueRef<AST::CallExpression>(WTFMove(*multiply), "operator%"_str, WTFMove(callArguments));
            break;
        }
        }
    }

    return WTFMove(previous);
}

auto Parser::parsePossiblePrefix() -> Expected<UniqueRef<AST::Expression>, Error>
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
        auto next = parsePossiblePrefix();
        if (!next)
            return Unexpected<Error>(next.error());

        switch (prefix->type) {
        case Lexer::Token::Type::PlusPlus: {
            auto result = AST::ReadModifyWriteExpression::create(Lexer::Token(*prefix), WTFMove(*next));
            Vector<UniqueRef<AST::Expression>> callArguments;
            callArguments.append(result->oldVariableReference());
            result->setNewValueExpression(makeUniqueRef<AST::CallExpression>(Lexer::Token(*prefix), "operator++"_str, WTFMove(callArguments)));
            result->setResultExpression(result->newVariableReference());
            return { WTFMove(result) };
        }
        case Lexer::Token::Type::MinusMinus: {
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

    return parsePossibleSuffix();
}

auto Parser::parsePossibleSuffix() -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto suffix = backtrackingScope<Expected<UniqueRef<AST::Expression>, Error>>([&]() -> Expected<UniqueRef<AST::Expression>, Error> {
        auto expression = parseCallExpression();
        if (!expression)
            return Unexpected<Error>(expression.error());

        while (true) {
            auto result = backtrackingScope<SuffixExpression>([&]() -> SuffixExpression {
                return parseLimitedSuffixOperator(WTFMove(*expression));
            });
            expression = WTFMove(result.result);
            if (!result)
                break;
        }
        return expression;
    });
    if (suffix)
        return suffix;

    suffix = backtrackingScope<Expected<UniqueRef<AST::Expression>, Error>>([&]() -> Expected<UniqueRef<AST::Expression>, Error> {
        auto expression = parseTerm();
        if (!expression)
            return Unexpected<Error>(expression.error());

        while (true) {
            auto result = backtrackingScope<SuffixExpression>([&]() -> SuffixExpression {
                return parseSuffixOperator(WTFMove(*expression));
            });
            expression = WTFMove(result.result);
            if (!result)
                break;
        }
        return expression;
    });
    if (suffix)
        return suffix;

    return Unexpected<Error>(suffix.error());
}

auto Parser::parseCallExpression() -> Expected<UniqueRef<AST::Expression>, Error>
{
    auto name = consumeType(Lexer::Token::Type::Identifier);
    if (!name)
        return Unexpected<Error>(name.error());
    auto callName = name->stringView.toString();

    auto leftParenthesis = consumeType(Lexer::Token::Type::LeftParenthesis);
    if (!leftParenthesis)
        return Unexpected<Error>(leftParenthesis.error());

    Vector<UniqueRef<AST::Expression>> arguments;
    if (tryType(Lexer::Token::Type::RightParenthesis))
        return { makeUniqueRef<AST::CallExpression>(WTFMove(*name), WTFMove(callName), WTFMove(arguments)) };

    auto firstArgument = parsePossibleTernaryConditional();
    if (!firstArgument)
        return Unexpected<Error>(firstArgument.error());
    arguments.append(WTFMove(*firstArgument));
    while (tryType(Lexer::Token::Type::Comma)) {
        auto argument = parsePossibleTernaryConditional();
        if (!argument)
            return Unexpected<Error>(argument.error());
        arguments.append(WTFMove(*argument));
    }

    auto rightParenthesis = consumeType(Lexer::Token::Type::RightParenthesis);
    if (!rightParenthesis)
        return Unexpected<Error>(rightParenthesis.error());

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
        auto expression = parseExpression();
        if (!expression)
            return Unexpected<Error>(expression.error());

        auto rightParenthesis = consumeType(Lexer::Token::Type::RightParenthesis);
        if (!rightParenthesis)
            return Unexpected<Error>(rightParenthesis.error());

        return { WTFMove(*expression) };
    }
    }
}

}

}

#endif
