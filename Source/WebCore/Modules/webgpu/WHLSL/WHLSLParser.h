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

#pragma once

#if ENABLE(WEBGPU)

#include "WHLSLAST.h"
#include "WHLSLError.h"
#include "WHLSLLexer.h"
#include "WHLSLParsingMode.h"
#include <wtf/Expected.h>
#include <wtf/Optional.h>
#include <wtf/PrintStream.h>

namespace WebCore {

namespace WHLSL {

class Program;

class Parser {
public:
    Expected<void, Error> parse(Program&, StringView, ParsingMode, AST::NameSpace);

private:
    // FIXME: We should not need this
    // https://bugs.webkit.org/show_bug.cgi?id=198357
    template<typename T> T backtrackingScope(std::function<T()> callback)
    {
        auto state = m_lexer.state();
        auto result = callback();
        if (result)
            return result;
        m_lexer.setState(WTFMove(state));
        return result;
    }

    enum class TryToPeek {
        Yes,
        No
    };
    Unexpected<Error> fail(const String& message, TryToPeek = TryToPeek::Yes);
    Expected<Token, Error> peek();
    Expected<Token, Error> peekFurther();
    bool peekType(Token::Type);
    template <Token::Type... types>
    bool peekTypes();
    Optional<Token> tryType(Token::Type);
    template <Token::Type... types>
    Optional<Token> tryTypes();
    Expected<Token, Error> consumeType(Token::Type);
    template <Token::Type... types>
    Expected<Token, Error> consumeTypes();

    Expected<Variant<int, unsigned>, Error> consumeIntegralLiteral();
    Expected<unsigned, Error> consumeNonNegativeIntegralLiteral();
    Expected<AST::ConstantExpression, Error> parseConstantExpression();
    Expected<AST::TypeArgument, Error> parseTypeArgument();
    Expected<AST::TypeArguments, Error> parseTypeArguments();
    struct TypeSuffixAbbreviated {
        CodeLocation location;
        Token token;
        Optional<unsigned> numElements;
    };
    Expected<TypeSuffixAbbreviated, Error> parseTypeSuffixAbbreviated();
    struct TypeSuffixNonAbbreviated {
        CodeLocation location;
        Token token;
        Optional<AST::AddressSpace> addressSpace;
        Optional<unsigned> numElements;
    };
    Expected<TypeSuffixNonAbbreviated, Error> parseTypeSuffixNonAbbreviated();
    Expected<Ref<AST::UnnamedType>, Error> parseAddressSpaceType();
    Expected<Ref<AST::UnnamedType>, Error> parseNonAddressSpaceType();
    Expected<Ref<AST::UnnamedType>, Error> parseType();
    Expected<AST::TypeDefinition, Error> parseTypeDefinition();
    Expected<AST::BuiltInSemantic, Error> parseBuiltInSemantic();
    Expected<AST::ResourceSemantic, Error> parseResourceSemantic();
    Expected<AST::SpecializationConstantSemantic, Error> parseSpecializationConstantSemantic();
    Expected<AST::StageInOutSemantic, Error> parseStageInOutSemantic();
    Expected<std::unique_ptr<AST::Semantic>, Error> parseSemantic();
    AST::Qualifiers parseQualifiers();
    Expected<AST::StructureElement, Error> parseStructureElement();
    Expected<AST::StructureDefinition, Error> parseStructureDefinition();
    Expected<AST::EnumerationDefinition, Error> parseEnumerationDefinition();
    Expected<AST::EnumerationMember, Error> parseEnumerationMember(int64_t defaultValue);
    Expected<AST::NativeTypeDeclaration, Error> parseNativeTypeDeclaration();
    Expected<AST::NumThreadsFunctionAttribute, Error> parseNumThreadsFunctionAttribute();
    Expected<AST::AttributeBlock, Error> parseAttributeBlock();
    Expected<AST::VariableDeclaration, Error> parseParameter();
    Expected<AST::VariableDeclarations, Error> parseParameters();
    Expected<AST::FunctionDeclaration, Error> parseComputeFunctionDeclaration();
    Expected<AST::FunctionDeclaration, Error> parseVertexOrFragmentFunctionDeclaration();
    Expected<AST::FunctionDeclaration, Error> parseRegularFunctionDeclaration();
    Expected<AST::FunctionDeclaration, Error> parseOperatorFunctionDeclaration();
    Expected<AST::FunctionDeclaration, Error> parseFunctionDeclaration();
    Expected<AST::FunctionDefinition, Error> parseFunctionDefinition();
    Expected<AST::NativeFunctionDeclaration, Error> parseNativeFunctionDeclaration();

