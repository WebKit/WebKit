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

#include "WHLSLArrayReferenceType.h"
#include "WHLSLArrayType.h"
#include "WHLSLAssignmentExpression.h"
#include "WHLSLBaseFunctionAttribute.h"
#include "WHLSLBaseSemantic.h"
#include "WHLSLBlock.h"
#include "WHLSLBooleanLiteral.h"
#include "WHLSLBreak.h"
#include "WHLSLBuiltInSemantic.h"
#include "WHLSLCallExpression.h"
#include "WHLSLCommaExpression.h"
#include "WHLSLConstantExpression.h"
#include "WHLSLContinue.h"
#include "WHLSLDereferenceExpression.h"
#include "WHLSLDoWhileLoop.h"
#include "WHLSLDotExpression.h"
#include "WHLSLEffectfulExpressionStatement.h"
#include "WHLSLEnumerationDefinition.h"
#include "WHLSLEnumerationMember.h"
#include "WHLSLExpression.h"
#include "WHLSLFallthrough.h"
#include "WHLSLFloatLiteral.h"
#include "WHLSLForLoop.h"
#include "WHLSLFunctionAttribute.h"
#include "WHLSLFunctionDeclaration.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLIfStatement.h"
#include "WHLSLIndexExpression.h"
#include "WHLSLIntegerLiteral.h"
#include "WHLSLLexer.h"
#include "WHLSLLogicalExpression.h"
#include "WHLSLLogicalNotExpression.h"
#include "WHLSLMakeArrayReferenceExpression.h"
#include "WHLSLMakePointerExpression.h"
#include "WHLSLNativeFunctionDeclaration.h"
#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLNode.h"
#include "WHLSLNullLiteral.h"
#include "WHLSLNumThreadsFunctionAttribute.h"
#include "WHLSLPointerType.h"
#include "WHLSLProgram.h"
#include "WHLSLPropertyAccessExpression.h"
#include "WHLSLQualifier.h"
#include "WHLSLReadModifyWriteExpression.h"
#include "WHLSLReferenceType.h"
#include "WHLSLResourceSemantic.h"
#include "WHLSLReturn.h"
#include "WHLSLSemantic.h"
#include "WHLSLSpecializationConstantSemantic.h"
#include "WHLSLStageInOutSemantic.h"
#include "WHLSLStatement.h"
#include "WHLSLStructureDefinition.h"
#include "WHLSLStructureElement.h"
#include "WHLSLSwitchCase.h"
#include "WHLSLSwitchStatement.h"
#include "WHLSLTernaryExpression.h"
#include "WHLSLTrap.h"
#include "WHLSLType.h"
#include "WHLSLTypeArgument.h"
#include "WHLSLTypeDefinition.h"
#include "WHLSLTypeReference.h"
#include "WHLSLUnsignedIntegerLiteral.h"
#include "WHLSLValue.h"
#include "WHLSLVariableDeclaration.h"
#include "WHLSLVariableDeclarationsStatement.h"
#include "WHLSLVariableReference.h"
#include "WHLSLWhileLoop.h"
#include <wtf/Expected.h>
#include <wtf/Optional.h>

namespace WebCore {

namespace WHLSL {

class Parser {
public:
    enum class Mode {
        StandardLibrary,
        User
    };

    struct Error {
        Error(String&& error)
            : error(WTFMove(error))
        {
        }

        String error;
    };

    Optional<Error> parse(Program&, StringView, Mode);

private:
    template<typename T> T backtrackingScope(std::function<T()> callback)
    {
        auto state = m_lexer.state();
        auto result = callback();
        if (result)
            return result;
        m_lexer.setState(WTFMove(state));
        return result;
    }

    Unexpected<Error> fail(const String& message);
    Expected<Lexer::Token, Error> peek();
    Optional<Lexer::Token> tryType(Lexer::Token::Type);
    Optional<Lexer::Token> tryTypes(Vector<Lexer::Token::Type>);
    Expected<Lexer::Token, Error> consumeType(Lexer::Token::Type);
    Expected<Lexer::Token, Error> consumeTypes(Vector<Lexer::Token::Type>);

