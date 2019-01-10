/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

load("AddressSpace.js");
load("AnonymousVariable.js");
load("ArrayRefType.js");
load("ArrayType.js");
load("Assignment.js");
load("AutoWrapper.js");
load("Block.js");
load("BoolLiteral.js");
load("Break.js");
load("CallExpression.js");
load("CallFunction.js");
load("Check.js");
load("CheckLiteralTypes.js");
load("CheckLoops.js");
load("CheckRecursiveTypes.js");
load("CheckRecursion.js");
load("CheckReturns.js");
load("CheckUnreachableCode.js");
load("CheckWrapped.js");
load("Checker.js");
load("CloneProgram.js");
load("CommaExpression.js");
load("ConstexprFolder.js");
load("ConstexprTypeParameter.js");
load("Continue.js");
load("ConvertPtrToArrayRefExpression.js");
load("DereferenceExpression.js");
load("DoWhileLoop.js");
load("DotExpression.js");
load("DoubleLiteral.js");
load("DoubleLiteralType.js");
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
load("FlattenProtocolExtends.js");
load("FlattenedStructOffsetGatherer.js");
load("FloatLiteral.js");
load("FloatLiteralType.js");
load("FoldConstexprs.js");
load("ForLoop.js");
load("Func.js");
load("FuncDef.js");
load("FuncInstantiator.js");
load("FuncParameter.js");
load("FunctionLikeBlock.js");
load("HighZombieFinder.js");
load("IdentityExpression.js");
load("IfStatement.js");
load("IndexExpression.js");
load("InferTypesForCall.js");
load("Inline.js");
load("Inliner.js");
load("InstantiateImmediates.js");
load("IntLiteral.js");
load("IntLiteralType.js");
load("Intrinsics.js");
load("LateChecker.js");
load("Lexer.js");
load("LexerToken.js");
load("LiteralTypeChecker.js");
load("LogicalExpression.js");
load("LogicalNot.js");
load("LoopChecker.js");
load("MakeArrayRefExpression.js");
load("MakePtrExpression.js");
load("NameContext.js");
load("NameFinder.js");
load("NameResolver.js");
load("NativeFunc.js");
load("NativeFuncInstance.js");
load("NativeType.js");
load("NativeTypeInstance.js");
load("NormalUsePropertyResolver.js");
load("NullLiteral.js");
load("NullType.js");
load("OriginKind.js");
load("OverloadResolutionFailure.js");
load("Parse.js");
load("Prepare.js");
load("Program.js");
load("ProgramWithUnnecessaryThingsRemoved.js");
load("PropertyResolver.js");
load("Protocol.js");
load("ProtocolDecl.js");
load("ProtocolFuncDecl.js");
load("ProtocolRef.js");
load("PtrType.js");
load("ReadModifyWriteExpression.js");
load("RecursionChecker.js");
load("RecursiveTypeChecker.js");
load("ResolveNames.js");
load("ResolveOverloadImpl.js");
load("ResolveProperties.js");
load("ResolveTypeDefs.js");
load("Return.js");
load("ReturnChecker.js");
load("ReturnException.js");
load("StandardLibrary.js");
load("StatementCloner.js");
load("StructLayoutBuilder.js");
load("StructType.js");
load("Substitution.js");
load("SwitchCase.js");
load("SwitchStatement.js");
load("SynthesizeEnumFunctions.js");
load("SynthesizeStructAccessors.js");
load("TrapStatement.js");
load("TypeDef.js");
load("TypeDefResolver.js");
load("TypeOrVariableRef.js");
load("TypeParameterRewriter.js");
load("TypeRef.js");
load("TypeVariable.js");
load("TypeVariableTracker.js");
load("TypedValue.js");
load("UintLiteral.js");
load("UintLiteralType.js");
load("UnificationContext.js");
load("UnreachableCodeChecker.js");
load("VariableDecl.js");
load("VariableRef.js");
load("VisitingSet.js");
load("WSyntaxError.js");
load("WTrapError.js");
load("WTypeError.js");
load("WhileLoop.js");
load("WrapChecker.js");

