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

};

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