    Expected<Variant<int, unsigned>, Error> consumeIntegralLiteral();
    Expected<unsigned, Error> consumeNonNegativeIntegralLiteral();
    Expected<AST::ConstantExpression, Error> parseConstantExpression();
    Expected<AST::TypeArgument, Error> parseTypeArgument();
    Expected<AST::TypeArguments, Error> parseTypeArguments();
    struct TypeSuffixAbbreviated {
        Lexer::Token token;
        Optional<unsigned> numElements;
    };
    Expected<TypeSuffixAbbreviated, Error> parseTypeSuffixAbbreviated();
    struct TypeSuffixNonAbbreviated {
        Lexer::Token token;
        Optional<AST::AddressSpace> addressSpace;
        Optional<unsigned> numElements;
    };
    Expected<TypeSuffixNonAbbreviated, Error> parseTypeSuffixNonAbbreviated();
    Expected<UniqueRef<AST::UnnamedType>, Error> parseAddressSpaceType();
    Expected<UniqueRef<AST::UnnamedType>, Error> parseNonAddressSpaceType();
    Expected<UniqueRef<AST::UnnamedType>, Error> parseType();
    Expected<AST::TypeDefinition, Error> parseTypeDefinition();
    Expected<AST::BuiltInSemantic, Error> parseBuiltInSemantic();
    Expected<AST::ResourceSemantic, Error> parseResourceSemantic();
    Expected<AST::SpecializationConstantSemantic, Error> parseSpecializationConstantSemantic();
    Expected<AST::StageInOutSemantic, Error> parseStageInOutSemantic();
    Expected<AST::Semantic, Error> parseSemantic();
    AST::Qualifiers parseQualifiers();
    Expected<AST::StructureElement, Error> parseStructureElement();
    Expected<AST::StructureDefinition, Error> parseStructureDefinition();
    Expected<AST::EnumerationDefinition, Error> parseEnumerationDefinition();
    Expected<AST::EnumerationMember, Error> parseEnumerationMember();
    Expected<AST::NativeTypeDeclaration, Error> parseNativeTypeDeclaration();
    Expected<AST::NumThreadsFunctionAttribute, Error> parseNumThreadsFunctionAttribute();
    Expected<AST::AttributeBlock, Error> parseAttributeBlock();
    Expected<AST::VariableDeclaration, Error> parseParameter();
    Expected<AST::VariableDeclarations, Error> parseParameters();
    Expected<AST::FunctionDeclaration, Error> parseEntryPointFunctionDeclaration();
    Expected<AST::FunctionDeclaration, Error> parseRegularFunctionDeclaration();
    Expected<AST::FunctionDeclaration, Error> parseOperatorFunctionDeclaration();
    Expected<AST::FunctionDeclaration, Error> parseFunctionDeclaration();
    Expected<AST::FunctionDefinition, Error> parseFunctionDefinition();
    Expected<AST::NativeFunctionDeclaration, Error> parseNativeFunctionDeclaration();

    Expected<AST::Block, Error> parseBlock();
    AST::Block parseBlockBody(Lexer::Token&& origin);
    Expected<AST::IfStatement, Error> parseIfStatement();
    Expected<AST::SwitchStatement, Error> parseSwitchStatement();
    Expected<AST::SwitchCase, Error> parseSwitchCase();
    Expected<AST::ForLoop, Error> parseForLoop();
    Expected<AST::WhileLoop, Error> parseWhileLoop();
    Expected<AST::DoWhileLoop, Error> parseDoWhileLoop();
    Expected<AST::VariableDeclaration, Error> parseVariableDeclaration(UniqueRef<AST::UnnamedType>&&);
    Expected<AST::VariableDeclarationsStatement, Error> parseVariableDeclarations();
    Expected<UniqueRef<AST::Statement>, Error> parseStatement();

    Expected<UniqueRef<AST::Expression>, Error> parseEffectfulExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseEffectfulAssignment();
    Expected<UniqueRef<AST::Expression>, Error> parseEffectfulPrefix();
    Expected<UniqueRef<AST::Expression>, Error> parseEffectfulSuffix();
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
    Expected<UniqueRef<AST::Expression>, Error> parseTernaryConditional();
    Expected<UniqueRef<AST::Expression>, Error> parseAssignment();
    Expected<UniqueRef<AST::Expression>, Error> parsePossibleTernaryConditional();
    Expected<UniqueRef<AST::Expression>, Error> parsePossibleLogicalBinaryOperation();
    Expected<UniqueRef<AST::Expression>, Error> parsePossibleRelationalBinaryOperation();
    Expected<UniqueRef<AST::Expression>, Error> parsePossibleShift();
    Expected<UniqueRef<AST::Expression>, Error> parsePossibleAdd();
    Expected<UniqueRef<AST::Expression>, Error> parsePossibleMultiply();
    Expected<UniqueRef<AST::Expression>, Error> parsePossiblePrefix();
    Expected<UniqueRef<AST::Expression>, Error> parsePossibleSuffix();
    Expected<UniqueRef<AST::Expression>, Error> parseCallExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseTerm();

    Lexer m_lexer;
    Mode m_mode;
};

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
