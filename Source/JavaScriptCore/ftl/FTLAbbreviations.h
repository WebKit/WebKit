/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef FTLAbbreviations_h
#define FTLAbbreviations_h

#if ENABLE(FTL_JIT)

#include "FTLAbbreviatedTypes.h"
#include "FTLValueFromBlock.h"
#include "LLVMAPI.h"
#include <cstring>

namespace JSC { namespace FTL {

// This file contains short-form calls into the LLVM C API. It is meant to
// save typing and make the lowering code clearer. If we ever call an LLVM C API
// function more than once in the FTL lowering code, we should add a shortcut for
// it here.

#if USE(JSVALUE32_64)
#error "The FTL backend assumes that pointers are 64-bit."
#endif

static inline LType voidType(LContext context) { return llvm->VoidTypeInContext(context); }
static inline LType int1Type(LContext context) { return llvm->Int1TypeInContext(context); }
static inline LType int8Type(LContext context) { return llvm->Int8TypeInContext(context); }
static inline LType int16Type(LContext context) { return llvm->Int16TypeInContext(context); }
static inline LType int32Type(LContext context) { return llvm->Int32TypeInContext(context); }
static inline LType int64Type(LContext context) { return llvm->Int64TypeInContext(context); }
static inline LType intPtrType(LContext context) { return llvm->Int64TypeInContext(context); }
static inline LType floatType(LContext context) { return llvm->FloatTypeInContext(context); }
static inline LType doubleType(LContext context) { return llvm->DoubleTypeInContext(context); }

static inline LType pointerType(LType type) { return llvm->PointerType(type, 0); }
static inline LType arrayType(LType type, unsigned count) { return llvm->ArrayType(type, count); }
static inline LType vectorType(LType type, unsigned count) { return llvm->VectorType(type, count); }

enum PackingMode { NotPacked, Packed };
static inline LType structType(LContext context, LType* elementTypes, unsigned elementCount, PackingMode packing = NotPacked)
{
    return llvm->StructTypeInContext(context, elementTypes, elementCount, packing == Packed);
}
static inline LType structType(LContext context, PackingMode packing = NotPacked)
{
    return structType(context, 0, 0, packing);
}
static inline LType structType(LContext context, LType element1, PackingMode packing = NotPacked)
{
    return structType(context, &element1, 1, packing);
}
static inline LType structType(LContext context, LType element1, LType element2, PackingMode packing = NotPacked)
{
    LType elements[] = { element1, element2 };
    return structType(context, elements, 2, packing);
}

enum Variadicity { NotVariadic, Variadic };
static inline LType functionType(LType returnType, const LType* paramTypes, unsigned paramCount, Variadicity variadicity)
{
    return llvm->FunctionType(returnType, const_cast<LType*>(paramTypes), paramCount, variadicity == Variadic);
}
template<typename VectorType>
inline LType functionType(LType returnType, const VectorType& vector, Variadicity variadicity = NotVariadic)
{
    return functionType(returnType, vector.begin(), vector.size(), variadicity);
}
static inline LType functionType(LType returnType, Variadicity variadicity = NotVariadic)
{
    return functionType(returnType, 0, 0, variadicity);
}
static inline LType functionType(LType returnType, LType param1, Variadicity variadicity = NotVariadic)
{
    return functionType(returnType, &param1, 1, variadicity);
}
static inline LType functionType(LType returnType, LType param1, LType param2, Variadicity variadicity = NotVariadic)
{
    LType paramTypes[] = { param1, param2 };
    return functionType(returnType, paramTypes, 2, variadicity);
}
static inline LType functionType(LType returnType, LType param1, LType param2, LType param3, Variadicity variadicity = NotVariadic)
{
    LType paramTypes[] = { param1, param2, param3 };
    return functionType(returnType, paramTypes, 3, variadicity);
}
static inline LType functionType(LType returnType, LType param1, LType param2, LType param3, LType param4, Variadicity variadicity = NotVariadic)
{
    LType paramTypes[] = { param1, param2, param3, param4 };
    return functionType(returnType, paramTypes, 4, variadicity);
}

static inline LType typeOf(LValue value) { return llvm->TypeOf(value); }

static inline unsigned mdKindID(LContext context, const char* string) { return llvm->GetMDKindIDInContext(context, string, std::strlen(string)); }
static inline LValue mdString(LContext context, const char* string, unsigned length) { return llvm->MDStringInContext(context, string, length); }
static inline LValue mdString(LContext context, const char* string) { return mdString(context, string, std::strlen(string)); }
static inline LValue mdNode(LContext context, LValue* args, unsigned numArgs) { return llvm->MDNodeInContext(context, args, numArgs); }
template<typename VectorType>
static inline LValue mdNode(LContext context, const VectorType& vector) { return mdNode(context, const_cast<LValue*>(vector.begin()), vector.size()); }
static inline LValue mdNode(LContext context) { return mdNode(context, 0, 0); }
static inline LValue mdNode(LContext context, LValue arg1) { return mdNode(context, &arg1, 1); }
static inline LValue mdNode(LContext context, LValue arg1, LValue arg2)
{
    LValue args[] = { arg1, arg2 };
    return mdNode(context, args, 2);
}
static inline LValue mdNode(LContext context, LValue arg1, LValue arg2, LValue arg3)
{
    LValue args[] = { arg1, arg2, arg3 };
    return mdNode(context, args, 3);
}

static inline void setMetadata(LValue instruction, unsigned kind, LValue metadata) { llvm->SetMetadata(instruction, kind, metadata); }

static inline LValue addFunction(LModule module, const char* name, LType type) { return llvm->AddFunction(module, name, type); }
static inline void setLinkage(LValue global, LLinkage linkage) { llvm->SetLinkage(global, linkage); }
static inline void setFunctionCallingConv(LValue function, LCallConv convention) { llvm->SetFunctionCallConv(function, convention); }
static inline void addTargetDependentFunctionAttr(LValue function, const char* key, const char* value) { llvm->AddTargetDependentFunctionAttr(function, key, value); }

static inline LValue addExternFunction(LModule module, const char* name, LType type)
{
    LValue result = addFunction(module, name, type);
    setLinkage(result, LLVMExternalLinkage);
    return result;
}

static inline LValue getParam(LValue function, unsigned index) { return llvm->GetParam(function, index); }
static inline LValue getUndef(LType type) { return llvm->GetUndef(type); }

enum BitExtension { ZeroExtend, SignExtend };
static inline LValue constInt(LType type, unsigned long long value, BitExtension extension = ZeroExtend) { return llvm->ConstInt(type, value, extension == SignExtend); }
static inline LValue constReal(LType type, double value) { return llvm->ConstReal(type, value); }
static inline LValue constIntToPtr(LValue value, LType type) { return llvm->ConstIntToPtr(value, type); }
static inline LValue constNull(LType type) { return llvm->ConstNull(type); }
static inline LValue constBitCast(LValue value, LType type) { return llvm->ConstBitCast(value, type); }

static inline LBasicBlock appendBasicBlock(LContext context, LValue function, const char* name = "") { return llvm->AppendBasicBlockInContext(context, function, name); }
static inline LBasicBlock insertBasicBlock(LContext context, LBasicBlock beforeBasicBlock, const char* name = "") { return llvm->InsertBasicBlockInContext(context, beforeBasicBlock, name); }

static inline LValue buildPhi(LBuilder builder, LType type) { return llvm->BuildPhi(builder, type, ""); }
static inline void addIncoming(LValue phi, const LValue* values, const LBasicBlock* blocks, unsigned numPredecessors)
{
    llvm->AddIncoming(phi, const_cast<LValue*>(values), const_cast<LBasicBlock*>(blocks), numPredecessors);
}
static inline void addIncoming(LValue phi, ValueFromBlock value1)
{
    LValue value = value1.value();
    LBasicBlock block = value1.block();
    addIncoming(phi, &value, &block, 1);
}
static inline void addIncoming(LValue phi, ValueFromBlock value1, ValueFromBlock value2)
{
    LValue values[] = { value1.value(), value2.value() };
    LBasicBlock blocks[] = { value1.block(), value2.block() };
    addIncoming(phi, values, blocks, 2);
}
static inline LValue buildPhi(LBuilder builder, LType type, ValueFromBlock value1)
{
    LValue result = buildPhi(builder, type);
    addIncoming(result, value1);
    return result;
}
static inline LValue buildPhi(
    LBuilder builder, LType type, ValueFromBlock value1, ValueFromBlock value2)
{
    LValue result = buildPhi(builder, type);
    addIncoming(result, value1, value2);
    return result;
}

static inline LValue buildAlloca(LBuilder builder, LType type) { return llvm->BuildAlloca(builder, type, ""); }
static inline LValue buildAdd(LBuilder builder, LValue left, LValue right) { return llvm->BuildAdd(builder, left, right, ""); }
static inline LValue buildSub(LBuilder builder, LValue left, LValue right) { return llvm->BuildSub(builder, left, right, ""); }
static inline LValue buildMul(LBuilder builder, LValue left, LValue right) { return llvm->BuildMul(builder, left, right, ""); }
static inline LValue buildDiv(LBuilder builder, LValue left, LValue right) { return llvm->BuildSDiv(builder, left, right, ""); }
static inline LValue buildRem(LBuilder builder, LValue left, LValue right) { return llvm->BuildSRem(builder, left, right, ""); }
static inline LValue buildNeg(LBuilder builder, LValue value) { return llvm->BuildNeg(builder, value, ""); }
static inline LValue buildFAdd(LBuilder builder, LValue left, LValue right) { return llvm->BuildFAdd(builder, left, right, ""); }
static inline LValue buildFSub(LBuilder builder, LValue left, LValue right) { return llvm->BuildFSub(builder, left, right, ""); }
static inline LValue buildFMul(LBuilder builder, LValue left, LValue right) { return llvm->BuildFMul(builder, left, right, ""); }
static inline LValue buildFDiv(LBuilder builder, LValue left, LValue right) { return llvm->BuildFDiv(builder, left, right, ""); }
static inline LValue buildFRem(LBuilder builder, LValue left, LValue right) { return llvm->BuildFRem(builder, left, right, ""); }
static inline LValue buildFNeg(LBuilder builder, LValue value) { return llvm->BuildFNeg(builder, value, ""); }
static inline LValue buildAnd(LBuilder builder, LValue left, LValue right) { return llvm->BuildAnd(builder, left, right, ""); }
static inline LValue buildOr(LBuilder builder, LValue left, LValue right) { return llvm->BuildOr(builder, left, right, ""); }
static inline LValue buildXor(LBuilder builder, LValue left, LValue right) { return llvm->BuildXor(builder, left, right, ""); }
static inline LValue buildShl(LBuilder builder, LValue left, LValue right) { return llvm->BuildShl(builder, left, right, ""); }
static inline LValue buildAShr(LBuilder builder, LValue left, LValue right) { return llvm->BuildAShr(builder, left, right, ""); }
static inline LValue buildLShr(LBuilder builder, LValue left, LValue right) { return llvm->BuildLShr(builder, left, right, ""); }
static inline LValue buildNot(LBuilder builder, LValue value) { return llvm->BuildNot(builder, value, ""); }
static inline LValue buildLoad(LBuilder builder, LValue pointer) { return llvm->BuildLoad(builder, pointer, ""); }
static inline LValue buildStore(LBuilder builder, LValue value, LValue pointer) { return llvm->BuildStore(builder, value, pointer); }
static inline LValue buildSExt(LBuilder builder, LValue value, LType type) { return llvm->BuildSExt(builder, value, type, ""); }
static inline LValue buildZExt(LBuilder builder, LValue value, LType type) { return llvm->BuildZExt(builder, value, type, ""); }
static inline LValue buildFPToSI(LBuilder builder, LValue value, LType type) { return llvm->BuildFPToSI(builder, value, type, ""); }
static inline LValue buildFPToUI(LBuilder builder, LValue value, LType type) { return llvm->BuildFPToUI(builder, value, type, ""); }
static inline LValue buildSIToFP(LBuilder builder, LValue value, LType type) { return llvm->BuildSIToFP(builder, value, type, ""); }
static inline LValue buildUIToFP(LBuilder builder, LValue value, LType type) { return llvm->BuildUIToFP(builder, value, type, ""); }
static inline LValue buildIntCast(LBuilder builder, LValue value, LType type) { return llvm->BuildIntCast(builder, value, type, ""); }
static inline LValue buildFPCast(LBuilder builder, LValue value, LType type) { return llvm->BuildFPCast(builder, value, type, ""); }
static inline LValue buildIntToPtr(LBuilder builder, LValue value, LType type) { return llvm->BuildIntToPtr(builder, value, type, ""); }
static inline LValue buildPtrToInt(LBuilder builder, LValue value, LType type) { return llvm->BuildPtrToInt(builder, value, type, ""); }
static inline LValue buildBitCast(LBuilder builder, LValue value, LType type) { return llvm->BuildBitCast(builder, value, type, ""); }
static inline LValue buildICmp(LBuilder builder, LIntPredicate cond, LValue left, LValue right) { return llvm->BuildICmp(builder, cond, left, right, ""); }
static inline LValue buildFCmp(LBuilder builder, LRealPredicate cond, LValue left, LValue right) { return llvm->BuildFCmp(builder, cond, left, right, ""); }
static inline LValue buildInsertElement(LBuilder builder, LValue vector, LValue element, LValue index) { return llvm->BuildInsertElement(builder, vector, element, index, ""); }

enum SynchronizationScope { SingleThread, CrossThread };
static inline LValue buildFence(LBuilder builder, LAtomicOrdering ordering, SynchronizationScope scope = CrossThread)
{
    return llvm->BuildFence(builder, ordering, scope == SingleThread, "");
}

static inline LValue buildCall(LBuilder builder, LValue function, const LValue* args, unsigned numArgs)
{
    return llvm->BuildCall(builder, function, const_cast<LValue*>(args), numArgs, "");
}
template<typename VectorType>
inline LValue buildCall(LBuilder builder, LValue function, const VectorType& vector)
{
    return buildCall(builder, function, vector.begin(), vector.size());
}
static inline LValue buildCall(LBuilder builder, LValue function)
{
    return buildCall(builder, function, 0, 0);
}
static inline LValue buildCall(LBuilder builder, LValue function, LValue arg1)
{
    return buildCall(builder, function, &arg1, 1);
}
static inline LValue buildCall(LBuilder builder, LValue function, LValue arg1, LValue arg2)
{
    LValue args[] = { arg1, arg2 };
    return buildCall(builder, function, args, 2);
}
static inline LValue buildCall(LBuilder builder, LValue function, LValue arg1, LValue arg2, LValue arg3)
{
    LValue args[] = { arg1, arg2, arg3 };
    return buildCall(builder, function, args, 3);
}
static inline LValue buildCall(LBuilder builder, LValue function, LValue arg1, LValue arg2, LValue arg3, LValue arg4)
{
    LValue args[] = { arg1, arg2, arg3, arg4 };
    return buildCall(builder, function, args, 4);
}
static inline LValue buildCall(LBuilder builder, LValue function, LValue arg1, LValue arg2, LValue arg3, LValue arg4, LValue arg5)
{
    LValue args[] = { arg1, arg2, arg3, arg4, arg5 };
    return buildCall(builder, function, args, 5);
}
static inline LValue buildCall(LBuilder builder, LValue function, LValue arg1, LValue arg2, LValue arg3, LValue arg4, LValue arg5, LValue arg6)
{
    LValue args[] = { arg1, arg2, arg3, arg4, arg5, arg6 };
    return buildCall(builder, function, args, 6);
}
static inline LValue buildCall(LBuilder builder, LValue function, LValue arg1, LValue arg2, LValue arg3, LValue arg4, LValue arg5, LValue arg6, LValue arg7)
{
    LValue args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7 };
    return buildCall(builder, function, args, 7);
}
static inline LValue buildCall(LBuilder builder, LValue function, LValue arg1, LValue arg2, LValue arg3, LValue arg4, LValue arg5, LValue arg6, LValue arg7, LValue arg8)
{
    LValue args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 };
    return buildCall(builder, function, args, 8);
}
static inline void setInstructionCallingConvention(LValue instruction, LCallConv callingConvention) { llvm->SetInstructionCallConv(instruction, callingConvention); }
static inline LValue buildExtractValue(LBuilder builder, LValue aggVal, unsigned index) { return llvm->BuildExtractValue(builder, aggVal, index, ""); }
static inline LValue buildSelect(LBuilder builder, LValue condition, LValue taken, LValue notTaken) { return llvm->BuildSelect(builder, condition, taken, notTaken, ""); }
static inline LValue buildBr(LBuilder builder, LBasicBlock destination) { return llvm->BuildBr(builder, destination); }
static inline LValue buildCondBr(LBuilder builder, LValue condition, LBasicBlock taken, LBasicBlock notTaken) { return llvm->BuildCondBr(builder, condition, taken, notTaken); }
static inline LValue buildSwitch(LBuilder builder, LValue value, LBasicBlock fallThrough, unsigned numCases) { return llvm->BuildSwitch(builder, value, fallThrough, numCases); }
static inline void addCase(LValue switchInst, LValue value, LBasicBlock target) { llvm->AddCase(switchInst, value, target); }
template<typename VectorType>
static inline LValue buildSwitch(LBuilder builder, LValue value, const VectorType& cases, LBasicBlock fallThrough)
{
    LValue result = buildSwitch(builder, value, fallThrough, cases.size());
    for (unsigned i = 0; i < cases.size(); ++i)
        addCase(result, cases[i].value(), cases[i].target());
    return result;
}
static inline LValue buildRet(LBuilder builder, LValue value) { return llvm->BuildRet(builder, value); }
static inline LValue buildUnreachable(LBuilder builder) { return llvm->BuildUnreachable(builder); }

static inline void dumpModule(LModule module) { llvm->DumpModule(module); }
static inline void verifyModule(LModule module)
{
    char* error = 0;
    llvm->VerifyModule(module, LLVMAbortProcessAction, &error);
    llvm->DisposeMessage(error);
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLAbbreviations_h

