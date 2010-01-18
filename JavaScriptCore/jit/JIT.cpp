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
#if ENABLE(ASSEMBLER) && CPU(X86) && !OS(MAC_OS_X)
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
#if USE(JSVALUE32_64)
    , m_jumpTargetIndex(0)
    , m_mappedBytecodeIndex((unsigned)-1)
    , m_mappedVirtualRegisterIndex((unsigned)-1)
    , m_mappedTag((RegisterID)-1)
    , m_mappedPayload((RegisterID)-1)
#else
    , m_lastResultBytecodeRegister(std::numeric_limits<int>::max())
    , m_jumpTargetsPosition(0)
#endif
{
}

#if USE(JSVALUE32_64)
void JIT::emitTimeoutCheck()
{
    Jump skipTimeout = branchSub32(NonZero, Imm32(1), timeoutCheckRegister);
    JITStubCall stubCall(this, cti_timeout_check);
    stubCall.addArgument(regT1, regT0); // save last result registers.
    stubCall.call(timeoutCheckRegister);
    stubCall.getArgument(0, regT1, regT0); // reload last result registers.
    skipTimeout.link(this);
}
#else
void JIT::emitTimeoutCheck()
{
    Jump skipTimeout = branchSub32(NonZero, Imm32(1), timeoutCheckRegister);
    JITStubCall(this, cti_timeout_check).call(timeoutCheckRegister);
    skipTimeout.link(this);

    killLastResultRegister();
}
#endif

#define NEXT_OPCODE(name) \
    m_bytecodeIndex += OPCODE_LENGTH(name); \
    break;