    Expected<AST::Block, Error> parseBlock();
    Expected<AST::Block, Error> parseBlockBody();
    Expected<AST::IfStatement, Error> parseIfStatement();
    Expected<AST::SwitchStatement, Error> parseSwitchStatement();
    Expected<AST::SwitchCase, Error> parseSwitchCase();
    Expected<AST::ForLoop, Error> parseForLoop();
    Expected<AST::WhileLoop, Error> parseWhileLoop();
    Expected<AST::DoWhileLoop, Error> parseDoWhileLoop();
    Expected<AST::VariableDeclaration, Error> parseVariableDeclaration(Ref<AST::UnnamedType>&&);
    Expected<AST::VariableDeclarationsStatement, Error> parseVariableDeclarations();
    Expected<UniqueRef<AST::Statement>, Error> parseStatement();

    Expected<UniqueRef<AST::Statement>, Error> parseEffectfulExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseEffectfulAssignment();
    struct SuffixExpression {
        SuffixExpression(UniqueRef<AST::Expression>&& result, bool success)
            : result(WTFMove(result))
            , success(success)
        {
        }

        UniqueRef<AST::Expression> result;
        bool success;
        operator bool() const { return success; }
    };
    SuffixExpression parseLimitedSuffixOperator(UniqueRef<AST::Expression>&&);
    SuffixExpression parseSuffixOperator(UniqueRef<AST::Expression>&&);

    Expected<UniqueRef<AST::Expression>, Error> parseExpression();
    Expected<UniqueRef<AST::Expression>, Error> parsePossibleTernaryConditional();
    Expected<UniqueRef<AST::Expression>, Error> completeTernaryConditional(UniqueRef<AST::Expression>&& predicate);
    Expected<UniqueRef<AST::Expression>, Error> completeAssignment(UniqueRef<AST::Expression>&& left);
    Expected<UniqueRef<AST::Expression>, Error> parsePossibleLogicalBinaryOperation();
    Expected<UniqueRef<AST::Expression>, Error> completePossibleLogicalBinaryOperation(UniqueRef<AST::Expression>&& previous);
    Expected<UniqueRef<AST::Expression>, Error> parsePossibleRelationalBinaryOperation();
    Expected<UniqueRef<AST::Expression>, Error> completePossibleRelationalBinaryOperation(UniqueRef<AST::Expression>&& previous);
    Expected<UniqueRef<AST::Expression>, Error> parsePossibleShift();
    Expected<UniqueRef<AST::Expression>, Error> completePossibleShift(UniqueRef<AST::Expression>&& previous);
    Expected<UniqueRef<AST::Expression>, Error> parsePossibleAdd();
    Expected<UniqueRef<AST::Expression>, Error> completePossibleAdd(UniqueRef<AST::Expression>&& previous);
    Expected<UniqueRef<AST::Expression>, Error> parsePossibleMultiply();
    Expected<UniqueRef<AST::Expression>, Error> completePossibleMultiply(UniqueRef<AST::Expression>&& previous);
    Expected<UniqueRef<AST::Expression>, Error> parsePossiblePrefix(bool *isEffectful = nullptr);
    Expected<UniqueRef<AST::Expression>, Error> parsePossibleSuffix(bool *isEffectful = nullptr);
    Expected<UniqueRef<AST::Expression>, Error> parseCallExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseTerm();

    Lexer m_lexer;
    ParsingMode m_mode;
};

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
