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

#include <wtf/Platform.h>

#if ENABLE(FTL_JIT)

#include "FTLAbbreviatedTypes.h"
#include "FTLSwitchCase.h"
#include "FTLValueFromBlock.h"
#include <cstring>

namespace JSC { namespace FTL {

// This file contains short-form calls into the LLVM C API. It is meant to
// save typing and make the lowering code clearer. If we ever call an LLVM C API
// function more than once in the FTL lowering code, we should add a shortcut for
// it here.

#if USE(JSVALUE32_64)
#error "The FTL backend assumes that pointers are 64-bit."
#endif

static inline LType voidType(LContext context) { return LLVMVoidTypeInContext(context); }
static inline LType int1Type(LContext context) { return LLVMInt1TypeInContext(context); }
static inline LType int8Type(LContext context) { return LLVMInt8TypeInContext(context); }
static inline LType int16Type(LContext context) { return LLVMInt16TypeInContext(context); }
static inline LType int32Type(LContext context) { return LLVMInt32TypeInContext(context); }
static inline LType int64Type(LContext context) { return LLVMInt64TypeInContext(context); }
static inline LType intPtrType(LContext context) { return LLVMInt64TypeInContext(context); }
static inline LType doubleType(LContext context) { return LLVMDoubleTypeInContext(context); }

static inline LType pointerType(LType type) { return LLVMPointerType(type, 0); }

enum PackingMode { NotPacked, Packed };
static inline LType structType(LContext context, LType* elementTypes, unsigned elementCount, PackingMode packing = NotPacked)
{
    return LLVMStructTypeInContext(context, elementTypes, elementCount, packing == Packed);
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
    return LLVMFunctionType(returnType, const_cast<LType*>(paramTypes), paramCount, variadicity == Variadic);
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

static inline LType typeOf(LValue value) { return LLVMTypeOf(value); }

static inline unsigned mdKindID(LContext context, const char* string) { return LLVMGetMDKindIDInContext(context, string, std::strlen(string)); }
static inline LValue mdString(LContext context, const char* string, unsigned length) { return LLVMMDStringInContext(context, string, length); }
static inline LValue mdString(LContext context, const char* string) { return mdString(context, string, std::strlen(string)); }
static inline LValue mdNode(LContext context, LValue* args, unsigned numArgs) { return LLVMMDNodeInContext(context, args, numArgs); }
static inline LValue mdNode(LContext context) { return mdNode(context, 0, 0); }
static inline LValue mdNode(LContext context, LValue arg1) { return mdNode(context, &arg1, 1); }
static inline LValue mdNode(LContext context, LValue arg1, LValue arg2)
{
    LValue args[] = { arg1, arg2 };
    return mdNode(context, args, 2);
}

static inline void setMetadata(LValue instruction, unsigned kind, LValue metadata) { LLVMSetMetadata(instruction, kind, metadata); }

static inline LValue addFunction(LModule module, const char* name, LType type) { return LLVMAddFunction(module, name, type); }
static inline void setLinkage(LValue global, LLinkage linkage) { LLVMSetLinkage(global, linkage); }
static inline void setFunctionCallingConv(LValue function, LCallConv convention) { LLVMSetFunctionCallConv(function, convention); }

static inline LValue addExternFunction(LModule module, const char* name, LType type)
{
    LValue result = addFunction(module, name, type);
    setLinkage(result, LLVMExternalLinkage);
    return result;
}

static inline LValue getParam(LValue function, unsigned index) { return LLVMGetParam(function, index); }

enum BitExtension { ZeroExtend, SignExtend };
static inline LValue constInt(LType type, unsigned long long value, BitExtension extension = ZeroExtend) { return LLVMConstInt(type, value, extension == SignExtend); }
static inline LValue constReal(LType type, double value) { return LLVMConstReal(type, value); }
static inline LValue constIntToPtr(LValue value, LType type) { return LLVMConstIntToPtr(value, type); }
static inline LValue constBitCast(LValue value, LType type) { return LLVMConstBitCast(value, type); }

static inline LBasicBlock appendBasicBlock(LContext context, LValue function, const char* name = "") { return LLVMAppendBasicBlockInContext(context, function, name); }
static inline LBasicBlock insertBasicBlock(LContext context, LBasicBlock beforeBasicBlock, const char* name = "") { return LLVMInsertBasicBlockInContext(context, beforeBasicBlock, name); }

static inline LValue buildPhi(LBuilder builder, LType type) { return LLVMBuildPhi(builder, type, ""); }
static inline void addIncoming(LValue phi, const LValue* values, const LBasicBlock* blocks, unsigned numPredecessors)
{
    LLVMAddIncoming(phi, const_cast<LValue*>(values), const_cast<LBasicBlock*>(blocks), numPredecessors);
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

static inline LValue buildAlloca(LBuilder builder, LType type) { return LLVMBuildAlloca(builder, type, ""); }
static inline LValue buildAdd(LBuilder builder, LValue left, LValue right) { return LLVMBuildAdd(builder, left, right, ""); }
static inline LValue buildSub(LBuilder builder, LValue left, LValue right) { return LLVMBuildSub(builder, left, right, ""); }
static inline LValue buildMul(LBuilder builder, LValue left, LValue right) { return LLVMBuildMul(builder, left, right, ""); }
static inline LValue buildDiv(LBuilder builder, LValue left, LValue right) { return LLVMBuildSDiv(builder, left, right, ""); }
static inline LValue buildRem(LBuilder builder, LValue left, LValue right) { return LLVMBuildSRem(builder, left, right, ""); }
static inline LValue buildNeg(LBuilder builder, LValue value) { return LLVMBuildNeg(builder, value, ""); }
static inline LValue buildFAdd(LBuilder builder, LValue left, LValue right) { return LLVMBuildFAdd(builder, left, right, ""); }
static inline LValue buildFSub(LBuilder builder, LValue left, LValue right) { return LLVMBuildFSub(builder, left, right, ""); }
static inline LValue buildFMul(LBuilder builder, LValue left, LValue right) { return LLVMBuildFMul(builder, left, right, ""); }
static inline LValue buildFDiv(LBuilder builder, LValue left, LValue right) { return LLVMBuildFDiv(builder, left, right, ""); }
static inline LValue buildFRem(LBuilder builder, LValue left, LValue right) { return LLVMBuildFRem(builder, left, right, ""); }
static inline LValue buildFNeg(LBuilder builder, LValue value) { return LLVMBuildFNeg(builder, value, ""); }
static inline LValue buildAnd(LBuilder builder, LValue left, LValue right) { return LLVMBuildAnd(builder, left, right, ""); }
static inline LValue buildOr(LBuilder builder, LValue left, LValue right) { return LLVMBuildOr(builder, left, right, ""); }
static inline LValue buildXor(LBuilder builder, LValue left, LValue right) { return LLVMBuildXor(builder, left, right, ""); }
static inline LValue buildShl(LBuilder builder, LValue left, LValue right) { return LLVMBuildShl(builder, left, right, ""); }
static inline LValue buildAShr(LBuilder builder, LValue left, LValue right) { return LLVMBuildAShr(builder, left, right, ""); }
static inline LValue buildLShr(LBuilder builder, LValue left, LValue right) { return LLVMBuildLShr(builder, left, right, ""); }
static inline LValue buildNot(LBuilder builder, LValue value) { return LLVMBuildNot(builder, value, ""); }
static inline LValue buildLoad(LBuilder builder, LValue pointer) { return LLVMBuildLoad(builder, pointer, ""); }
static inline LValue buildStore(LBuilder builder, LValue value, LValue pointer) { return LLVMBuildStore(builder, value, pointer); }
static inline LValue buildZExt(LBuilder builder, LValue value, LType type) { return LLVMBuildZExt(builder, value, type, ""); }
static inline LValue buildFPToSI(LBuilder builder, LValue value, LType type) { return LLVMBuildFPToSI(builder, value, type, ""); }
static inline LValue buildSIToFP(LBuilder builder, LValue value, LType type) { return LLVMBuildSIToFP(builder, value, type, ""); }
static inline LValue buildUIToFP(LBuilder builder, LValue value, LType type) { return LLVMBuildUIToFP(builder, value, type, ""); }
static inline LValue buildIntCast(LBuilder builder, LValue value, LType type) { return LLVMBuildIntCast(builder, value, type, ""); }
static inline LValue buildIntToPtr(LBuilder builder, LValue value, LType type) { return LLVMBuildIntToPtr(builder, value, type, ""); }
static inline LValue buildPtrToInt(LBuilder builder, LValue value, LType type) { return LLVMBuildPtrToInt(builder, value, type, ""); }
static inline LValue buildBitCast(LBuilder builder, LValue value, LType type) { return LLVMBuildBitCast(builder, value, type, ""); }
static inline LValue buildICmp(LBuilder builder, LIntPredicate cond, LValue left, LValue right) { return LLVMBuildICmp(builder, cond, left, right, ""); }
static inline LValue buildFCmp(LBuilder builder, LRealPredicate cond, LValue left, LValue right) { return LLVMBuildFCmp(builder, cond, left, right, ""); }
static inline LValue buildCall(LBuilder builder, LValue function, const LValue* args, unsigned numArgs)
{
    return LLVMBuildCall(builder, function, const_cast<LValue*>(args), numArgs, "");
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
enum TailCallMode { IsNotTailCall, IsTailCall };
static inline void setTailCall(LValue call, TailCallMode mode) { LLVMSetTailCall(call, mode == IsTailCall); }
static inline LValue buildExtractValue(LBuilder builder, LValue aggVal, unsigned index) { return LLVMBuildExtractValue(builder, aggVal, index, ""); }
static inline LValue buildSelect(LBuilder builder, LValue condition, LValue taken, LValue notTaken) { return LLVMBuildSelect(builder, condition, taken, notTaken, ""); }
static inline LValue buildBr(LBuilder builder, LBasicBlock destination) { return LLVMBuildBr(builder, destination); }
static inline LValue buildCondBr(LBuilder builder, LValue condition, LBasicBlock taken, LBasicBlock notTaken) { return LLVMBuildCondBr(builder, condition, taken, notTaken); }
static inline LValue buildSwitch(LBuilder builder, LValue value, LBasicBlock fallThrough, unsigned numCases) { return LLVMBuildSwitch(builder, value, fallThrough, numCases); }
static inline void addCase(LValue switchInst, LValue value, LBasicBlock target) { LLVMAddCase(switchInst, value, target); }
template<typename VectorType>
static inline LValue buildSwitch(LBuilder builder, LValue value, const VectorType& cases, LBasicBlock fallThrough)
{
    LValue result = buildSwitch(builder, value, fallThrough, cases.size());
    for (unsigned i = 0; i < cases.size(); ++i)
        addCase(result, cases[i].value(), cases[i].target());
    return result;
}
static inline LValue buildRet(LBuilder builder, LValue value) { return LLVMBuildRet(builder, value); }
static inline LValue buildUnreachable(LBuilder builder) { return LLVMBuildUnreachable(builder); }

static inline void dumpModule(LModule module) { LLVMDumpModule(module); }
static inline void verifyModule(LModule module)
{
    char* error = 0;
    LLVMVerifyModule(module, LLVMAbortProcessAction, &error);
    LLVMDisposeMessage(error);
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLAbbreviations_h

