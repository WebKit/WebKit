/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
"use strict";

load("Node.js");
load("Type.js");
load("ReferenceType.js");
load("Value.js");
load("Expression.js");
load("Rewriter.js");
load("Visitor.js");
load("CreateLiteral.js");
load("CreateLiteralType.js");
load("PropertyAccessExpression.js");
load("NativeType.js");
load("Semantic.js");

load("AddressSpace.js");
load("AllocateAtEntryPoints.js");
load("AnonymousVariable.js");
load("ArrayRefType.js");
load("ArrayType.js");
load("Assignment.js");
load("AutoWrapper.js");
load("Block.js");
load("BoolLiteral.js");
load("Break.js");
load("BuiltInSemantic.js");
load("BuiltinMatrixGetter.js");
load("BuiltinMatrixSetter.js");
load("BuiltinVectorGetter.js");
load("BuiltinVectorSetter.js");
load("CallExpression.js");
load("CallFunction.js");
load("Casts.js");
load("Check.js");
load("CheckLiteralTypes.js");
load("CheckLoops.js");
load("CheckNativeFuncStages.js");
load("CheckRecursion.js");
load("CheckRecursiveTypes.js");
load("CheckReturns.js");
load("CheckTypesWithArguments.js");
load("CheckUnreachableCode.js");
load("CheckWrapped.js");
load("Checker.js");
load("CloneProgram.js");
load("CommaExpression.js");
load("ConstexprFolder.js");
load("Continue.js");
load("ConvertPtrToArrayRefExpression.js");
load("DoWhileLoop.js");
load("DotExpression.js");
load("DereferenceExpression.js");
load("EArrayRef.js");
load("EBuffer.js");
load("EBufferBuilder.js");
load("EPtr.js");
load("EnumLiteral.js");
load("EnumMember.js");
load("EnumType.js");
load("EvaluationCommon.js");
load("Evaluator.js");
load("ExpressionFinder.js");
load("ExternalOrigin.js");
load("Field.js");
load("FindHighZombies.js");
load("FlattenedStructOffsetGatherer.js");
load("FloatLiteral.js");
load("FloatLiteralType.js");
load("FoldConstexprs.js");
load("ForLoop.js");
load("Func.js");
load("FuncAttribute.js");
load("FuncDef.js");
load("FuncNumThreadsAttribute.js");
load("FuncParameter.js");
load("FunctionLikeBlock.js");
load("HighZombieFinder.js");
load("IdentityExpression.js");
load("IfStatement.js");
load("IndexExpression.js");
load("InferTypesForCall.js");
load("Inline.js");
load("Inliner.js");
load("IntLiteral.js");
load("IntLiteralType.js");
load("Intrinsics.js");
load("LayoutBuffers.js");
load("Lexer.js");
load("LexerToken.js");
load("LiteralTypeChecker.js");
load("LogicalExpression.js");
load("LogicalNot.js");
load("LoopChecker.js");
load("MakeArrayRefExpression.js");
load("MakePtrExpression.js");
load("MatrixType.js");
load("NameContext.js");
load("NameFinder.js");
load("NameResolver.js");
load("NativeFunc.js");
load("NormalUsePropertyResolver.js");
load("NullLiteral.js");
load("NullType.js");
load("OperatorAnderIndexer.js");
load("OperatorArrayRefLength.js");
load("OriginKind.js");
load("OverloadResolutionFailure.js");
load("Parse.js");
load("Prepare.js");
load("PropertyResolver.js");
load("Program.js");
load("ProgramWithUnnecessaryThingsRemoved.js");
load("PtrType.js");
load("ReadModifyWriteExpression.js");
load("RecursionChecker.js");
load("RecursiveTypeChecker.js");
load("ResolveNames.js");
load("ResolveOverloadImpl.js");
load("ResolveProperties.js");
load("ResolveTypeDefs.js");
load("ResourceSemantic.js");
load("Return.js");
load("ReturnChecker.js");
load("ReturnException.js");
load("Sampler.js");
load("SpecializationConstantSemantic.js");
load("StageInOutSemantic.js");
load("StandardLibrary.js");
load("StatementCloner.js");
load("StructLayoutBuilder.js");
load("StructType.js");
load("SwitchCase.js");
load("SwitchStatement.js");
load("SynthesizeArrayOperatorLength.js");
load("SynthesizeEnumFunctions.js");
load("SynthesizeStructAccessors.js");
load("SynthesizeCopyConstructorOperator.js");
load("SynthesizeDefaultConstructorOperator.js");
load("TernaryExpression.js");
load("Texture.js");
load("TextureOperations.js");
load("TrapStatement.js");
load("TypeDef.js");
load("TypeDefResolver.js");
load("TypeRef.js");
load("TypeOverloadResolutionFailure.js");
load("TypedValue.js");
load("UintLiteral.js");
load("UintLiteralType.js");
load("UnificationContext.js");
load("UnreachableCodeChecker.js");
load("VariableDecl.js");
load("VariableRef.js");
load("VectorType.js");
load("VisitingSet.js");
load("WLexicalError.js");
load("WSyntaxError.js");
load("WTrapError.js");
load("WTypeError.js");
load("WhileLoop.js");
load("WrapChecker.js");
