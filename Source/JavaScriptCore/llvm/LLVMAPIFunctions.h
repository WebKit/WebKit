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

#ifndef LLVMAPIFunctions_h
#define LLVMAPIFunctions_h

#include "LLVMHeaders.h"

#define FOR_EACH_LLVM_API_FUNCTION(macro) \
    macro(void, InitializeCore, (LLVMPassRegistryRef R)) \
    macro(void, Shutdown, ()) \
    macro(char *, CreateMessage, (const char *Message)) \
    macro(void, DisposeMessage, (char *Message)) \
    macro(void, InstallFatalErrorHandler, (LLVMFatalErrorHandler Handler)) \
    macro(LLVMContextRef, ContextCreate, (void)) \
    macro(LLVMContextRef, GetGlobalContext, (void)) \
    macro(void, ContextDispose, (LLVMContextRef C)) \
    macro(unsigned, GetMDKindIDInContext, (LLVMContextRef C, const char* Name, unsigned SLen)) \
    macro(unsigned, GetMDKindID, (const char* Name, unsigned SLen)) \
    macro(LLVMModuleRef, ModuleCreateWithName, (const char *ModuleID)) \
    macro(LLVMModuleRef, ModuleCreateWithNameInContext, (const char *ModuleID, LLVMContextRef C)) \
    macro(void, DisposeModule, (LLVMModuleRef M)) \
    macro(const char *, GetDataLayout, (LLVMModuleRef M)) \
    macro(void, SetDataLayout, (LLVMModuleRef M, const char *Triple)) \
    macro(const char *, GetTarget, (LLVMModuleRef M)) \
    macro(void, SetTarget, (LLVMModuleRef M, const char *Triple)) \
    macro(void, DumpModule, (LLVMModuleRef M)) \
    macro(LLVMBool, PrintModuleToFile, (LLVMModuleRef M, const char *Filename, char **ErrorMessage)) \
    macro(void, SetModuleInlineAsm, (LLVMModuleRef M, const char *Asm)) \
    macro(LLVMContextRef, GetModuleContext, (LLVMModuleRef M)) \
    macro(LLVMTypeRef, GetTypeByName, (LLVMModuleRef M, const char *Name)) \
    macro(unsigned, GetNamedMetadataNumOperands, (LLVMModuleRef M, const char* name)) \
    macro(void, GetNamedMetadataOperands, (LLVMModuleRef M, const char* name, LLVMValueRef *Dest)) \
    macro(void, AddNamedMetadataOperand, (LLVMModuleRef M, const char* name, LLVMValueRef Val)) \
    macro(LLVMValueRef, AddFunction, (LLVMModuleRef M, const char *Name, LLVMTypeRef FunctionTy)) \
    macro(LLVMValueRef, GetNamedFunction, (LLVMModuleRef M, const char *Name)) \
    macro(LLVMValueRef, GetFirstFunction, (LLVMModuleRef M)) \
    macro(LLVMValueRef, GetLastFunction, (LLVMModuleRef M)) \
    macro(LLVMValueRef, GetNextFunction, (LLVMValueRef Fn)) \
    macro(LLVMValueRef, GetPreviousFunction, (LLVMValueRef Fn)) \
    macro(LLVMTypeKind, GetTypeKind, (LLVMTypeRef Ty)) \
    macro(LLVMBool, TypeIsSized, (LLVMTypeRef Ty)) \
    macro(LLVMContextRef, GetTypeContext, (LLVMTypeRef Ty)) \
    macro(LLVMTypeRef, Int1TypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, Int8TypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, Int16TypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, Int32TypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, Int64TypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, IntTypeInContext, (LLVMContextRef C, unsigned NumBits)) \
    macro(LLVMTypeRef, Int1Type, (void)) \
    macro(LLVMTypeRef, Int8Type, (void)) \
    macro(LLVMTypeRef, Int16Type, (void)) \
    macro(LLVMTypeRef, Int32Type, (void)) \
    macro(LLVMTypeRef, Int64Type, (void)) \
    macro(LLVMTypeRef, IntType, (unsigned NumBits)) \
    macro(unsigned, GetIntTypeWidth, (LLVMTypeRef IntegerTy)) \
    macro(LLVMTypeRef, HalfTypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, FloatTypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, DoubleTypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, X86FP80TypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, FP128TypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, PPCFP128TypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, HalfType, (void)) \
    macro(LLVMTypeRef, FloatType, (void)) \
    macro(LLVMTypeRef, DoubleType, (void)) \
    macro(LLVMTypeRef, X86FP80Type, (void)) \
    macro(LLVMTypeRef, FP128Type, (void)) \
    macro(LLVMTypeRef, PPCFP128Type, (void)) \
    macro(LLVMTypeRef, FunctionType, (LLVMTypeRef ReturnType, LLVMTypeRef *ParamTypes, unsigned ParamCount, LLVMBool IsVarArg)) \
    macro(LLVMBool, IsFunctionVarArg, (LLVMTypeRef FunctionTy)) \
    macro(LLVMTypeRef, GetReturnType, (LLVMTypeRef FunctionTy)) \
    macro(unsigned, CountParamTypes, (LLVMTypeRef FunctionTy)) \
    macro(void, GetParamTypes, (LLVMTypeRef FunctionTy, LLVMTypeRef *Dest)) \
    macro(LLVMTypeRef, StructTypeInContext, (LLVMContextRef C, LLVMTypeRef *ElementTypes, unsigned ElementCount, LLVMBool Packed)) \
    macro(LLVMTypeRef, StructType, (LLVMTypeRef *ElementTypes, unsigned ElementCount, LLVMBool Packed)) \
    macro(LLVMTypeRef, StructCreateNamed, (LLVMContextRef C, const char *Name)) \
    macro(const char *, GetStructName, (LLVMTypeRef Ty)) \
    macro(void, StructSetBody, (LLVMTypeRef StructTy, LLVMTypeRef *ElementTypes, unsigned ElementCount, LLVMBool Packed)) \
    macro(unsigned, CountStructElementTypes, (LLVMTypeRef StructTy)) \
    macro(void, GetStructElementTypes, (LLVMTypeRef StructTy, LLVMTypeRef *Dest)) \
    macro(LLVMBool, IsPackedStruct, (LLVMTypeRef StructTy)) \
    macro(LLVMBool, IsOpaqueStruct, (LLVMTypeRef StructTy)) \
    macro(LLVMTypeRef, GetElementType, (LLVMTypeRef Ty)) \
    macro(LLVMTypeRef, ArrayType, (LLVMTypeRef ElementType, unsigned ElementCount)) \
    macro(unsigned, GetArrayLength, (LLVMTypeRef ArrayTy)) \
    macro(LLVMTypeRef, PointerType, (LLVMTypeRef ElementType, unsigned AddressSpace)) \
    macro(unsigned, GetPointerAddressSpace, (LLVMTypeRef PointerTy)) \
    macro(LLVMTypeRef, VectorType, (LLVMTypeRef ElementType, unsigned ElementCount)) \
    macro(unsigned, GetVectorSize, (LLVMTypeRef VectorTy)) \
    macro(LLVMTypeRef, VoidTypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, LabelTypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, X86MMXTypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, VoidType, (void)) \
    macro(LLVMTypeRef, LabelType, (void)) \
    macro(LLVMTypeRef, X86MMXType, (void)) \
    macro(LLVMTypeRef, TypeOf, (LLVMValueRef Val)) \
    macro(const char *, GetValueName, (LLVMValueRef Val)) \
    macro(void, SetValueName, (LLVMValueRef Val, const char *Name)) \
    macro(void, DumpValue, (LLVMValueRef Val)) \
    macro(void, ReplaceAllUsesWith, (LLVMValueRef OldVal, LLVMValueRef NewVal)) \
    macro(LLVMBool, IsConstant, (LLVMValueRef Val)) \
    macro(LLVMBool, IsUndef, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAArgument, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsABasicBlock, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAInlineAsm, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAMDNode, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAMDString, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAUser, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAConstant, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsABlockAddress, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAConstantAggregateZero, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAConstantArray, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAConstantExpr, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAConstantFP, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAConstantInt, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAConstantPointerNull, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAConstantStruct, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAConstantVector, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAGlobalValue, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAFunction, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAGlobalAlias, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAGlobalVariable, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAUndefValue, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAInstruction, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsABinaryOperator, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsACallInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAIntrinsicInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsADbgInfoIntrinsic, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsADbgDeclareInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAMemIntrinsic, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAMemCpyInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAMemMoveInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAMemSetInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsACmpInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAFCmpInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAICmpInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAExtractElementInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAGetElementPtrInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAInsertElementInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAInsertValueInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsALandingPadInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAPHINode, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsASelectInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAShuffleVectorInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAStoreInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsATerminatorInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsABranchInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAIndirectBrInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAInvokeInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAReturnInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsASwitchInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAUnreachableInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAResumeInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAUnaryInstruction, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAAllocaInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsACastInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsABitCastInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAFPExtInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAFPToSIInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAFPToUIInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAFPTruncInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAIntToPtrInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAPtrToIntInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsASExtInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsASIToFPInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsATruncInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAUIToFPInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAZExtInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAExtractValueInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsALoadInst, (LLVMValueRef Val)) \
    macro(LLVMValueRef, IsAVAArgInst, (LLVMValueRef Val)) \
    macro(LLVMUseRef, GetFirstUse, (LLVMValueRef Val)) \
    macro(LLVMUseRef, GetNextUse, (LLVMUseRef U)) \
    macro(LLVMValueRef, GetUser, (LLVMUseRef U)) \
    macro(LLVMValueRef, GetUsedValue, (LLVMUseRef U)) \
    macro(LLVMValueRef, GetOperand, (LLVMValueRef Val, unsigned Index)) \
    macro(void, SetOperand, (LLVMValueRef User, unsigned Index, LLVMValueRef Val)) \
    macro(int, GetNumOperands, (LLVMValueRef Val)) \
    macro(LLVMValueRef, ConstNull, (LLVMTypeRef Ty)) \
    macro(LLVMValueRef, ConstAllOnes, (LLVMTypeRef Ty)) \
    macro(LLVMValueRef, GetUndef, (LLVMTypeRef Ty)) \
    macro(LLVMBool, IsNull, (LLVMValueRef Val)) \
    macro(LLVMValueRef, ConstPointerNull, (LLVMTypeRef Ty)) \
    macro(LLVMValueRef, ConstInt, (LLVMTypeRef IntTy, unsigned long long N, LLVMBool SignExtend)) \
    macro(LLVMValueRef, ConstIntOfArbitraryPrecision, (LLVMTypeRef IntTy, unsigned NumWords, const uint64_t Words[])) \
    macro(LLVMValueRef, ConstIntOfString, (LLVMTypeRef IntTy, const char *Text, uint8_t Radix)) \
    macro(LLVMValueRef, ConstIntOfStringAndSize, (LLVMTypeRef IntTy, const char *Text, unsigned SLen, uint8_t Radix)) \
    macro(LLVMValueRef, ConstReal, (LLVMTypeRef RealTy, double N)) \
    macro(LLVMValueRef, ConstRealOfString, (LLVMTypeRef RealTy, const char *Text)) \
    macro(LLVMValueRef, ConstRealOfStringAndSize, (LLVMTypeRef RealTy, const char *Text, unsigned SLen)) \
    macro(unsigned long long, ConstIntGetZExtValue, (LLVMValueRef ConstantVal)) \
    macro(long long, ConstIntGetSExtValue, (LLVMValueRef ConstantVal)) \
    macro(LLVMValueRef, ConstStringInContext, (LLVMContextRef C, const char *Str, unsigned Length, LLVMBool DontNullTerminate)) \
    macro(LLVMValueRef, ConstString, (const char *Str, unsigned Length, LLVMBool DontNullTerminate)) \
    macro(LLVMValueRef, ConstStructInContext, (LLVMContextRef C, LLVMValueRef *ConstantVals, unsigned Count, LLVMBool Packed)) \
    macro(LLVMValueRef, ConstStruct, (LLVMValueRef *ConstantVals, unsigned Count, LLVMBool Packed)) \
    macro(LLVMValueRef, ConstArray, (LLVMTypeRef ElementTy, LLVMValueRef *ConstantVals, unsigned Length)) \
    macro(LLVMValueRef, ConstNamedStruct, (LLVMTypeRef StructTy, LLVMValueRef *ConstantVals, unsigned Count)) \
    macro(LLVMValueRef, ConstVector, (LLVMValueRef *ScalarConstantVals, unsigned Size)) \
    macro(LLVMOpcode, GetConstOpcode, (LLVMValueRef ConstantVal)) \
    macro(LLVMValueRef, AlignOf, (LLVMTypeRef Ty)) \
    macro(LLVMValueRef, SizeOf, (LLVMTypeRef Ty)) \
    macro(LLVMValueRef, ConstNeg, (LLVMValueRef ConstantVal)) \
    macro(LLVMValueRef, ConstNSWNeg, (LLVMValueRef ConstantVal)) \
    macro(LLVMValueRef, ConstNUWNeg, (LLVMValueRef ConstantVal)) \
    macro(LLVMValueRef, ConstFNeg, (LLVMValueRef ConstantVal)) \
    macro(LLVMValueRef, ConstNot, (LLVMValueRef ConstantVal)) \
    macro(LLVMValueRef, ConstAdd, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstNSWAdd, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstNUWAdd, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstFAdd, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstSub, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstNSWSub, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstNUWSub, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstFSub, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstMul, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstNSWMul, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstNUWMul, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstFMul, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstUDiv, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstSDiv, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstExactSDiv, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstFDiv, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstURem, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstSRem, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstFRem, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstAnd, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstOr, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstXor, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstICmp, (LLVMIntPredicate Predicate, LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstFCmp, (LLVMRealPredicate Predicate, LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstShl, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstLShr, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstAShr, (LLVMValueRef LHSConstant, LLVMValueRef RHSConstant)) \
    macro(LLVMValueRef, ConstGEP, (LLVMValueRef ConstantVal, LLVMValueRef *ConstantIndices, unsigned NumIndices)) \
    macro(LLVMValueRef, ConstInBoundsGEP, (LLVMValueRef ConstantVal, LLVMValueRef *ConstantIndices, unsigned NumIndices)) \
    macro(LLVMValueRef, ConstTrunc, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstSExt, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstZExt, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstFPTrunc, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstFPExt, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstUIToFP, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstSIToFP, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstFPToUI, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstFPToSI, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstPtrToInt, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstIntToPtr, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstBitCast, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstZExtOrBitCast, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstSExtOrBitCast, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstTruncOrBitCast, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstPointerCast, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstIntCast, (LLVMValueRef ConstantVal, LLVMTypeRef ToType, LLVMBool isSigned)) \
    macro(LLVMValueRef, ConstFPCast, (LLVMValueRef ConstantVal, LLVMTypeRef ToType)) \
    macro(LLVMValueRef, ConstSelect, (LLVMValueRef ConstantCondition, LLVMValueRef ConstantIfTrue, LLVMValueRef ConstantIfFalse)) \
    macro(LLVMValueRef, ConstExtractElement, (LLVMValueRef VectorConstant, LLVMValueRef IndexConstant)) \
    macro(LLVMValueRef, ConstInsertElement, (LLVMValueRef VectorConstant, LLVMValueRef ElementValueConstant, LLVMValueRef IndexConstant)) \
    macro(LLVMValueRef, ConstShuffleVector, (LLVMValueRef VectorAConstant, LLVMValueRef VectorBConstant, LLVMValueRef MaskConstant)) \
    macro(LLVMValueRef, ConstExtractValue, (LLVMValueRef AggConstant, unsigned *IdxList, unsigned NumIdx)) \
    macro(LLVMValueRef, ConstInsertValue, (LLVMValueRef AggConstant, LLVMValueRef ElementValueConstant, unsigned *IdxList, unsigned NumIdx)) \
    macro(LLVMValueRef, ConstInlineAsm, (LLVMTypeRef Ty, const char *AsmString, const char *Constraints, LLVMBool HasSideEffects, LLVMBool IsAlignStack)) \
    macro(LLVMValueRef, BlockAddress, (LLVMValueRef F, LLVMBasicBlockRef BB)) \
    macro(LLVMModuleRef, GetGlobalParent, (LLVMValueRef Global)) \
    macro(LLVMBool, IsDeclaration, (LLVMValueRef Global)) \
    macro(LLVMLinkage, GetLinkage, (LLVMValueRef Global)) \
    macro(void, SetLinkage, (LLVMValueRef Global, LLVMLinkage Linkage)) \
    macro(const char *, GetSection, (LLVMValueRef Global)) \
    macro(void, SetSection, (LLVMValueRef Global, const char *Section)) \
    macro(LLVMVisibility, GetVisibility, (LLVMValueRef Global)) \
    macro(void, SetVisibility, (LLVMValueRef Global, LLVMVisibility Viz)) \
    macro(unsigned, GetAlignment, (LLVMValueRef Global)) \
    macro(void, SetAlignment, (LLVMValueRef Global, unsigned Bytes)) \
    macro(LLVMValueRef, AddGlobal, (LLVMModuleRef M, LLVMTypeRef Ty, const char *Name)) \
    macro(LLVMValueRef, AddGlobalInAddressSpace, (LLVMModuleRef M, LLVMTypeRef Ty, const char *Name, unsigned AddressSpace)) \
    macro(LLVMValueRef, GetNamedGlobal, (LLVMModuleRef M, const char *Name)) \
    macro(LLVMValueRef, GetFirstGlobal, (LLVMModuleRef M)) \
    macro(LLVMValueRef, GetLastGlobal, (LLVMModuleRef M)) \
    macro(LLVMValueRef, GetNextGlobal, (LLVMValueRef GlobalVar)) \
    macro(LLVMValueRef, GetPreviousGlobal, (LLVMValueRef GlobalVar)) \
    macro(void, DeleteGlobal, (LLVMValueRef GlobalVar)) \
    macro(LLVMValueRef, GetInitializer, (LLVMValueRef GlobalVar)) \
    macro(void, SetInitializer, (LLVMValueRef GlobalVar, LLVMValueRef ConstantVal)) \
    macro(LLVMBool, IsThreadLocal, (LLVMValueRef GlobalVar)) \
    macro(void, SetThreadLocal, (LLVMValueRef GlobalVar, LLVMBool IsThreadLocal)) \
    macro(LLVMBool, IsGlobalConstant, (LLVMValueRef GlobalVar)) \
    macro(void, SetGlobalConstant, (LLVMValueRef GlobalVar, LLVMBool IsConstant)) \
    macro(LLVMThreadLocalMode, GetThreadLocalMode, (LLVMValueRef GlobalVar)) \
    macro(void, SetThreadLocalMode, (LLVMValueRef GlobalVar, LLVMThreadLocalMode Mode)) \
    macro(LLVMBool, IsExternallyInitialized, (LLVMValueRef GlobalVar)) \
    macro(void, SetExternallyInitialized, (LLVMValueRef GlobalVar, LLVMBool IsExtInit)) \
    macro(LLVMValueRef, AddAlias, (LLVMModuleRef M, LLVMTypeRef Ty, LLVMValueRef Aliasee, const char *Name)) \
    macro(void, DeleteFunction, (LLVMValueRef Fn)) \
    macro(unsigned, GetIntrinsicID, (LLVMValueRef Fn)) \
    macro(unsigned, GetFunctionCallConv, (LLVMValueRef Fn)) \
    macro(void, SetFunctionCallConv, (LLVMValueRef Fn, unsigned CC)) \
    macro(const char *, GetGC, (LLVMValueRef Fn)) \
    macro(void, SetGC, (LLVMValueRef Fn, const char *Name)) \
    macro(void, AddFunctionAttr, (LLVMValueRef Fn, LLVMAttribute PA)) \
    macro(void, AddTargetDependentFunctionAttr, (LLVMValueRef Fn, const char *A, const char *V)) \
    macro(LLVMAttribute, GetFunctionAttr, (LLVMValueRef Fn)) \
    macro(void, RemoveFunctionAttr, (LLVMValueRef Fn, LLVMAttribute PA)) \
    macro(unsigned, CountParams, (LLVMValueRef Fn)) \
    macro(void, GetParams, (LLVMValueRef Fn, LLVMValueRef *Params)) \
    macro(LLVMValueRef, GetParam, (LLVMValueRef Fn, unsigned Index)) \
    macro(LLVMValueRef, GetParamParent, (LLVMValueRef Inst)) \
    macro(LLVMValueRef, GetFirstParam, (LLVMValueRef Fn)) \
    macro(LLVMValueRef, GetLastParam, (LLVMValueRef Fn)) \
    macro(LLVMValueRef, GetNextParam, (LLVMValueRef Arg)) \
    macro(LLVMValueRef, GetPreviousParam, (LLVMValueRef Arg)) \
    macro(void, AddAttribute, (LLVMValueRef Arg, LLVMAttribute PA)) \
    macro(void, RemoveAttribute, (LLVMValueRef Arg, LLVMAttribute PA)) \
    macro(LLVMAttribute, GetAttribute, (LLVMValueRef Arg)) \
    macro(void, SetParamAlignment, (LLVMValueRef Arg, unsigned align)) \
    macro(LLVMValueRef, MDStringInContext, (LLVMContextRef C, const char *Str, unsigned SLen)) \
    macro(LLVMValueRef, MDString, (const char *Str, unsigned SLen)) \
    macro(LLVMValueRef, MDNodeInContext, (LLVMContextRef C, LLVMValueRef *Vals, unsigned Count)) \
    macro(LLVMValueRef, MDNode, (LLVMValueRef *Vals, unsigned Count)) \
    macro(const char *, GetMDString, (LLVMValueRef V, unsigned* Len)) \
    macro(unsigned, GetMDNodeNumOperands, (LLVMValueRef V)) \
    macro(void, GetMDNodeOperands, (LLVMValueRef V, LLVMValueRef *Dest)) \
    macro(LLVMValueRef, BasicBlockAsValue, (LLVMBasicBlockRef BB)) \
    macro(LLVMBool, ValueIsBasicBlock, (LLVMValueRef Val)) \
    macro(LLVMBasicBlockRef, ValueAsBasicBlock, (LLVMValueRef Val)) \
    macro(LLVMValueRef, GetBasicBlockParent, (LLVMBasicBlockRef BB)) \
    macro(LLVMValueRef, GetBasicBlockTerminator, (LLVMBasicBlockRef BB)) \
    macro(unsigned, CountBasicBlocks, (LLVMValueRef Fn)) \
    macro(void, GetBasicBlocks, (LLVMValueRef Fn, LLVMBasicBlockRef *BasicBlocks)) \
    macro(LLVMBasicBlockRef, GetFirstBasicBlock, (LLVMValueRef Fn)) \
    macro(LLVMBasicBlockRef, GetLastBasicBlock, (LLVMValueRef Fn)) \
    macro(LLVMBasicBlockRef, GetNextBasicBlock, (LLVMBasicBlockRef BB)) \
    macro(LLVMBasicBlockRef, GetPreviousBasicBlock, (LLVMBasicBlockRef BB)) \
    macro(LLVMBasicBlockRef, GetEntryBasicBlock, (LLVMValueRef Fn)) \
    macro(LLVMBasicBlockRef, AppendBasicBlockInContext, (LLVMContextRef C, LLVMValueRef Fn, const char *Name)) \
    macro(LLVMBasicBlockRef, AppendBasicBlock, (LLVMValueRef Fn, const char *Name)) \
    macro(LLVMBasicBlockRef, InsertBasicBlockInContext, (LLVMContextRef C, LLVMBasicBlockRef BB, const char *Name)) \
    macro(LLVMBasicBlockRef, InsertBasicBlock, (LLVMBasicBlockRef InsertBeforeBB, const char *Name)) \
    macro(void, DeleteBasicBlock, (LLVMBasicBlockRef BB)) \
    macro(void, RemoveBasicBlockFromParent, (LLVMBasicBlockRef BB)) \
    macro(void, MoveBasicBlockBefore, (LLVMBasicBlockRef BB, LLVMBasicBlockRef MovePos)) \
    macro(void, MoveBasicBlockAfter, (LLVMBasicBlockRef BB, LLVMBasicBlockRef MovePos)) \
    macro(LLVMValueRef, GetFirstInstruction, (LLVMBasicBlockRef BB)) \
    macro(LLVMValueRef, GetLastInstruction, (LLVMBasicBlockRef BB)) \
    macro(int, HasMetadata, (LLVMValueRef Val)) \
    macro(LLVMValueRef, GetMetadata, (LLVMValueRef Val, unsigned KindID)) \
    macro(void, SetMetadata, (LLVMValueRef Val, unsigned KindID, LLVMValueRef Node)) \
    macro(LLVMBasicBlockRef, GetInstructionParent, (LLVMValueRef Inst)) \
    macro(LLVMValueRef, GetNextInstruction, (LLVMValueRef Inst)) \
    macro(LLVMValueRef, GetPreviousInstruction, (LLVMValueRef Inst)) \
    macro(void, InstructionEraseFromParent, (LLVMValueRef Inst)) \
    macro(LLVMOpcode, GetInstructionOpcode, (LLVMValueRef Inst)) \
    macro(LLVMIntPredicate, GetICmpPredicate, (LLVMValueRef Inst)) \
    macro(void, SetInstructionCallConv, (LLVMValueRef Instr, unsigned CC)) \
    macro(unsigned, GetInstructionCallConv, (LLVMValueRef Instr)) \
    macro(void, AddInstrAttribute, (LLVMValueRef Instr, unsigned index, LLVMAttribute)) \
    macro(void, RemoveInstrAttribute, (LLVMValueRef Instr, unsigned index, LLVMAttribute)) \
    macro(void, SetInstrParamAlignment, (LLVMValueRef Instr, unsigned index, unsigned align)) \
    macro(LLVMBool, IsTailCall, (LLVMValueRef CallInst)) \
    macro(void, SetTailCall, (LLVMValueRef CallInst, LLVMBool IsTailCall)) \
    macro(LLVMBasicBlockRef, GetSwitchDefaultDest, (LLVMValueRef SwitchInstr)) \
    macro(void, AddIncoming, (LLVMValueRef PhiNode, LLVMValueRef *IncomingValues, LLVMBasicBlockRef *IncomingBlocks, unsigned Count)) \
    macro(unsigned, CountIncoming, (LLVMValueRef PhiNode)) \
    macro(LLVMValueRef, GetIncomingValue, (LLVMValueRef PhiNode, unsigned Index)) \
    macro(LLVMBasicBlockRef, GetIncomingBlock, (LLVMValueRef PhiNode, unsigned Index)) \
    macro(LLVMBuilderRef, CreateBuilderInContext, (LLVMContextRef C)) \
    macro(LLVMBuilderRef, CreateBuilder, (void)) \
    macro(void, PositionBuilder, (LLVMBuilderRef Builder, LLVMBasicBlockRef Block, LLVMValueRef Instr)) \
    macro(void, PositionBuilderBefore, (LLVMBuilderRef Builder, LLVMValueRef Instr)) \
    macro(void, PositionBuilderAtEnd, (LLVMBuilderRef Builder, LLVMBasicBlockRef Block)) \
    macro(LLVMBasicBlockRef, GetInsertBlock, (LLVMBuilderRef Builder)) \
    macro(void, ClearInsertionPosition, (LLVMBuilderRef Builder)) \
    macro(void, InsertIntoBuilder, (LLVMBuilderRef Builder, LLVMValueRef Instr)) \
    macro(void, InsertIntoBuilderWithName, (LLVMBuilderRef Builder, LLVMValueRef Instr, const char *Name)) \
    macro(void, DisposeBuilder, (LLVMBuilderRef Builder)) \
    macro(void, SetCurrentDebugLocation, (LLVMBuilderRef Builder, LLVMValueRef L)) \
    macro(LLVMValueRef, GetCurrentDebugLocation, (LLVMBuilderRef Builder)) \
    macro(void, SetInstDebugLocation, (LLVMBuilderRef Builder, LLVMValueRef Inst)) \
    macro(LLVMValueRef, BuildRetVoid, (LLVMBuilderRef)) \
    macro(LLVMValueRef, BuildRet, (LLVMBuilderRef, LLVMValueRef V)) \
    macro(LLVMValueRef, BuildAggregateRet, (LLVMBuilderRef, LLVMValueRef *RetVals, unsigned N)) \
    macro(LLVMValueRef, BuildBr, (LLVMBuilderRef, LLVMBasicBlockRef Dest)) \
    macro(LLVMValueRef, BuildCondBr, (LLVMBuilderRef, LLVMValueRef If, LLVMBasicBlockRef Then, LLVMBasicBlockRef Else)) \
    macro(LLVMValueRef, BuildSwitch, (LLVMBuilderRef, LLVMValueRef V, LLVMBasicBlockRef Else, unsigned NumCases)) \
    macro(LLVMValueRef, BuildIndirectBr, (LLVMBuilderRef B, LLVMValueRef Addr, unsigned NumDests)) \
    macro(LLVMValueRef, BuildInvoke, (LLVMBuilderRef, LLVMValueRef Fn, LLVMValueRef *Args, unsigned NumArgs, LLVMBasicBlockRef Then, LLVMBasicBlockRef Catch, const char *Name)) \
    macro(LLVMValueRef, BuildLandingPad, (LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef PersFn, unsigned NumClauses, const char *Name)) \
    macro(LLVMValueRef, BuildResume, (LLVMBuilderRef B, LLVMValueRef Exn)) \
    macro(LLVMValueRef, BuildUnreachable, (LLVMBuilderRef)) \
    macro(void, AddCase, (LLVMValueRef Switch, LLVMValueRef OnVal, LLVMBasicBlockRef Dest)) \
    macro(void, AddDestination, (LLVMValueRef IndirectBr, LLVMBasicBlockRef Dest)) \
    macro(void, AddClause, (LLVMValueRef LandingPad, LLVMValueRef ClauseVal)) \
    macro(void, SetCleanup, (LLVMValueRef LandingPad, LLVMBool Val)) \
    macro(LLVMValueRef, BuildAdd, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildNSWAdd, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildNUWAdd, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildFAdd, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildSub, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildNSWSub, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildNUWSub, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildFSub, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildMul, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildNSWMul, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildNUWMul, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildFMul, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildUDiv, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildSDiv, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildExactSDiv, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildFDiv, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildURem, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildSRem, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildFRem, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildShl, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildLShr, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildAShr, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildAnd, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildOr, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildXor, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildBinOp, (LLVMBuilderRef B, LLVMOpcode Op, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildNeg, (LLVMBuilderRef, LLVMValueRef V, const char *Name)) \
    macro(LLVMValueRef, BuildNSWNeg, (LLVMBuilderRef B, LLVMValueRef V, const char *Name)) \
    macro(LLVMValueRef, BuildNUWNeg, (LLVMBuilderRef B, LLVMValueRef V, const char *Name)) \
    macro(LLVMValueRef, BuildFNeg, (LLVMBuilderRef, LLVMValueRef V, const char *Name)) \
    macro(LLVMValueRef, BuildNot, (LLVMBuilderRef, LLVMValueRef V, const char *Name)) \
    macro(LLVMValueRef, BuildMalloc, (LLVMBuilderRef, LLVMTypeRef Ty, const char *Name)) \
    macro(LLVMValueRef, BuildArrayMalloc, (LLVMBuilderRef, LLVMTypeRef Ty, LLVMValueRef Val, const char *Name)) \
    macro(LLVMValueRef, BuildAlloca, (LLVMBuilderRef, LLVMTypeRef Ty, const char *Name)) \
    macro(LLVMValueRef, BuildArrayAlloca, (LLVMBuilderRef, LLVMTypeRef Ty, LLVMValueRef Val, const char *Name)) \
    macro(LLVMValueRef, BuildFree, (LLVMBuilderRef, LLVMValueRef PointerVal)) \
    macro(LLVMValueRef, BuildLoad, (LLVMBuilderRef, LLVMValueRef PointerVal, const char *Name)) \
    macro(LLVMValueRef, BuildStore, (LLVMBuilderRef, LLVMValueRef Val, LLVMValueRef Ptr)) \
    macro(LLVMValueRef, BuildGEP, (LLVMBuilderRef B, LLVMValueRef Pointer, LLVMValueRef *Indices, unsigned NumIndices, const char *Name)) \
    macro(LLVMValueRef, BuildInBoundsGEP, (LLVMBuilderRef B, LLVMValueRef Pointer, LLVMValueRef *Indices, unsigned NumIndices, const char *Name)) \
    macro(LLVMValueRef, BuildStructGEP, (LLVMBuilderRef B, LLVMValueRef Pointer, unsigned Idx, const char *Name)) \
    macro(LLVMValueRef, BuildGlobalString, (LLVMBuilderRef B, const char *Str, const char *Name)) \
    macro(LLVMValueRef, BuildGlobalStringPtr, (LLVMBuilderRef B, const char *Str, const char *Name)) \
    macro(LLVMBool, GetVolatile, (LLVMValueRef MemoryAccessInst)) \
    macro(void, SetVolatile, (LLVMValueRef MemoryAccessInst, LLVMBool IsVolatile)) \
    macro(LLVMValueRef, BuildTrunc, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildZExt, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildSExt, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildFPToUI, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildFPToSI, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildUIToFP, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildSIToFP, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildFPTrunc, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildFPExt, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildPtrToInt, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildIntToPtr, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildBitCast, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildZExtOrBitCast, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildSExtOrBitCast, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildTruncOrBitCast, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildCast, (LLVMBuilderRef B, LLVMOpcode Op, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildPointerCast, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildIntCast, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildFPCast, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildICmp, (LLVMBuilderRef, LLVMIntPredicate Op, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildFCmp, (LLVMBuilderRef, LLVMRealPredicate Op, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildPhi, (LLVMBuilderRef, LLVMTypeRef Ty, const char *Name)) \
    macro(LLVMValueRef, BuildCall, (LLVMBuilderRef, LLVMValueRef Fn, LLVMValueRef *Args, unsigned NumArgs, const char *Name)) \
    macro(LLVMValueRef, BuildSelect, (LLVMBuilderRef, LLVMValueRef If, LLVMValueRef Then, LLVMValueRef Else, const char *Name)) \
    macro(LLVMValueRef, BuildVAArg, (LLVMBuilderRef, LLVMValueRef List, LLVMTypeRef Ty, const char *Name)) \
    macro(LLVMValueRef, BuildExtractElement, (LLVMBuilderRef, LLVMValueRef VecVal, LLVMValueRef Index, const char *Name)) \
    macro(LLVMValueRef, BuildInsertElement, (LLVMBuilderRef, LLVMValueRef VecVal, LLVMValueRef EltVal, LLVMValueRef Index, const char *Name)) \
    macro(LLVMValueRef, BuildShuffleVector, (LLVMBuilderRef, LLVMValueRef V1, LLVMValueRef V2, LLVMValueRef Mask, const char *Name)) \
    macro(LLVMValueRef, BuildExtractValue, (LLVMBuilderRef, LLVMValueRef AggVal, unsigned Index, const char *Name)) \
    macro(LLVMValueRef, BuildInsertValue, (LLVMBuilderRef, LLVMValueRef AggVal, LLVMValueRef EltVal, unsigned Index, const char *Name)) \
    macro(LLVMValueRef, BuildIsNull, (LLVMBuilderRef, LLVMValueRef Val, const char *Name)) \
    macro(LLVMValueRef, BuildIsNotNull, (LLVMBuilderRef, LLVMValueRef Val, const char *Name)) \
    macro(LLVMValueRef, BuildPtrDiff, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildFence, (LLVMBuilderRef B, LLVMAtomicOrdering Ordering, LLVMBool isSingleThread, const char *Name)) \
    macro(LLVMValueRef, BuildAtomicRMW, (LLVMBuilderRef B, LLVMAtomicRMWBinOp op, LLVMValueRef PTR, LLVMValueRef Val, LLVMAtomicOrdering ordering, LLVMBool singleThread)) \
    macro(LLVMModuleProviderRef, CreateModuleProviderForExistingModule, (LLVMModuleRef M)) \
    macro(void, DisposeModuleProvider, (LLVMModuleProviderRef M)) \
    macro(LLVMBool, CreateMemoryBufferWithContentsOfFile, (const char *Path, LLVMMemoryBufferRef *OutMemBuf, char **OutMessage)) \
    macro(LLVMBool, CreateMemoryBufferWithSTDIN, (LLVMMemoryBufferRef *OutMemBuf, char **OutMessage)) \
    macro(LLVMMemoryBufferRef, CreateMemoryBufferWithMemoryRange, (const char *InputData, size_t InputDataLength, const char *BufferName, LLVMBool RequiresNullTerminator)) \
    macro(LLVMMemoryBufferRef, CreateMemoryBufferWithMemoryRangeCopy, (const char *InputData, size_t InputDataLength, const char *BufferName)) \
    macro(const char *, GetBufferStart, (LLVMMemoryBufferRef MemBuf)) \
    macro(size_t, GetBufferSize, (LLVMMemoryBufferRef MemBuf)) \
    macro(void, DisposeMemoryBuffer, (LLVMMemoryBufferRef MemBuf)) \
    macro(LLVMPassRegistryRef, GetGlobalPassRegistry, (void)) \
    macro(LLVMPassManagerRef, CreatePassManager, (void)) \
    macro(LLVMPassManagerRef, CreateFunctionPassManagerForModule, (LLVMModuleRef M)) \
    macro(LLVMPassManagerRef, CreateFunctionPassManager, (LLVMModuleProviderRef MP)) \
    macro(LLVMBool, RunPassManager, (LLVMPassManagerRef PM, LLVMModuleRef M)) \
    macro(LLVMBool, InitializeFunctionPassManager, (LLVMPassManagerRef FPM)) \
    macro(LLVMBool, RunFunctionPassManager, (LLVMPassManagerRef FPM, LLVMValueRef F)) \
    macro(LLVMBool, FinalizeFunctionPassManager, (LLVMPassManagerRef FPM)) \
    macro(void, DisposePassManager, (LLVMPassManagerRef PM)) \
    macro(LLVMBool, StartMultithreaded, ()) \
    macro(void, StopMultithreaded, ()) \
    macro(LLVMBool, IsMultithreaded, ()) \
    macro(LLVMTargetDataRef, CreateTargetData, (const char *StringRep)) \
    macro(void, AddTargetData, (LLVMTargetDataRef, LLVMPassManagerRef)) \
    macro(void, AddTargetLibraryInfo, (LLVMTargetLibraryInfoRef, LLVMPassManagerRef)) \
    macro(char *, CopyStringRepOfTargetData, (LLVMTargetDataRef)) \
    macro(enum LLVMByteOrdering, ByteOrder, (LLVMTargetDataRef)) \
    macro(unsigned, PointerSize, (LLVMTargetDataRef)) \
    macro(LLVMTypeRef, IntPtrType, (LLVMTargetDataRef)) \
    macro(unsigned long long, SizeOfTypeInBits, (LLVMTargetDataRef, LLVMTypeRef)) \
    macro(unsigned long long, StoreSizeOfType, (LLVMTargetDataRef, LLVMTypeRef)) \
    macro(unsigned long long, ABISizeOfType, (LLVMTargetDataRef, LLVMTypeRef)) \
    macro(unsigned, ABIAlignmentOfType, (LLVMTargetDataRef, LLVMTypeRef)) \
    macro(unsigned, CallFrameAlignmentOfType, (LLVMTargetDataRef, LLVMTypeRef)) \
    macro(unsigned, PreferredAlignmentOfType, (LLVMTargetDataRef, LLVMTypeRef)) \
    macro(unsigned, PreferredAlignmentOfGlobal, (LLVMTargetDataRef, LLVMValueRef GlobalVar)) \
    macro(unsigned, ElementAtOffset, (LLVMTargetDataRef, LLVMTypeRef StructTy, unsigned long long Offset)) \
    macro(unsigned long long, OffsetOfElement, (LLVMTargetDataRef, LLVMTypeRef StructTy, unsigned Element)) \
    macro(void, DisposeTargetData, (LLVMTargetDataRef)) \
    macro(LLVMTargetRef, GetFirstTarget, ()) \
    macro(LLVMTargetRef, GetNextTarget, (LLVMTargetRef T)) \
    macro(const char *, GetTargetName, (LLVMTargetRef T)) \
    macro(const char *, GetTargetDescription, (LLVMTargetRef T)) \
    macro(LLVMBool, TargetHasJIT, (LLVMTargetRef T)) \
    macro(LLVMBool, TargetHasTargetMachine, (LLVMTargetRef T)) \
    macro(LLVMBool, TargetHasAsmBackend, (LLVMTargetRef T)) \
    macro(void, DisposeTargetMachine, (LLVMTargetMachineRef T)) \
    macro(LLVMTargetRef, GetTargetMachineTarget, (LLVMTargetMachineRef T)) \
    macro(char *, GetTargetMachineTriple, (LLVMTargetMachineRef T)) \
    macro(char *, GetTargetMachineCPU, (LLVMTargetMachineRef T)) \
    macro(char *, GetTargetMachineFeatureString, (LLVMTargetMachineRef T)) \
    macro(LLVMTargetDataRef, GetTargetMachineData, (LLVMTargetMachineRef T)) \
    macro(LLVMBool, TargetMachineEmitToFile, (LLVMTargetMachineRef T, LLVMModuleRef M, char *Filename, LLVMCodeGenFileType codegen, char **ErrorMessage)) \
    macro(void, LinkInJIT, (void)) \
    macro(void, LinkInMCJIT, (void)) \
    macro(void, LinkInInterpreter, (void)) \
    macro(LLVMGenericValueRef, CreateGenericValueOfInt, (LLVMTypeRef Ty, unsigned long long N, LLVMBool IsSigned)) \
    macro(LLVMGenericValueRef, CreateGenericValueOfPointer, (void *P)) \
    macro(LLVMGenericValueRef, CreateGenericValueOfFloat, (LLVMTypeRef Ty, double N)) \
    macro(unsigned, GenericValueIntWidth, (LLVMGenericValueRef GenValRef)) \
    macro(unsigned long long, GenericValueToInt, (LLVMGenericValueRef GenVal, LLVMBool IsSigned)) \
    macro(void *, GenericValueToPointer, (LLVMGenericValueRef GenVal)) \
    macro(double, GenericValueToFloat, (LLVMTypeRef TyRef, LLVMGenericValueRef GenVal)) \
    macro(void, DisposeGenericValue, (LLVMGenericValueRef GenVal)) \
    macro(LLVMBool, CreateExecutionEngineForModule, (LLVMExecutionEngineRef *OutEE, LLVMModuleRef M, char **OutError)) \
    macro(LLVMBool, CreateInterpreterForModule, (LLVMExecutionEngineRef *OutInterp, LLVMModuleRef M, char **OutError)) \
    macro(LLVMBool, CreateJITCompilerForModule, (LLVMExecutionEngineRef *OutJIT, LLVMModuleRef M, unsigned OptLevel, char **OutError)) \
    macro(void, InitializeMCJITCompilerOptions, (struct LLVMMCJITCompilerOptions *Options, size_t SizeOfOptions)) \
    macro(LLVMBool, CreateMCJITCompilerForModule, (LLVMExecutionEngineRef *OutJIT, LLVMModuleRef M, struct LLVMMCJITCompilerOptions *Options, size_t SizeOfOptions, char **OutError)) \
    macro(LLVMBool, CreateExecutionEngine, (LLVMExecutionEngineRef *OutEE, LLVMModuleProviderRef MP, char **OutError)) \
    macro(LLVMBool, CreateInterpreter, (LLVMExecutionEngineRef *OutInterp, LLVMModuleProviderRef MP, char **OutError)) \
    macro(LLVMBool, CreateJITCompiler, (LLVMExecutionEngineRef *OutJIT, LLVMModuleProviderRef MP, unsigned OptLevel, char **OutError)) \
    macro(void, DisposeExecutionEngine, (LLVMExecutionEngineRef EE)) \
    macro(void, RunStaticConstructors, (LLVMExecutionEngineRef EE)) \
    macro(void, RunStaticDestructors, (LLVMExecutionEngineRef EE)) \
    macro(int, RunFunctionAsMain, (LLVMExecutionEngineRef EE, LLVMValueRef F, unsigned ArgC, const char * const *ArgV, const char * const *EnvP)) \
    macro(LLVMGenericValueRef, RunFunction, (LLVMExecutionEngineRef EE, LLVMValueRef F, unsigned NumArgs, LLVMGenericValueRef *Args)) \
    macro(void, FreeMachineCodeForFunction, (LLVMExecutionEngineRef EE, LLVMValueRef F)) \
    macro(void, AddModule, (LLVMExecutionEngineRef EE, LLVMModuleRef M)) \
    macro(void, AddModuleProvider, (LLVMExecutionEngineRef EE, LLVMModuleProviderRef MP)) \
    macro(LLVMBool, RemoveModule, (LLVMExecutionEngineRef EE, LLVMModuleRef M, LLVMModuleRef *OutMod, char **OutError)) \
    macro(LLVMBool, RemoveModuleProvider, (LLVMExecutionEngineRef EE, LLVMModuleProviderRef MP, LLVMModuleRef *OutMod, char **OutError)) \
    macro(LLVMBool, FindFunction, (LLVMExecutionEngineRef EE, const char *Name, LLVMValueRef *OutFn)) \
    macro(void *, RecompileAndRelinkFunction, (LLVMExecutionEngineRef EE, LLVMValueRef Fn)) \
    macro(LLVMTargetDataRef, GetExecutionEngineTargetData, (LLVMExecutionEngineRef EE)) \
    macro(void, AddGlobalMapping, (LLVMExecutionEngineRef EE, LLVMValueRef Global, void* Addr)) \
    macro(void *, GetPointerToGlobal, (LLVMExecutionEngineRef EE, LLVMValueRef Global)) \
    macro(LLVMMCJITMemoryManagerRef, CreateSimpleMCJITMemoryManager, (void *Opaque, LLVMMemoryManagerAllocateCodeSectionCallback AllocateCodeSection, LLVMMemoryManagerAllocateDataSectionCallback AllocateDataSection, LLVMMemoryManagerFinalizeMemoryCallback FinalizeMemory, LLVMMemoryManagerDestroyCallback Destory)) \
    macro(void, DisposeMCJITMemoryManager, (LLVMMCJITMemoryManagerRef MM)) \
    macro(LLVMBool, VerifyModule, (LLVMModuleRef M, LLVMVerifierFailureAction Action, char **OutMessage)) \
    macro(LLVMBool, VerifyFunction, (LLVMValueRef Fn, LLVMVerifierFailureAction Action)) \
    macro(void, ViewFunctionCFG, (LLVMValueRef Fn)) \
    macro(void, ViewFunctionCFGOnly, (LLVMValueRef Fn)) \
    macro(LLVMDisasmContextRef, CreateDisasm, (const char *TripleName, void *DisInfo, int TagType, LLVMOpInfoCallback GetOpInfo, LLVMSymbolLookupCallback SymbolLookUp)) \
    macro(LLVMDisasmContextRef, CreateDisasmCPU, (const char *Triple, const char *CPU, void *DisInfo, int TagType, LLVMOpInfoCallback GetOpInfo, LLVMSymbolLookupCallback SymbolLookUp)) \
    macro(int, SetDisasmOptions, (LLVMDisasmContextRef DC, uint64_t Options)) \
    macro(void, DisasmDispose, (LLVMDisasmContextRef DC)) \
    macro(size_t, DisasmInstruction, (LLVMDisasmContextRef DC, uint8_t *Bytes, uint64_t BytesSize, uint64_t PC, char *OutString, size_t OutStringSize)) \
    macro(unsigned, PointerSizeForAS, (LLVMTargetDataRef, unsigned AS)) \
    macro(LLVMTypeRef, IntPtrTypeForAS, (LLVMTargetDataRef, unsigned AS)) \
    macro(LLVMPassManagerBuilderRef, PassManagerBuilderCreate, (void)) \
    macro(void, PassManagerBuilderDispose, (LLVMPassManagerBuilderRef PMB)) \
    macro(void, PassManagerBuilderSetOptLevel, (LLVMPassManagerBuilderRef PMB, unsigned OptLevel)) \
    macro(void, PassManagerBuilderSetSizeLevel, (LLVMPassManagerBuilderRef PMB, unsigned SizeLevel)) \
    macro(void, PassManagerBuilderSetDisableUnitAtATime, (LLVMPassManagerBuilderRef PMB, LLVMBool Value)) \
    macro(void, PassManagerBuilderSetDisableUnrollLoops, (LLVMPassManagerBuilderRef PMB, LLVMBool Value)) \
    macro(void, PassManagerBuilderSetDisableSimplifyLibCalls, (LLVMPassManagerBuilderRef PMB, LLVMBool Value)) \
    macro(void, PassManagerBuilderUseInlinerWithThreshold, (LLVMPassManagerBuilderRef PMB, unsigned Threshold)) \
    macro(void, PassManagerBuilderPopulateFunctionPassManager, (LLVMPassManagerBuilderRef PMB, LLVMPassManagerRef PM)) \
    macro(void, PassManagerBuilderPopulateModulePassManager, (LLVMPassManagerBuilderRef PMB, LLVMPassManagerRef PM)) \
    macro(void, PassManagerBuilderPopulateLTOPassManager, (LLVMPassManagerBuilderRef PMB, LLVMPassManagerRef PM, LLVMBool Internalize, LLVMBool RunInliner)) \
    macro(void, AddAggressiveDCEPass, (LLVMPassManagerRef PM)) \
    macro(void, AddCFGSimplificationPass, (LLVMPassManagerRef PM)) \
    macro(void, AddDeadStoreEliminationPass, (LLVMPassManagerRef PM)) \
    macro(void, AddGVNPass, (LLVMPassManagerRef PM)) \
    macro(void, AddIndVarSimplifyPass, (LLVMPassManagerRef PM)) \
    macro(void, AddInstructionCombiningPass, (LLVMPassManagerRef PM)) \
    macro(void, AddJumpThreadingPass, (LLVMPassManagerRef PM)) \
    macro(void, AddLICMPass, (LLVMPassManagerRef PM)) \
    macro(void, AddLoopDeletionPass, (LLVMPassManagerRef PM)) \
    macro(void, AddLoopIdiomPass, (LLVMPassManagerRef PM)) \
    macro(void, AddLoopRotatePass, (LLVMPassManagerRef PM)) \
    macro(void, AddLoopUnrollPass, (LLVMPassManagerRef PM)) \
    macro(void, AddLoopUnswitchPass, (LLVMPassManagerRef PM)) \
    macro(void, AddMemCpyOptPass, (LLVMPassManagerRef PM)) \
    macro(void, AddPartiallyInlineLibCallsPass, (LLVMPassManagerRef PM)) \
    macro(void, AddPromoteMemoryToRegisterPass, (LLVMPassManagerRef PM)) \
    macro(void, AddReassociatePass, (LLVMPassManagerRef PM)) \
    macro(void, AddSCCPPass, (LLVMPassManagerRef PM)) \
    macro(void, AddScalarReplAggregatesPass, (LLVMPassManagerRef PM)) \
    macro(void, AddScalarReplAggregatesPassSSA, (LLVMPassManagerRef PM)) \
    macro(void, AddScalarReplAggregatesPassWithThreshold, (LLVMPassManagerRef PM, int Threshold)) \
    macro(void, AddSimplifyLibCallsPass, (LLVMPassManagerRef PM)) \
    macro(void, AddTailCallEliminationPass, (LLVMPassManagerRef PM)) \
    macro(void, AddConstantPropagationPass, (LLVMPassManagerRef PM)) \
    macro(void, AddDemoteMemoryToRegisterPass, (LLVMPassManagerRef PM)) \
    macro(void, AddVerifierPass, (LLVMPassManagerRef PM)) \
    macro(void, AddCorrelatedValuePropagationPass, (LLVMPassManagerRef PM)) \
    macro(void, AddEarlyCSEPass, (LLVMPassManagerRef PM)) \
    macro(void, AddLowerExpectIntrinsicPass, (LLVMPassManagerRef PM)) \
    macro(void, AddTypeBasedAliasAnalysisPass, (LLVMPassManagerRef PM)) \
    macro(void, AddBasicAliasAnalysisPass, (LLVMPassManagerRef PM))

#endif // LLVMAPIFunctions_h

