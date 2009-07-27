/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JIT.h"

// This probably does not belong here; adding here for now as a quick Windows build fix.
#if ENABLE(ASSEMBLER) && PLATFORM(X86) && !PLATFORM(MAC)
#include "MacroAssembler.h"
JSC::MacroAssemblerX86Common::SSE2CheckState JSC::MacroAssemblerX86Common::s_sse2CheckState = NotCheckedSSE2;
#endif

#if ENABLE(JIT)

#include "CodeBlock.h"
#include "Interpreter.h"
#include "JITInlineMethods.h"
#include "JITStubCall.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "LinkBuffer.h"
#include "RepatchBuffer.h"
#include "ResultType.h"
#include "SamplingTool.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

using namespace std;

namespace JSC {

void ctiPatchNearCallByReturnAddress(CodeBlock* codeblock, ReturnAddressPtr returnAddress, MacroAssemblerCodePtr newCalleeFunction)
{
    RepatchBuffer repatchBuffer(codeblock);
    repatchBuffer.relinkNearCallerToTrampoline(returnAddress, newCalleeFunction);
}

void ctiPatchCallByReturnAddress(CodeBlock* codeblock, ReturnAddressPtr returnAddress, MacroAssemblerCodePtr newCalleeFunction)
{
    RepatchBuffer repatchBuffer(codeblock);
    repatchBuffer.relinkCallerToTrampoline(returnAddress, newCalleeFunction);
}

void ctiPatchCallByReturnAddress(CodeBlock* codeblock, ReturnAddressPtr returnAddress, FunctionPtr newCalleeFunction)
{
    RepatchBuffer repatchBuffer(codeblock);
    repatchBuffer.relinkCallerToFunction(returnAddress, newCalleeFunction);
}

JIT::JIT(JSGlobalData* globalData, CodeBlock* codeBlock)
    : m_interpreter(globalData->interpreter)
    , m_globalData(globalData)
    , m_codeBlock(codeBlock)
    , m_labels(codeBlock ? codeBlock->instructions().size() : 0)
    , m_propertyAccessCompilationInfo(codeBlock ? codeBlock->numberOfStructureStubInfos() : 0)
    , m_callStructureStubCompilationInfo(codeBlock ? codeBlock->numberOfCallLinkInfos() : 0)
    , m_bytecodeIndex((unsigned)-1)
    , m_lastResultBytecodeRegister(std::numeric_limits<int>::max())
    , m_jumpTargetsPosition(0)
{
}

void JIT::compileOpStrictEq(Instruction* currentInstruction, CompileOpStrictEqType type)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src1 = currentInstruction[2].u.operand;
    unsigned src2 = currentInstruction[3].u.operand;

    emitGetVirtualRegisters(src1, regT0, src2, regT1);

    // Jump to a slow case if either operand is a number, or if both are JSCell*s.
    move(regT0, regT2);
    orPtr(regT1, regT2);
    addSlowCase(emitJumpIfJSCell(regT2));
    addSlowCase(emitJumpIfImmediateNumber(regT2));

    if (type == OpStrictEq)
        set32(Equal, regT1, regT0, regT0);
    else
        set32(NotEqual, regT1, regT0, regT0);
    emitTagAsBoolImmediate(regT0);

    emitPutVirtualRegister(dst);
}

void JIT::emitTimeoutCheck()
{
    Jump skipTimeout = branchSub32(NonZero, Imm32(1), timeoutCheckRegister);
    JITStubCall(this, JITStubs::cti_timeout_check).call(timeoutCheckRegister);
    skipTimeout.link(this);

    killLastResultRegister();
}


#define NEXT_OPCODE(name) \
    m_bytecodeIndex += OPCODE_LENGTH(name); \
    break;