#if USE(JSVALUE32_64)
#define DEFINE_BINARY_OP(name) \
    case name: { \
        JITStubCall stubCall(this, cti_##name); \
        stubCall.addArgument(currentInstruction[2].u.operand); \
        stubCall.addArgument(currentInstruction[3].u.operand); \
        stubCall.call(currentInstruction[1].u.operand); \
        NEXT_OPCODE(name); \
    }

#define DEFINE_UNARY_OP(name) \
    case name: { \
        JITStubCall stubCall(this, cti_##name); \
        stubCall.addArgument(currentInstruction[2].u.operand); \
        stubCall.call(currentInstruction[1].u.operand); \
        NEXT_OPCODE(name); \
    }

#else // USE(JSVALUE32_64)

#define DEFINE_BINARY_OP(name) \
    case name: { \
        JITStubCall stubCall(this, cti_##name); \
        stubCall.addArgument(currentInstruction[2].u.operand, regT2); \
        stubCall.addArgument(currentInstruction[3].u.operand, regT2); \
        stubCall.call(currentInstruction[1].u.operand); \
        NEXT_OPCODE(name); \
    }

#define DEFINE_UNARY_OP(name) \
    case name: { \
        JITStubCall stubCall(this, cti_##name); \
        stubCall.addArgument(currentInstruction[2].u.operand, regT2); \
        stubCall.call(currentInstruction[1].u.operand); \
        NEXT_OPCODE(name); \
    }
#endif // USE(JSVALUE32_64)

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

#if !USE(JSVALUE32_64)
        if (m_labels[m_bytecodeIndex].isUsed())
            killLastResultRegister();
#endif

        m_labels[m_bytecodeIndex] = label();

        switch (m_interpreter->getOpcodeID(currentInstruction->u.opcode)) {
        DEFINE_BINARY_OP(op_del_by_val)
#if USE(JSVALUE32)
        DEFINE_BINARY_OP(op_div)
#endif
        DEFINE_BINARY_OP(op_in)
        DEFINE_BINARY_OP(op_less)
        DEFINE_BINARY_OP(op_lesseq)
        DEFINE_BINARY_OP(op_urshift)
        DEFINE_UNARY_OP(op_is_boolean)
        DEFINE_UNARY_OP(op_is_function)
        DEFINE_UNARY_OP(op_is_number)
        DEFINE_UNARY_OP(op_is_object)
        DEFINE_UNARY_OP(op_is_string)
        DEFINE_UNARY_OP(op_is_undefined)
#if !USE(JSVALUE32_64)
        DEFINE_UNARY_OP(op_negate)
#endif
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
#if !USE(JSVALUE32)
        DEFINE_OP(op_div)
#endif
        DEFINE_OP(op_end)
        DEFINE_OP(op_enter)
        DEFINE_OP(op_enter_with_activation)
        DEFINE_OP(op_eq)
        DEFINE_OP(op_eq_null)
        DEFINE_OP(op_get_by_id)
        DEFINE_OP(op_get_by_val)
        DEFINE_OP(op_get_by_pname)
        DEFINE_OP(op_get_global_var)
        DEFINE_OP(op_get_pnames)
        DEFINE_OP(op_get_scoped_var)
        DEFINE_OP(op_instanceof)
        DEFINE_OP(op_jeq_null)
        DEFINE_OP(op_jfalse)
        DEFINE_OP(op_jmp)
        DEFINE_OP(op_jmp_scopes)
        DEFINE_OP(op_jneq_null)
        DEFINE_OP(op_jneq_ptr)
        DEFINE_OP(op_jnless)
        DEFINE_OP(op_jless)
        DEFINE_OP(op_jnlesseq)
        DEFINE_OP(op_jsr)
        DEFINE_OP(op_jtrue)
        DEFINE_OP(op_load_varargs)
        DEFINE_OP(op_loop)
        DEFINE_OP(op_loop_if_less)
        DEFINE_OP(op_loop_if_lesseq)
        DEFINE_OP(op_loop_if_true)
        DEFINE_OP(op_loop_if_false)
        DEFINE_OP(op_lshift)
        DEFINE_OP(op_method_check)
        DEFINE_OP(op_mod)
        DEFINE_OP(op_mov)
        DEFINE_OP(op_mul)
#if USE(JSVALUE32_64)
        DEFINE_OP(op_negate)
#endif
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
#if USE(JSVALUE32_64)
    m_globalResolveInfoIndex = 0;
#endif
    m_callLinkInfoIndex = 0;

    for (Vector<SlowCaseEntry>::iterator iter = m_slowCases.begin(); iter != m_slowCases.end();) {
#if !USE(JSVALUE32_64)
        killLastResultRegister();
#endif

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
#if !USE(JSVALUE32)
        DEFINE_SLOWCASE_OP(op_div)
#endif
        DEFINE_SLOWCASE_OP(op_eq)
        DEFINE_SLOWCASE_OP(op_get_by_id)
        DEFINE_SLOWCASE_OP(op_get_by_val)
        DEFINE_SLOWCASE_OP(op_get_by_pname)
        DEFINE_SLOWCASE_OP(op_instanceof)
        DEFINE_SLOWCASE_OP(op_jfalse)
        DEFINE_SLOWCASE_OP(op_jnless)
        DEFINE_SLOWCASE_OP(op_jless)
        DEFINE_SLOWCASE_OP(op_jnlesseq)
        DEFINE_SLOWCASE_OP(op_jtrue)
        DEFINE_SLOWCASE_OP(op_loop_if_less)
        DEFINE_SLOWCASE_OP(op_loop_if_lesseq)
        DEFINE_SLOWCASE_OP(op_loop_if_true)
        DEFINE_SLOWCASE_OP(op_loop_if_false)
        DEFINE_SLOWCASE_OP(op_lshift)
        DEFINE_SLOWCASE_OP(op_method_check)
        DEFINE_SLOWCASE_OP(op_mod)
        DEFINE_SLOWCASE_OP(op_mul)
#if USE(JSVALUE32_64)
        DEFINE_SLOWCASE_OP(op_negate)
#endif
        DEFINE_SLOWCASE_OP(op_neq)
        DEFINE_SLOWCASE_OP(op_not)
        DEFINE_SLOWCASE_OP(op_nstricteq)
        DEFINE_SLOWCASE_OP(op_post_dec)
        DEFINE_SLOWCASE_OP(op_post_inc)
        DEFINE_SLOWCASE_OP(op_pre_dec)
        DEFINE_SLOWCASE_OP(op_pre_inc)
        DEFINE_SLOWCASE_OP(op_put_by_id)
        DEFINE_SLOWCASE_OP(op_put_by_val)
#if USE(JSVALUE32_64)
        DEFINE_SLOWCASE_OP(op_resolve_global)
#endif
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

JITCode JIT::privateCompile()
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
        JITStubCall(this, cti_register_file_check).call();
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

            record.jumpTable.simpleJumpTable->ctiDefault = patchBuffer.locationOf(m_labels[bytecodeIndex + record.defaultOffset]);

            for (unsigned j = 0; j < record.jumpTable.simpleJumpTable->branchOffsets.size(); ++j) {
                unsigned offset = record.jumpTable.simpleJumpTable->branchOffsets[j];
                record.jumpTable.simpleJumpTable->ctiOffsets[j] = offset ? patchBuffer.locationOf(m_labels[bytecodeIndex + offset]) : record.jumpTable.simpleJumpTable->ctiDefault;
            }
        } else {
            ASSERT(record.type == SwitchRecord::String);

            record.jumpTable.stringJumpTable->ctiDefault = patchBuffer.locationOf(m_labels[bytecodeIndex + record.defaultOffset]);

            StringJumpTable::StringOffsetTable::iterator end = record.jumpTable.stringJumpTable->offsetTable.end();            
            for (StringJumpTable::StringOffsetTable::iterator it = record.jumpTable.stringJumpTable->offsetTable.begin(); it != end; ++it) {
                unsigned offset = it->second.branchOffset;
                it->second.ctiOffset = offset ? patchBuffer.locationOf(m_labels[bytecodeIndex + offset]) : record.jumpTable.stringJumpTable->ctiDefault;
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

    return patchBuffer.finalizeCode();
}

#if !USE(JSVALUE32_64)
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
#endif

#if ENABLE(JIT_OPTIMIZE_CALL)
void JIT::unlinkCall(CallLinkInfo* callLinkInfo)
{
    // When the JSFunction is deleted the pointer embedded in the instruction stream will no longer be valid
    // (and, if a new JSFunction happened to be constructed at the same location, we could get a false positive
    // match).  Reset the check so it no longer matches.
    RepatchBuffer repatchBuffer(callLinkInfo->ownerCodeBlock.get());
#if USE(JSVALUE32_64)
    repatchBuffer.repatch(callLinkInfo->hotPathBegin, 0);
#else
    repatchBuffer.repatch(callLinkInfo->hotPathBegin, JSValue::encode(JSValue()));
#endif
}

void JIT::linkCall(JSFunction* callee, CodeBlock* callerCodeBlock, CodeBlock* calleeCodeBlock, JITCode& code, CallLinkInfo* callLinkInfo, int callerArgCount, JSGlobalData* globalData)
{
    RepatchBuffer repatchBuffer(callerCodeBlock);

    // Currently we only link calls with the exact number of arguments.
    // If this is a native call calleeCodeBlock is null so the number of parameters is unimportant
    if (!calleeCodeBlock || (callerArgCount == calleeCodeBlock->m_numParameters)) {
        ASSERT(!callLinkInfo->isLinked());
    
        if (calleeCodeBlock)
            calleeCodeBlock->addCaller(callLinkInfo);
    
        repatchBuffer.repatch(callLinkInfo->hotPathBegin, callee);
        repatchBuffer.relink(callLinkInfo->hotPathOther, code.addressForCall());
    }

    // patch the call so we do not continue to try to link.
    repatchBuffer.relink(callLinkInfo->callReturnLocation, globalData->jitStubs.ctiVirtualCall());
}
#endif // ENABLE(JIT_OPTIMIZE_CALL)

} // namespace JSC

#endif // ENABLE(JIT)
