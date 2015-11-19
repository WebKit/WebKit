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
    macro(void, DisposeMessage, (char *Message)) \
    macro(void, InstallFatalErrorHandler, (LLVMFatalErrorHandler Handler)) \
    macro(LLVMContextRef, ContextCreate, (void)) \
    macro(void, ContextDispose, (LLVMContextRef C)) \
    macro(unsigned, GetMDKindIDInContext, (LLVMContextRef C, const char* Name, unsigned SLen)) \
    macro(LLVMModuleRef, ModuleCreateWithNameInContext, (const char *ModuleID, LLVMContextRef C)) \
    macro(void, DisposeModule, (LLVMModuleRef M)) \
    macro(void, SetDataLayout, (LLVMModuleRef M, const char *Triple)) \
    macro(void, SetTarget, (LLVMModuleRef M, const char *Triple)) \
    macro(void, DumpModule, (LLVMModuleRef M)) \
    macro(LLVMContextRef, GetModuleContext, (LLVMModuleRef M)) \
    macro(LLVMTypeRef, GetTypeByName, (LLVMModuleRef M, const char *Name)) \
    macro(void, DumpType, (LLVMTypeRef Val)) \
    macro(LLVMValueRef, AddFunction, (LLVMModuleRef M, const char *Name, LLVMTypeRef FunctionTy)) \
    macro(LLVMValueRef, GetFirstFunction, (LLVMModuleRef M)) \
    macro(LLVMValueRef, GetNextFunction, (LLVMValueRef Fn)) \
    macro(LLVMTypeRef, Int1TypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, Int8TypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, Int16TypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, Int32TypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, Int64TypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, IntTypeInContext, (LLVMContextRef C, unsigned NumBits)) \
    macro(LLVMTypeRef, FloatTypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, DoubleTypeInContext, (LLVMContextRef C)) \
    macro(LLVMTypeRef, FunctionType, (LLVMTypeRef ReturnType, LLVMTypeRef *ParamTypes, unsigned ParamCount, LLVMBool IsVarArg)) \
    macro(LLVMTypeRef, StructTypeInContext, (LLVMContextRef C, LLVMTypeRef *ElementTypes, unsigned ElementCount, LLVMBool Packed)) \
    macro(LLVMTypeRef, ArrayType, (LLVMTypeRef ElementType, unsigned ElementCount)) \
    macro(LLVMTypeRef, PointerType, (LLVMTypeRef ElementType, unsigned AddressSpace)) \
    macro(LLVMTypeRef, VectorType, (LLVMTypeRef ElementType, unsigned ElementCount)) \
    macro(LLVMTypeRef, VoidTypeInContext, (LLVMContextRef C)) \
    macro(LLVMValueRef, ConstNull, (LLVMTypeRef Ty)) \
    macro(LLVMValueRef, GetUndef, (LLVMTypeRef Ty)) \
    macro(LLVMValueRef, ConstInt, (LLVMTypeRef IntTy, unsigned long long N, LLVMBool SignExtend)) \
    macro(LLVMValueRef, ConstReal, (LLVMTypeRef RealTy, double N)) \
    macro(void, SetLinkage, (LLVMValueRef Global, LLVMLinkage Linkage)) \
    macro(void, SetFunctionCallConv, (LLVMValueRef Fn, unsigned CC)) \
    macro(void, AddTargetDependentFunctionAttr, (LLVMValueRef Fn, const char *A, const char *V)) \
    macro(LLVMValueRef, GetParam, (LLVMValueRef Fn, unsigned Index)) \
    macro(LLVMValueRef, MDStringInContext, (LLVMContextRef C, const char *Str, unsigned SLen)) \
    macro(LLVMValueRef, MDString, (const char *Str, unsigned SLen)) \
    macro(LLVMValueRef, MDNodeInContext, (LLVMContextRef C, LLVMValueRef *Vals, unsigned Count)) \
    macro(LLVMValueRef, MDNode, (LLVMValueRef *Vals, unsigned Count)) \
    macro(LLVMBasicBlockRef, AppendBasicBlockInContext, (LLVMContextRef C, LLVMValueRef Fn, const char *Name)) \
    macro(LLVMBasicBlockRef, InsertBasicBlockInContext, (LLVMContextRef C, LLVMBasicBlockRef BB, const char *Name)) \
    macro(void, SetMetadata, (LLVMValueRef Val, unsigned KindID, LLVMValueRef Node)) \
    macro(void, SetInstructionCallConv, (LLVMValueRef Instr, unsigned CC)) \
    macro(void, AddIncoming, (LLVMValueRef PhiNode, LLVMValueRef *IncomingValues, LLVMBasicBlockRef *IncomingBlocks, unsigned Count)) \
    macro(LLVMBuilderRef, CreateBuilderInContext, (LLVMContextRef C)) \
    macro(void, PositionBuilderAtEnd, (LLVMBuilderRef Builder, LLVMBasicBlockRef Block)) \
    macro(void, DisposeBuilder, (LLVMBuilderRef Builder)) \
    macro(LLVMValueRef, BuildRet, (LLVMBuilderRef, LLVMValueRef V)) \
    macro(LLVMValueRef, BuildBr, (LLVMBuilderRef, LLVMBasicBlockRef Dest)) \
    macro(LLVMValueRef, BuildCondBr, (LLVMBuilderRef, LLVMValueRef If, LLVMBasicBlockRef Then, LLVMBasicBlockRef Else)) \
    macro(LLVMValueRef, BuildSwitch, (LLVMBuilderRef, LLVMValueRef V, LLVMBasicBlockRef Else, unsigned NumCases)) \
    macro(LLVMValueRef, BuildUnreachable, (LLVMBuilderRef)) \
    macro(void, AddCase, (LLVMValueRef Switch, LLVMValueRef OnVal, LLVMBasicBlockRef Dest)) \
    macro(LLVMValueRef, BuildAdd, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildFAdd, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildSub, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildFSub, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildMul, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildFMul, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildSDiv, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildFDiv, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildSRem, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildFRem, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildShl, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildLShr, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildAShr, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildAnd, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildOr, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildXor, (LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildNeg, (LLVMBuilderRef, LLVMValueRef V, const char *Name)) \
    macro(LLVMValueRef, BuildFNeg, (LLVMBuilderRef, LLVMValueRef V, const char *Name)) \
    macro(LLVMValueRef, BuildNot, (LLVMBuilderRef, LLVMValueRef V, const char *Name)) \
    macro(LLVMValueRef, BuildAlloca, (LLVMBuilderRef, LLVMTypeRef Ty, const char *Name)) \
    macro(LLVMValueRef, BuildLoad, (LLVMBuilderRef, LLVMValueRef PointerVal, const char *Name)) \
    macro(LLVMValueRef, BuildStore, (LLVMBuilderRef, LLVMValueRef Val, LLVMValueRef Ptr)) \
    macro(LLVMValueRef, BuildZExt, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildSExt, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildFPToUI, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildFPToSI, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildUIToFP, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildSIToFP, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildPtrToInt, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildIntToPtr, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildBitCast, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildIntCast, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildFPCast, (LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name)) \
    macro(LLVMValueRef, BuildICmp, (LLVMBuilderRef, LLVMIntPredicate Op, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildFCmp, (LLVMBuilderRef, LLVMRealPredicate Op, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)) \
    macro(LLVMValueRef, BuildPhi, (LLVMBuilderRef, LLVMTypeRef Ty, const char *Name)) \
    macro(LLVMValueRef, BuildCall, (LLVMBuilderRef, LLVMValueRef Fn, LLVMValueRef *Args, unsigned NumArgs, const char *Name)) \
    macro(LLVMValueRef, BuildSelect, (LLVMBuilderRef, LLVMValueRef If, LLVMValueRef Then, LLVMValueRef Else, const char *Name)) \
    macro(LLVMValueRef, BuildInsertElement, (LLVMBuilderRef, LLVMValueRef VecVal, LLVMValueRef EltVal, LLVMValueRef Index, const char *Name)) \
    macro(LLVMValueRef, BuildExtractValue, (LLVMBuilderRef, LLVMValueRef AggVal, unsigned Index, const char *Name)) \
    macro(LLVMValueRef, BuildFence, (LLVMBuilderRef B, LLVMAtomicOrdering Ordering, LLVMBool isSingleThread, const char *Name)) \
    macro(LLVMPassManagerRef, CreatePassManager, (void)) \
    macro(LLVMPassManagerRef, CreateFunctionPassManagerForModule, (LLVMModuleRef M)) \
    macro(LLVMPassManagerRef, CreateFunctionPassManager, (LLVMModuleProviderRef MP)) \
    macro(LLVMBool, RunPassManager, (LLVMPassManagerRef PM, LLVMModuleRef M)) \
    macro(LLVMBool, InitializeFunctionPassManager, (LLVMPassManagerRef FPM)) \
    macro(LLVMBool, RunFunctionPassManager, (LLVMPassManagerRef FPM, LLVMValueRef F)) \
    macro(LLVMBool, FinalizeFunctionPassManager, (LLVMPassManagerRef FPM)) \
    macro(void, DisposePassManager, (LLVMPassManagerRef PM)) \
    macro(LLVMBool, StartMultithreaded, ()) \
    macro(void, AddTargetData, (LLVMTargetDataRef, LLVMPassManagerRef)) \
    macro(char *, CopyStringRepOfTargetData, (LLVMTargetDataRef)) \
    macro(LLVMTypeRef, IntPtrType, (LLVMTargetDataRef)) \
    macro(void, LinkInMCJIT, (void)) \
    macro(void, InitializeMCJITCompilerOptions, (struct LLVMMCJITCompilerOptions *Options, size_t SizeOfOptions)) \
    macro(LLVMBool, CreateMCJITCompilerForModule, (LLVMExecutionEngineRef *OutJIT, LLVMModuleRef M, struct LLVMMCJITCompilerOptions *Options, size_t SizeOfOptions, char **OutError)) \
    macro(void, DisposeExecutionEngine, (LLVMExecutionEngineRef EE)) \
    macro(LLVMTargetDataRef, GetExecutionEngineTargetData, (LLVMExecutionEngineRef EE)) \
    macro(LLVMTargetMachineRef, GetExecutionEngineTargetMachine, (LLVMExecutionEngineRef EE)) \
    macro(void *, GetPointerToGlobal, (LLVMExecutionEngineRef EE, LLVMValueRef Global)) \
    macro(LLVMMCJITMemoryManagerRef, CreateSimpleMCJITMemoryManager, (void *Opaque, LLVMMemoryManagerAllocateCodeSectionCallback AllocateCodeSection, LLVMMemoryManagerAllocateDataSectionCallback AllocateDataSection, LLVMMemoryManagerFinalizeMemoryCallback FinalizeMemory, LLVMMemoryManagerDestroyCallback Destory)) \
    macro(LLVMBool, VerifyModule, (LLVMModuleRef M, LLVMVerifierFailureAction Action, char **OutMessage)) \
    macro(LLVMDisasmContextRef, CreateDisasm, (const char *TripleName, void *DisInfo, int TagType, LLVMOpInfoCallback GetOpInfo, LLVMSymbolLookupCallback SymbolLookUp)) \
    macro(void, DisasmDispose, (LLVMDisasmContextRef DC)) \
    macro(size_t, DisasmInstruction, (LLVMDisasmContextRef DC, uint8_t *Bytes, uint64_t BytesSize, uint64_t PC, char *OutString, size_t OutStringSize)) \
    macro(LLVMPassManagerBuilderRef, PassManagerBuilderCreate, (void)) \
    macro(void, PassManagerBuilderDispose, (LLVMPassManagerBuilderRef PMB)) \
    macro(void, PassManagerBuilderSetOptLevel, (LLVMPassManagerBuilderRef PMB, unsigned OptLevel)) \
    macro(void, PassManagerBuilderSetSizeLevel, (LLVMPassManagerBuilderRef PMB, unsigned SizeLevel)) \
    macro(void, PassManagerBuilderUseInlinerWithThreshold, (LLVMPassManagerBuilderRef PMB, unsigned Threshold)) \
    macro(void, PassManagerBuilderPopulateFunctionPassManager, (LLVMPassManagerBuilderRef PMB, LLVMPassManagerRef PM)) \
    macro(void, PassManagerBuilderPopulateModulePassManager, (LLVMPassManagerBuilderRef PMB, LLVMPassManagerRef PM)) \
    macro(void, AddAnalysisPasses, (LLVMTargetMachineRef T, LLVMPassManagerRef PM)) \
    macro(void, AddAggressiveDCEPass, (LLVMPassManagerRef PM)) \
    macro(void, AddCFGSimplificationPass, (LLVMPassManagerRef PM)) \
    macro(void, AddDeadStoreEliminationPass, (LLVMPassManagerRef PM)) \
    macro(void, AddFunctionInliningPass, (LLVMPassManagerRef PM)) \
    macro(void, AddGlobalDCEPass, (LLVMPassManagerRef PM)) \
    macro(void, AddPruneEHPass, (LLVMPassManagerRef PM)) \
    macro(void, AddGlobalOptimizerPass, (LLVMPassManagerRef PM)) \
    macro(void, AddGVNPass, (LLVMPassManagerRef PM)) \
    macro(void, AddInstructionCombiningPass, (LLVMPassManagerRef PM)) \
    macro(void, AddPromoteMemoryToRegisterPass, (LLVMPassManagerRef PM)) \
    macro(void, AddConstantPropagationPass, (LLVMPassManagerRef PM)) \
    macro(void, AddTypeBasedAliasAnalysisPass, (LLVMPassManagerRef PM)) \
    macro(void, AddBasicAliasAnalysisPass, (LLVMPassManagerRef PM))

#endif // LLVMAPIFunctions_h