#define DEFINE_BINARY_OP(name) \
    case name: { \
        JITStubCall stubCall(this, JITStubs::cti_##name); \
        stubCall.addArgument(currentInstruction[2].u.operand, regT2); \
        stubCall.addArgument(currentInstruction[3].u.operand, regT2); \
        stubCall.call(currentInstruction[1].u.operand); \
        NEXT_OPCODE(name); \
    }

#define DEFINE_UNARY_OP(name) \
    case name: { \
        JITStubCall stubCall(this, JITStubs::cti_##name); \
        stubCall.addArgument(currentInstruction[2].u.operand, regT2); \
        stubCall.call(currentInstruction[1].u.operand); \
        NEXT_OPCODE(name); \
    }

#define DEFINE_OP(name) \
    case name: { \
        emit_##name(currentInstruction); \
        NEXT_OPCODE(name); \
    }

#define DEFINE_SLOWCASE_OP(name) \
    case name: { \
        emitSlow_##name(currentInstruction, iter); \
        NEXT_OPCODE(name); \
    }

void JIT::privateCompileMainPass()
{
    Instruction* instructionsBegin = m_codeBlock->instructions().begin();
    unsigned instructionCount = m_codeBlock->instructions().size();

    m_propertyAccessInstructionIndex = 0;
    m_globalResolveInfoIndex = 0;
    m_callLinkInfoIndex = 0;

    for (m_bytecodeIndex = 0; m_bytecodeIndex < instructionCount; ) {
        Instruction* currentInstruction = instructionsBegin + m_bytecodeIndex;
        ASSERT_WITH_MESSAGE(m_interpreter->isOpcode(currentInstruction->u.opcode), "privateCompileMainPass gone bad @ %d", m_bytecodeIndex);

#if ENABLE(OPCODE_SAMPLING)
        if (m_bytecodeIndex > 0) // Avoid the overhead of sampling op_enter twice.
            sampleInstruction(currentInstruction);
#endif

        if (m_labels[m_bytecodeIndex].isUsed())
            killLastResultRegister();
        
        m_labels[m_bytecodeIndex] = label();

        switch (m_interpreter->getOpcodeID(currentInstruction->u.opcode)) {
        DEFINE_BINARY_OP(op_del_by_val)
        DEFINE_BINARY_OP(op_div)
        DEFINE_BINARY_OP(op_in)
        DEFINE_BINARY_OP(op_less)
        DEFINE_BINARY_OP(op_lesseq)
        DEFINE_BINARY_OP(op_urshift)
        DEFINE_UNARY_OP(op_get_pnames)
        DEFINE_UNARY_OP(op_is_boolean)
        DEFINE_UNARY_OP(op_is_function)
        DEFINE_UNARY_OP(op_is_number)
        DEFINE_UNARY_OP(op_is_object)
        DEFINE_UNARY_OP(op_is_string)
        DEFINE_UNARY_OP(op_is_undefined)
        DEFINE_UNARY_OP(op_negate)
        DEFINE_UNARY_OP(op_typeof)

        DEFINE_OP(op_add)
        DEFINE_OP(op_bitand)
        DEFINE_OP(op_bitnot)
        DEFINE_OP(op_bitor)
        DEFINE_OP(op_bitxor)
        DEFINE_OP(op_call)
        DEFINE_OP(op_call_eval)
        DEFINE_OP(op_call_varargs)
        DEFINE_OP(op_catch)
        DEFINE_OP(op_construct)
        DEFINE_OP(op_construct_verify)
        DEFINE_OP(op_convert_this)
        DEFINE_OP(op_init_arguments)
        DEFINE_OP(op_create_arguments)
        DEFINE_OP(op_debug)
        DEFINE_OP(op_del_by_id)
        DEFINE_OP(op_end)
        DEFINE_OP(op_enter)
        DEFINE_OP(op_enter_with_activation)
        DEFINE_OP(op_eq)
        DEFINE_OP(op_eq_null)
        DEFINE_OP(op_get_by_id)
        DEFINE_OP(op_get_by_val)
        DEFINE_OP(op_get_global_var)
        DEFINE_OP(op_get_scoped_var)
        DEFINE_OP(op_instanceof)
        DEFINE_OP(op_jeq_null)
        DEFINE_OP(op_jfalse)
        DEFINE_OP(op_jmp)
        DEFINE_OP(op_jmp_scopes)
        DEFINE_OP(op_jneq_null)
        DEFINE_OP(op_jneq_ptr)
        DEFINE_OP(op_jnless)
        DEFINE_OP(op_jnlesseq)
        DEFINE_OP(op_jsr)
        DEFINE_OP(op_jtrue)
        DEFINE_OP(op_load_varargs)
        DEFINE_OP(op_loop)
        DEFINE_OP(op_loop_if_less)
        DEFINE_OP(op_loop_if_lesseq)
        DEFINE_OP(op_loop_if_true)
        DEFINE_OP(op_lshift)
        DEFINE_OP(op_method_check)
        DEFINE_OP(op_mod)
        DEFINE_OP(op_mov)
        DEFINE_OP(op_mul)
        DEFINE_OP(op_neq)
        DEFINE_OP(op_neq_null)
        DEFINE_OP(op_new_array)
        DEFINE_OP(op_new_error)
        DEFINE_OP(op_new_func)
        DEFINE_OP(op_new_func_exp)
        DEFINE_OP(op_new_object)
        DEFINE_OP(op_new_regexp)
        DEFINE_OP(op_next_pname)
        DEFINE_OP(op_not)
        DEFINE_OP(op_nstricteq)
        DEFINE_OP(op_pop_scope)
        DEFINE_OP(op_post_dec)
        DEFINE_OP(op_post_inc)
        DEFINE_OP(op_pre_dec)
        DEFINE_OP(op_pre_inc)
        DEFINE_OP(op_profile_did_call)
        DEFINE_OP(op_profile_will_call)
        DEFINE_OP(op_push_new_scope)
        DEFINE_OP(op_push_scope)
        DEFINE_OP(op_put_by_id)
        DEFINE_OP(op_put_by_index)
        DEFINE_OP(op_put_by_val)
        DEFINE_OP(op_put_getter)
        DEFINE_OP(op_put_global_var)
        DEFINE_OP(op_put_scoped_var)
        DEFINE_OP(op_put_setter)
        DEFINE_OP(op_resolve)
        DEFINE_OP(op_resolve_base)
        DEFINE_OP(op_resolve_func)
        DEFINE_OP(op_resolve_global)
        DEFINE_OP(op_resolve_skip)
        DEFINE_OP(op_resolve_with_base)
        DEFINE_OP(op_ret)
        DEFINE_OP(op_rshift)
        DEFINE_OP(op_sret)
        DEFINE_OP(op_strcat)
        DEFINE_OP(op_stricteq)
        DEFINE_OP(op_sub)
        DEFINE_OP(op_switch_char)
        DEFINE_OP(op_switch_imm)
        DEFINE_OP(op_switch_string)
        DEFINE_OP(op_tear_off_activation)
        DEFINE_OP(op_tear_off_arguments)
        DEFINE_OP(op_throw)
        DEFINE_OP(op_to_jsnumber)
        DEFINE_OP(op_to_primitive)

        case op_get_array_length:
        case op_get_by_id_chain:
        case op_get_by_id_generic:
        case op_get_by_id_proto:
        case op_get_by_id_proto_list:
        case op_get_by_id_self:
        case op_get_by_id_self_list:
        case op_get_string_length:
        case op_put_by_id_generic:
        case op_put_by_id_replace:
        case op_put_by_id_transition:
            ASSERT_NOT_REACHED();
        }
    }

    ASSERT(m_propertyAccessInstructionIndex == m_codeBlock->numberOfStructureStubInfos());
    ASSERT(m_callLinkInfoIndex == m_codeBlock->numberOfCallLinkInfos());

#ifndef NDEBUG
    // Reset this, in order to guard its use with ASSERTs.
    m_bytecodeIndex = (unsigned)-1;
#endif
}


void JIT::privateCompileLinkPass()
{
    unsigned jmpTableCount = m_jmpTable.size();
    for (unsigned i = 0; i < jmpTableCount; ++i)
        m_jmpTable[i].from.linkTo(m_labels[m_jmpTable[i].toBytecodeIndex], this);
    m_jmpTable.clear();
}

void JIT::privateCompileSlowCases()
{
    Instruction* instructionsBegin = m_codeBlock->instructions().begin();

    m_propertyAccessInstructionIndex = 0;
    m_callLinkInfoIndex = 0;

    for (Vector<SlowCaseEntry>::iterator iter = m_slowCases.begin(); iter != m_slowCases.end();) {
        // FIXME: enable peephole optimizations for slow cases when applicable
        killLastResultRegister();

        m_bytecodeIndex = iter->to;
#ifndef NDEBUG
        unsigned firstTo = m_bytecodeIndex;
#endif
        Instruction* currentInstruction = instructionsBegin + m_bytecodeIndex;

        switch (m_interpreter->getOpcodeID(currentInstruction->u.opcode)) {
        DEFINE_SLOWCASE_OP(op_add)
        DEFINE_SLOWCASE_OP(op_bitand)
        DEFINE_SLOWCASE_OP(op_bitnot)
        DEFINE_SLOWCASE_OP(op_bitor)
        DEFINE_SLOWCASE_OP(op_bitxor)
        DEFINE_SLOWCASE_OP(op_call)
        DEFINE_SLOWCASE_OP(op_call_eval)
        DEFINE_SLOWCASE_OP(op_call_varargs)
        DEFINE_SLOWCASE_OP(op_construct)
        DEFINE_SLOWCASE_OP(op_construct_verify)
        DEFINE_SLOWCASE_OP(op_convert_this)
        DEFINE_SLOWCASE_OP(op_eq)
        DEFINE_SLOWCASE_OP(op_get_by_id)
        DEFINE_SLOWCASE_OP(op_get_by_val)
        DEFINE_SLOWCASE_OP(op_instanceof)
        DEFINE_SLOWCASE_OP(op_jfalse)
        DEFINE_SLOWCASE_OP(op_jnless)
        DEFINE_SLOWCASE_OP(op_jnlesseq)
        DEFINE_SLOWCASE_OP(op_jtrue)
        DEFINE_SLOWCASE_OP(op_loop_if_less)
        DEFINE_SLOWCASE_OP(op_loop_if_lesseq)
        DEFINE_SLOWCASE_OP(op_loop_if_true)
        DEFINE_SLOWCASE_OP(op_lshift)
        DEFINE_SLOWCASE_OP(op_mod)
        DEFINE_SLOWCASE_OP(op_mul)
        DEFINE_SLOWCASE_OP(op_method_check)
        DEFINE_SLOWCASE_OP(op_neq)
        DEFINE_SLOWCASE_OP(op_not)
        DEFINE_SLOWCASE_OP(op_nstricteq)
        DEFINE_SLOWCASE_OP(op_post_dec)
        DEFINE_SLOWCASE_OP(op_post_inc)
        DEFINE_SLOWCASE_OP(op_pre_dec)
        DEFINE_SLOWCASE_OP(op_pre_inc)
        DEFINE_SLOWCASE_OP(op_put_by_id)
        DEFINE_SLOWCASE_OP(op_put_by_val)
        DEFINE_SLOWCASE_OP(op_rshift)
        DEFINE_SLOWCASE_OP(op_stricteq)
        DEFINE_SLOWCASE_OP(op_sub)
        DEFINE_SLOWCASE_OP(op_to_jsnumber)
        DEFINE_SLOWCASE_OP(op_to_primitive)
        default:
            ASSERT_NOT_REACHED();
        }

        ASSERT_WITH_MESSAGE(iter == m_slowCases.end() || firstTo != iter->to,"Not enough jumps linked in slow case codegen.");
        ASSERT_WITH_MESSAGE(firstTo == (iter - 1)->to, "Too many jumps linked in slow case codegen.");

        emitJumpSlowToHot(jump(), 0);
    }

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    ASSERT(m_propertyAccessInstructionIndex == m_codeBlock->numberOfStructureStubInfos());
#endif
    ASSERT(m_callLinkInfoIndex == m_codeBlock->numberOfCallLinkInfos());

#ifndef NDEBUG
    // Reset this, in order to guard its use with ASSERTs.
    m_bytecodeIndex = (unsigned)-1;
#endif
}

void JIT::privateCompile()
{
    sampleCodeBlock(m_codeBlock);
#if ENABLE(OPCODE_SAMPLING)
    sampleInstruction(m_codeBlock->instructions().begin());
#endif

    // Could use a pop_m, but would need to offset the following instruction if so.
    preserveReturnAddressAfterCall(regT2);
    emitPutToCallFrameHeader(regT2, RegisterFile::ReturnPC);

    Jump slowRegisterFileCheck;
    Label afterRegisterFileCheck;
    if (m_codeBlock->codeType() == FunctionCode) {
        // In the case of a fast linked call, we do not set this up in the caller.
        emitPutImmediateToCallFrameHeader(m_codeBlock, RegisterFile::CodeBlock);

        peek(regT0, OBJECT_OFFSETOF(JITStackFrame, registerFile) / sizeof (void*));
        addPtr(Imm32(m_codeBlock->m_numCalleeRegisters * sizeof(Register)), callFrameRegister, regT1);

        slowRegisterFileCheck = branchPtr(Above, regT1, Address(regT0, OBJECT_OFFSETOF(RegisterFile, m_end)));
        afterRegisterFileCheck = label();
    }

    privateCompileMainPass();
    privateCompileLinkPass();
    privateCompileSlowCases();

    if (m_codeBlock->codeType() == FunctionCode) {
        slowRegisterFileCheck.link(this);
        m_bytecodeIndex = 0;
        JITStubCall(this, JITStubs::cti_register_file_check).call();
#ifndef NDEBUG
        m_bytecodeIndex = (unsigned)-1; // Reset this, in order to guard its use with ASSERTs.
#endif
        jump(afterRegisterFileCheck);
    }

    ASSERT(m_jmpTable.isEmpty());

    LinkBuffer patchBuffer(this, m_globalData->executableAllocator.poolForSize(m_assembler.size()));

    // Translate vPC offsets into addresses in JIT generated code, for switch tables.
    for (unsigned i = 0; i < m_switches.size(); ++i) {
        SwitchRecord record = m_switches[i];
        unsigned bytecodeIndex = record.bytecodeIndex;

        if (record.type != SwitchRecord::String) {
            ASSERT(record.type == SwitchRecord::Immediate || record.type == SwitchRecord::Character); 
            ASSERT(record.jumpTable.simpleJumpTable->branchOffsets.size() == record.jumpTable.simpleJumpTable->ctiOffsets.size());

            record.jumpTable.simpleJumpTable->ctiDefault = patchBuffer.locationOf(m_labels[bytecodeIndex + 3 + record.defaultOffset]);

            for (unsigned j = 0; j < record.jumpTable.simpleJumpTable->branchOffsets.size(); ++j) {
                unsigned offset = record.jumpTable.simpleJumpTable->branchOffsets[j];
                record.jumpTable.simpleJumpTable->ctiOffsets[j] = offset ? patchBuffer.locationOf(m_labels[bytecodeIndex + 3 + offset]) : record.jumpTable.simpleJumpTable->ctiDefault;
            }
        } else {
            ASSERT(record.type == SwitchRecord::String);

            record.jumpTable.stringJumpTable->ctiDefault = patchBuffer.locationOf(m_labels[bytecodeIndex + 3 + record.defaultOffset]);

            StringJumpTable::StringOffsetTable::iterator end = record.jumpTable.stringJumpTable->offsetTable.end();            
            for (StringJumpTable::StringOffsetTable::iterator it = record.jumpTable.stringJumpTable->offsetTable.begin(); it != end; ++it) {
                unsigned offset = it->second.branchOffset;
                it->second.ctiOffset = offset ? patchBuffer.locationOf(m_labels[bytecodeIndex + 3 + offset]) : record.jumpTable.stringJumpTable->ctiDefault;
            }
        }
    }

    for (size_t i = 0; i < m_codeBlock->numberOfExceptionHandlers(); ++i) {
        HandlerInfo& handler = m_codeBlock->exceptionHandler(i);
        handler.nativeCode = patchBuffer.locationOf(m_labels[handler.target]);
    }

    for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
        if (iter->to)
            patchBuffer.link(iter->from, FunctionPtr(iter->to));
    }

    if (m_codeBlock->hasExceptionInfo()) {
        m_codeBlock->callReturnIndexVector().reserveCapacity(m_calls.size());
        for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter)
            m_codeBlock->callReturnIndexVector().append(CallReturnOffsetToBytecodeIndex(patchBuffer.returnAddressOffset(iter->from), iter->bytecodeIndex));
    }

    // Link absolute addresses for jsr
    for (Vector<JSRInfo>::iterator iter = m_jsrSites.begin(); iter != m_jsrSites.end(); ++iter)
        patchBuffer.patch(iter->storeLocation, patchBuffer.locationOf(iter->target).executableAddress());

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    for (unsigned i = 0; i < m_codeBlock->numberOfStructureStubInfos(); ++i) {
        StructureStubInfo& info = m_codeBlock->structureStubInfo(i);
        info.callReturnLocation = patchBuffer.locationOf(m_propertyAccessCompilationInfo[i].callReturnLocation);
        info.hotPathBegin = patchBuffer.locationOf(m_propertyAccessCompilationInfo[i].hotPathBegin);
    }
#endif
#if ENABLE(JIT_OPTIMIZE_CALL)
    for (unsigned i = 0; i < m_codeBlock->numberOfCallLinkInfos(); ++i) {
        CallLinkInfo& info = m_codeBlock->callLinkInfo(i);
        info.ownerCodeBlock = m_codeBlock;
        info.callReturnLocation = patchBuffer.locationOfNearCall(m_callStructureStubCompilationInfo[i].callReturnLocation);
        info.hotPathBegin = patchBuffer.locationOf(m_callStructureStubCompilationInfo[i].hotPathBegin);
        info.hotPathOther = patchBuffer.locationOfNearCall(m_callStructureStubCompilationInfo[i].hotPathOther);
    }
#endif
    unsigned methodCallCount = m_methodCallCompilationInfo.size();
    m_codeBlock->addMethodCallLinkInfos(methodCallCount);
    for (unsigned i = 0; i < methodCallCount; ++i) {
        MethodCallLinkInfo& info = m_codeBlock->methodCallLinkInfo(i);
        info.structureLabel = patchBuffer.locationOf(m_methodCallCompilationInfo[i].structureToCompare);
        info.callReturnLocation = m_codeBlock->structureStubInfo(m_methodCallCompilationInfo[i].propertyAccessIndex).callReturnLocation;
    }

    m_codeBlock->setJITCode(patchBuffer.finalizeCode());
}

void JIT::privateCompileCTIMachineTrampolines(RefPtr<ExecutablePool>* executablePool, JSGlobalData* globalData, CodePtr* ctiArrayLengthTrampoline, CodePtr* ctiStringLengthTrampoline, CodePtr* ctiVirtualCallPreLink, CodePtr* ctiVirtualCallLink, CodePtr* ctiVirtualCall, CodePtr* ctiNativeCallThunk)
{
#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    // (1) The first function provides fast property access for array length
    Label arrayLengthBegin = align();

    // Check eax is an array
    Jump array_failureCases1 = emitJumpIfNotJSCell(regT0);
    Jump array_failureCases2 = branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsArrayVPtr));

    // Checks out okay! - get the length from the storage
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSArray, m_storage)), regT0);
    load32(Address(regT0, OBJECT_OFFSETOF(ArrayStorage, m_length)), regT0);

    Jump array_failureCases3 = branch32(Above, regT0, Imm32(JSImmediate::maxImmediateInt));

    // regT0 contains a 64 bit value (is positive, is zero extended) so we don't need sign extend here.
    emitFastArithIntToImmNoCheck(regT0, regT0);

    ret();

    // (2) The second function provides fast property access for string length
    Label stringLengthBegin = align();

    // Check eax is a string
    Jump string_failureCases1 = emitJumpIfNotJSCell(regT0);
    Jump string_failureCases2 = branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsStringVPtr));

    // Checks out okay! - get the length from the Ustring.
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSString, m_value) + OBJECT_OFFSETOF(UString, m_rep)), regT0);
    load32(Address(regT0, OBJECT_OFFSETOF(UString::Rep, len)), regT0);

    Jump string_failureCases3 = branch32(Above, regT0, Imm32(JSImmediate::maxImmediateInt));

    // regT0 contains a 64 bit value (is positive, is zero extended) so we don't need sign extend here.
    emitFastArithIntToImmNoCheck(regT0, regT0);
    
    ret();
#endif

    // (3) Trampolines for the slow cases of op_call / op_call_eval / op_construct.
    COMPILE_ASSERT(sizeof(CodeType) == 4, CodeTypeEnumMustBe32Bit);

    Label virtualCallPreLinkBegin = align();

    // Load the callee CodeBlock* into eax
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSFunction, m_body)), regT3);
    loadPtr(Address(regT3, OBJECT_OFFSETOF(FunctionBodyNode, m_code)), regT0);
    Jump hasCodeBlock1 = branchTestPtr(NonZero, regT0);
    preserveReturnAddressAfterCall(regT3);
    restoreArgumentReference();
    Call callJSFunction1 = call();
    emitGetJITStubArg(1, regT2);
    emitGetJITStubArg(3, regT1);
    restoreReturnAddressBeforeReturn(regT3);
    hasCodeBlock1.link(this);

    Jump isNativeFunc1 = branch32(Equal, Address(regT0, OBJECT_OFFSETOF(CodeBlock, m_codeType)), Imm32(NativeCode));

    // Check argCount matches callee arity.
    Jump arityCheckOkay1 = branch32(Equal, Address(regT0, OBJECT_OFFSETOF(CodeBlock, m_numParameters)), regT1);
    preserveReturnAddressAfterCall(regT3);
    emitPutJITStubArg(regT3, 2);
    emitPutJITStubArg(regT0, 4);
    restoreArgumentReference();
    Call callArityCheck1 = call();
    move(regT1, callFrameRegister);
    emitGetJITStubArg(1, regT2);
    emitGetJITStubArg(3, regT1);
    restoreReturnAddressBeforeReturn(regT3);
    arityCheckOkay1.link(this);
    isNativeFunc1.link(this);
    
    compileOpCallInitializeCallFrame();

    preserveReturnAddressAfterCall(regT3);
    emitPutJITStubArg(regT3, 2);
    restoreArgumentReference();
    Call callDontLazyLinkCall = call();
    emitGetJITStubArg(1, regT2);
    restoreReturnAddressBeforeReturn(regT3);

    jump(regT0);

    Label virtualCallLinkBegin = align();

    // Load the callee CodeBlock* into eax
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSFunction, m_body)), regT3);
    loadPtr(Address(regT3, OBJECT_OFFSETOF(FunctionBodyNode, m_code)), regT0);
    Jump hasCodeBlock2 = branchTestPtr(NonZero, regT0);
    preserveReturnAddressAfterCall(regT3);
    restoreArgumentReference();
    Call callJSFunction2 = call();
    emitGetJITStubArg(1, regT2);
    emitGetJITStubArg(3, regT1);
    restoreReturnAddressBeforeReturn(regT3);
    hasCodeBlock2.link(this);

    Jump isNativeFunc2 = branch32(Equal, Address(regT0, OBJECT_OFFSETOF(CodeBlock, m_codeType)), Imm32(NativeCode));

    // Check argCount matches callee arity.
    Jump arityCheckOkay2 = branch32(Equal, Address(regT0, OBJECT_OFFSETOF(CodeBlock, m_numParameters)), regT1);
    preserveReturnAddressAfterCall(regT3);
    emitPutJITStubArg(regT3, 2);
    emitPutJITStubArg(regT0, 4);
    restoreArgumentReference();
    Call callArityCheck2 = call();
    move(regT1, callFrameRegister);
    emitGetJITStubArg(1, regT2);
    emitGetJITStubArg(3, regT1);
    restoreReturnAddressBeforeReturn(regT3);
    arityCheckOkay2.link(this);
    isNativeFunc2.link(this);

    compileOpCallInitializeCallFrame();

    preserveReturnAddressAfterCall(regT3);
    emitPutJITStubArg(regT3, 2);
    restoreArgumentReference();
    Call callLazyLinkCall = call();
    restoreReturnAddressBeforeReturn(regT3);

    jump(regT0);

    Label virtualCallBegin = align();

    // Load the callee CodeBlock* into eax
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSFunction, m_body)), regT3);
    loadPtr(Address(regT3, OBJECT_OFFSETOF(FunctionBodyNode, m_code)), regT0);
    Jump hasCodeBlock3 = branchTestPtr(NonZero, regT0);
    preserveReturnAddressAfterCall(regT3);
    restoreArgumentReference();
    Call callJSFunction3 = call();
    emitGetJITStubArg(1, regT2);
    emitGetJITStubArg(3, regT1);
    restoreReturnAddressBeforeReturn(regT3);
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSFunction, m_body)), regT3); // reload the function body nody, so we can reload the code pointer.
    hasCodeBlock3.link(this);
    
    Jump isNativeFunc3 = branch32(Equal, Address(regT0, OBJECT_OFFSETOF(CodeBlock, m_codeType)), Imm32(NativeCode));

    // Check argCount matches callee arity.
    Jump arityCheckOkay3 = branch32(Equal, Address(regT0, OBJECT_OFFSETOF(CodeBlock, m_numParameters)), regT1);
    preserveReturnAddressAfterCall(regT3);
    emitPutJITStubArg(regT3, 2);
    emitPutJITStubArg(regT0, 4);
    restoreArgumentReference();
    Call callArityCheck3 = call();
    move(regT1, callFrameRegister);
    emitGetJITStubArg(1, regT2);
    emitGetJITStubArg(3, regT1);
    restoreReturnAddressBeforeReturn(regT3);
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSFunction, m_body)), regT3); // reload the function body nody, so we can reload the code pointer.
    arityCheckOkay3.link(this);
    isNativeFunc3.link(this);

    // load ctiCode from the new codeBlock.
    loadPtr(Address(regT3, OBJECT_OFFSETOF(FunctionBodyNode, m_jitCode)), regT0);
    
    compileOpCallInitializeCallFrame();
    jump(regT0);

    
    Label nativeCallThunk = align();
    preserveReturnAddressAfterCall(regT0);
    emitPutToCallFrameHeader(regT0, RegisterFile::ReturnPC); // Push return address

    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, regT1);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT1, regT1);
    emitPutToCallFrameHeader(regT1, RegisterFile::ScopeChain);
    

#if PLATFORM(X86_64)
    emitGetFromCallFrameHeader32(RegisterFile::ArgumentCount, X86::ecx);

    // Allocate stack space for our arglist
    subPtr(Imm32(sizeof(ArgList)), stackPointerRegister);
    COMPILE_ASSERT((sizeof(ArgList) & 0xf) == 0, ArgList_should_by_16byte_aligned);
    
    // Set up arguments
    subPtr(Imm32(1), X86::ecx); // Don't include 'this' in argcount

    // Push argcount
    storePtr(X86::ecx, Address(stackPointerRegister, OBJECT_OFFSETOF(ArgList, m_argCount)));

    // Calculate the start of the callframe header, and store in edx
    addPtr(Imm32(-RegisterFile::CallFrameHeaderSize * (int32_t)sizeof(Register)), callFrameRegister, X86::edx);
    
    // Calculate start of arguments as callframe header - sizeof(Register) * argcount (ecx)
    mul32(Imm32(sizeof(Register)), X86::ecx, X86::ecx);
    subPtr(X86::ecx, X86::edx);

    // push pointer to arguments
    storePtr(X86::edx, Address(stackPointerRegister, OBJECT_OFFSETOF(ArgList, m_args)));
    
    // ArgList is passed by reference so is stackPointerRegister
    move(stackPointerRegister, X86::ecx);
    
    // edx currently points to the first argument, edx-sizeof(Register) points to 'this'
    loadPtr(Address(X86::edx, -(int32_t)sizeof(Register)), X86::edx);
    
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, X86::esi);

    move(callFrameRegister, X86::edi); 

    call(Address(X86::esi, OBJECT_OFFSETOF(JSFunction, m_data)));
    
    addPtr(Imm32(sizeof(ArgList)), stackPointerRegister);
#elif PLATFORM(X86)
    emitGetFromCallFrameHeader32(RegisterFile::ArgumentCount, regT0);

    /* We have two structs that we use to describe the stackframe we set up for our
     * call to native code.  NativeCallFrameStructure describes the how we set up the stack
     * in advance of the call.  NativeFunctionCalleeSignature describes the callframe
     * as the native code expects it.  We do this as we are using the fastcall calling
     * convention which results in the callee popping its arguments off the stack, but
     * not the rest of the callframe so we need a nice way to ensure we increment the
     * stack pointer by the right amount after the call.
     */
#if COMPILER(MSVC) || PLATFORM(LINUX)
    struct NativeCallFrameStructure {
      //  CallFrame* callFrame; // passed in EDX
        JSObject* callee;
        JSValue thisValue;
        ArgList* argPointer;
        ArgList args;
        JSValue result;
    };
    struct NativeFunctionCalleeSignature {
        JSObject* callee;
        JSValue thisValue;
        ArgList* argPointer;
    };
#else
    struct NativeCallFrameStructure {
      //  CallFrame* callFrame; // passed in ECX
      //  JSObject* callee; // passed in EDX
        JSValue thisValue;
        ArgList* argPointer;
        ArgList args;
    };
    struct NativeFunctionCalleeSignature {
        JSValue thisValue;
        ArgList* argPointer;
    };
#endif
    const int NativeCallFrameSize = (sizeof(NativeCallFrameStructure) + 15) & ~15;
    // Allocate system stack frame
    subPtr(Imm32(NativeCallFrameSize), stackPointerRegister);

    // Set up arguments
    subPtr(Imm32(1), regT0); // Don't include 'this' in argcount

    // push argcount
    storePtr(regT0, Address(stackPointerRegister, OBJECT_OFFSETOF(NativeCallFrameStructure, args) + OBJECT_OFFSETOF(ArgList, m_argCount)));
    
    // Calculate the start of the callframe header, and store in regT1
    addPtr(Imm32(-RegisterFile::CallFrameHeaderSize * (int)sizeof(Register)), callFrameRegister, regT1);
    
    // Calculate start of arguments as callframe header - sizeof(Register) * argcount (regT0)
    mul32(Imm32(sizeof(Register)), regT0, regT0);
    subPtr(regT0, regT1);
    storePtr(regT1, Address(stackPointerRegister, OBJECT_OFFSETOF(NativeCallFrameStructure, args) + OBJECT_OFFSETOF(ArgList, m_args)));

    // ArgList is passed by reference so is stackPointerRegister + 4 * sizeof(Register)
    addPtr(Imm32(OBJECT_OFFSETOF(NativeCallFrameStructure, args)), stackPointerRegister, regT0);
    storePtr(regT0, Address(stackPointerRegister, OBJECT_OFFSETOF(NativeCallFrameStructure, argPointer)));

    // regT1 currently points to the first argument, regT1 - sizeof(Register) points to 'this'
    loadPtr(Address(regT1, -(int)sizeof(Register)), regT1);
    storePtr(regT1, Address(stackPointerRegister, OBJECT_OFFSETOF(NativeCallFrameStructure, thisValue)));

#if COMPILER(MSVC) || PLATFORM(LINUX)
    // ArgList is passed by reference so is stackPointerRegister + 4 * sizeof(Register)
    addPtr(Imm32(OBJECT_OFFSETOF(NativeCallFrameStructure, result)), stackPointerRegister, X86::ecx);

    // Plant callee
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, X86::eax);
    storePtr(X86::eax, Address(stackPointerRegister, OBJECT_OFFSETOF(NativeCallFrameStructure, callee)));

    // Plant callframe
    move(callFrameRegister, X86::edx);

    call(Address(X86::eax, OBJECT_OFFSETOF(JSFunction, m_data)));

    // JSValue is a non-POD type
    loadPtr(Address(X86::eax), X86::eax);
#else
    // Plant callee
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, X86::edx);

    // Plant callframe
    move(callFrameRegister, X86::ecx);
    call(Address(X86::edx, OBJECT_OFFSETOF(JSFunction, m_data)));
#endif

    // We've put a few temporaries on the stack in addition to the actual arguments
    // so pull them off now
    addPtr(Imm32(NativeCallFrameSize - sizeof(NativeFunctionCalleeSignature)), stackPointerRegister);

#elif ENABLE(JIT_OPTIMIZE_NATIVE_CALL)
#error "JIT_OPTIMIZE_NATIVE_CALL not yet supported on this platform."
#else
    breakpoint();
#endif

    // Check for an exception
    loadPtr(&(globalData->exception), regT2);
    Jump exceptionHandler = branchTestPtr(NonZero, regT2);

    // Grab the return address.
    emitGetFromCallFrameHeaderPtr(RegisterFile::ReturnPC, regT1);
    
    // Restore our caller's "r".
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);
    
    // Return.
    restoreReturnAddressBeforeReturn(regT1);
    ret();

    // Handle an exception
    exceptionHandler.link(this);
    // Grab the return address.
    emitGetFromCallFrameHeaderPtr(RegisterFile::ReturnPC, regT1);
    move(ImmPtr(&globalData->exceptionLocation), regT2);
    storePtr(regT1, regT2);
    move(ImmPtr(reinterpret_cast<void*>(ctiVMThrowTrampoline)), regT2);
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);
    poke(callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof (void*));
    restoreReturnAddressBeforeReturn(regT2);
    ret();
    

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    Call array_failureCases1Call = makeTailRecursiveCall(array_failureCases1);
    Call array_failureCases2Call = makeTailRecursiveCall(array_failureCases2);
    Call array_failureCases3Call = makeTailRecursiveCall(array_failureCases3);
    Call string_failureCases1Call = makeTailRecursiveCall(string_failureCases1);
    Call string_failureCases2Call = makeTailRecursiveCall(string_failureCases2);
    Call string_failureCases3Call = makeTailRecursiveCall(string_failureCases3);
#endif

    // All trampolines constructed! copy the code, link up calls, and set the pointers on the Machine object.
    LinkBuffer patchBuffer(this, m_globalData->executableAllocator.poolForSize(m_assembler.size()));

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    patchBuffer.link(array_failureCases1Call, FunctionPtr(JITStubs::cti_op_get_by_id_array_fail));
    patchBuffer.link(array_failureCases2Call, FunctionPtr(JITStubs::cti_op_get_by_id_array_fail));
    patchBuffer.link(array_failureCases3Call, FunctionPtr(JITStubs::cti_op_get_by_id_array_fail));
    patchBuffer.link(string_failureCases1Call, FunctionPtr(JITStubs::cti_op_get_by_id_string_fail));
    patchBuffer.link(string_failureCases2Call, FunctionPtr(JITStubs::cti_op_get_by_id_string_fail));
    patchBuffer.link(string_failureCases3Call, FunctionPtr(JITStubs::cti_op_get_by_id_string_fail));
#endif
    patchBuffer.link(callArityCheck1, FunctionPtr(JITStubs::cti_op_call_arityCheck));
    patchBuffer.link(callArityCheck2, FunctionPtr(JITStubs::cti_op_call_arityCheck));
    patchBuffer.link(callArityCheck3, FunctionPtr(JITStubs::cti_op_call_arityCheck));
    patchBuffer.link(callJSFunction1, FunctionPtr(JITStubs::cti_op_call_JSFunction));
    patchBuffer.link(callJSFunction2, FunctionPtr(JITStubs::cti_op_call_JSFunction));
    patchBuffer.link(callJSFunction3, FunctionPtr(JITStubs::cti_op_call_JSFunction));
    patchBuffer.link(callDontLazyLinkCall, FunctionPtr(JITStubs::cti_vm_dontLazyLinkCall));
    patchBuffer.link(callLazyLinkCall, FunctionPtr(JITStubs::cti_vm_lazyLinkCall));

    CodeRef finalCode = patchBuffer.finalizeCode();
    *executablePool = finalCode.m_executablePool;

    *ctiVirtualCallPreLink = trampolineAt(finalCode, virtualCallPreLinkBegin);
    *ctiVirtualCallLink = trampolineAt(finalCode, virtualCallLinkBegin);
    *ctiVirtualCall = trampolineAt(finalCode, virtualCallBegin);
    *ctiNativeCallThunk = trampolineAt(finalCode, nativeCallThunk);
#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    *ctiArrayLengthTrampoline = trampolineAt(finalCode, arrayLengthBegin);
    *ctiStringLengthTrampoline = trampolineAt(finalCode, stringLengthBegin);
#else
    UNUSED_PARAM(ctiArrayLengthTrampoline);
    UNUSED_PARAM(ctiStringLengthTrampoline);
#endif
}

void JIT::emitGetVariableObjectRegister(RegisterID variableObject, int index, RegisterID dst)
{
    loadPtr(Address(variableObject, OBJECT_OFFSETOF(JSVariableObject, d)), dst);
    loadPtr(Address(dst, OBJECT_OFFSETOF(JSVariableObject::JSVariableObjectData, registers)), dst);
    loadPtr(Address(dst, index * sizeof(Register)), dst);
}

void JIT::emitPutVariableObjectRegister(RegisterID src, RegisterID variableObject, int index)
{
    loadPtr(Address(variableObject, OBJECT_OFFSETOF(JSVariableObject, d)), variableObject);
    loadPtr(Address(variableObject, OBJECT_OFFSETOF(JSVariableObject::JSVariableObjectData, registers)), variableObject);
    storePtr(src, Address(variableObject, index * sizeof(Register)));
}

void JIT::unlinkCall(CallLinkInfo* callLinkInfo)
{
    // When the JSFunction is deleted the pointer embedded in the instruction stream will no longer be valid
    // (and, if a new JSFunction happened to be constructed at the same location, we could get a false positive
    // match).  Reset the check so it no longer matches.
    RepatchBuffer repatchBuffer(callLinkInfo->ownerCodeBlock);
    repatchBuffer.repatch(callLinkInfo->hotPathBegin, JSValue::encode(JSValue()));
}

void JIT::linkCall(JSFunction* callee, CodeBlock* callerCodeBlock, CodeBlock* calleeCodeBlock, JITCode& code, CallLinkInfo* callLinkInfo, int callerArgCount, JSGlobalData* globalData)
{
    ASSERT(calleeCodeBlock);
    RepatchBuffer repatchBuffer(callerCodeBlock);

    // Currently we only link calls with the exact number of arguments.
    // If this is a native call calleeCodeBlock is null so the number of parameters is unimportant
    if (callerArgCount == calleeCodeBlock->m_numParameters || calleeCodeBlock->codeType() == NativeCode) {
        ASSERT(!callLinkInfo->isLinked());
    
        if (calleeCodeBlock)
            calleeCodeBlock->addCaller(callLinkInfo);
    
        repatchBuffer.repatch(callLinkInfo->hotPathBegin, callee);
        repatchBuffer.relink(callLinkInfo->hotPathOther, code.addressForCall());
    }

    // patch the call so we do not continue to try to link.
    repatchBuffer.relink(callLinkInfo->callReturnLocation, globalData->jitStubs.ctiVirtualCall());
}

} // namespace JSC

#endif // ENABLE(JIT)
