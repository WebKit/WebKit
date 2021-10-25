/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)
#if USE(JSVALUE32_64)
#include "JIT.h"

#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "DirectArguments.h"
#include "GCAwareJITStubRoutine.h"
#include "InterpreterInlines.h"
#include "JITInlines.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSLexicalEnvironment.h"
#include "JSPromise.h"
#include "LinkBuffer.h"
#include "OpcodeInlines.h"
#include "ResultType.h"
#include "SlowPathCall.h"
#include "StructureStubInfo.h"
#include <wtf/StringPrintStream.h>


namespace JSC {
    
void JIT::emit_op_put_getter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterById>();
    VirtualRegister base = bytecode.m_base;
    int property = bytecode.m_property;
    int options = bytecode.m_attributes;
    VirtualRegister getter = bytecode.m_accessor;

    emitLoadPayload(base, regT1);
    emitLoadPayload(getter, regT3);
    callOperation(operationPutGetterById, m_profiledCodeBlock->globalObject(), regT1, m_profiledCodeBlock->identifier(property).impl(), options, regT3);
}

void JIT::emit_op_put_setter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutSetterById>();
    VirtualRegister base = bytecode.m_base;
    int property = bytecode.m_property;
    int options = bytecode.m_attributes;
    VirtualRegister setter = bytecode.m_accessor;

    emitLoadPayload(base, regT1);
    emitLoadPayload(setter, regT3);
    callOperation(operationPutSetterById, m_profiledCodeBlock->globalObject(), regT1, m_profiledCodeBlock->identifier(property).impl(), options, regT3);
}

void JIT::emit_op_put_getter_setter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterSetterById>();
    VirtualRegister base = bytecode.m_base;
    int property = bytecode.m_property;
    int attributes = bytecode.m_attributes;
    VirtualRegister getter = bytecode.m_getter;
    VirtualRegister setter = bytecode.m_setter;

    emitLoadPayload(base, regT1);
    emitLoadPayload(getter, regT3);
    emitLoadPayload(setter, regT4);
    callOperation(operationPutGetterSetter, m_profiledCodeBlock->globalObject(), regT1, m_profiledCodeBlock->identifier(property).impl(), attributes, regT3, regT4);
}

void JIT::emit_op_put_getter_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterByVal>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    int32_t attributes = bytecode.m_attributes;
    VirtualRegister getter = bytecode.m_accessor;

    emitLoadPayload(base, regT2);
    emitLoad(property, regT1, regT0);
    emitLoadPayload(getter, regT3);
    callOperation(operationPutGetterByVal, m_profiledCodeBlock->globalObject(), regT2, JSValueRegs(regT1, regT0), attributes, regT3);
}

void JIT::emit_op_put_setter_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutSetterByVal>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    int32_t attributes = bytecode.m_attributes;
    VirtualRegister setter = bytecode.m_accessor;

    emitLoadPayload(base, regT2);
    emitLoad(property, regT1, regT0);
    emitLoadPayload(setter, regT3);
    callOperation(operationPutSetterByVal, m_profiledCodeBlock->globalObject(), regT2, JSValueRegs(regT1, regT0), attributes, regT3);
}

void JIT::emitResolveClosure(VirtualRegister dst, VirtualRegister scope, bool needsVarInjectionChecks, unsigned depth)
{
    emitVarInjectionCheck(needsVarInjectionChecks, regT0);
    move(TrustedImm32(JSValue::CellTag), regT1);
    emitLoadPayload(scope, regT0);
    for (unsigned i = 0; i < depth; ++i)
        loadPtr(Address(regT0, JSScope::offsetOfNext()), regT0);
    emitStore(dst, regT1, regT0);
}

void JIT::emit_op_resolve_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpResolveScope>();
    auto& metadata = bytecode.metadata(m_profiledCodeBlock);
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister scope = bytecode.m_scope;
    ResolveType resolveType = metadata.m_resolveType;
    unsigned depth = metadata.m_localScopeDepth;

    auto emitCode = [&] (ResolveType resolveType) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            JSScope* constantScope = JSScope::constantScopeForCodeBlock(resolveType, m_profiledCodeBlock);
            RELEASE_ASSERT(constantScope);
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType), regT0);
            load32(&metadata.m_globalLexicalBindingEpoch, regT1);
            addSlowCase(branch32(NotEqual, AbsoluteAddress(m_profiledCodeBlock->globalObject()->addressOfGlobalLexicalBindingEpoch()), regT1));
            move(TrustedImm32(JSValue::CellTag), regT1);
            move(TrustedImmPtr(constantScope), regT0);
            emitStore(dst, regT1, regT0);
            break;
        }

        case GlobalVar:
        case GlobalVarWithVarInjectionChecks: 
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks: {
            JSScope* constantScope = JSScope::constantScopeForCodeBlock(resolveType, m_profiledCodeBlock);
            RELEASE_ASSERT(constantScope);
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType), regT0);
            move(TrustedImm32(JSValue::CellTag), regT1);
            move(TrustedImmPtr(constantScope), regT0);
            emitStore(dst, regT1, regT0);
            break;
        }
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitResolveClosure(dst, scope, needsVarInjectionChecks(resolveType), depth);
            break;
        case ModuleVar:
            move(TrustedImm32(JSValue::CellTag), regT1);
            move(TrustedImmPtr(metadata.m_lexicalEnvironment.get()), regT0);
            emitStore(dst, regT1, regT0);
            break;
        case Dynamic:
            addSlowCase(jump());
            break;
        case ResolvedClosureVar:
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };
    switch (resolveType) {
    case GlobalProperty:
    case GlobalPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&metadata.m_resolveType, regT0);

        Jump notGlobalProperty = branch32(NotEqual, regT0, TrustedImm32(resolveType));
        emitCode(resolveType);
        skipToEnd.append(jump());

        notGlobalProperty.link(this);
        emitCode(needsVarInjectionChecks(resolveType) ? GlobalLexicalVarWithVarInjectionChecks : GlobalLexicalVar);

        skipToEnd.link(this);
        break;
    }
    case UnresolvedProperty:
    case UnresolvedPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&metadata.m_resolveType, regT0);

        Jump notGlobalProperty = branch32(NotEqual, regT0, TrustedImm32(GlobalProperty));
        emitCode(GlobalProperty);
        skipToEnd.append(jump());
        notGlobalProperty.link(this);

        Jump notGlobalPropertyWithVarInjections = branch32(NotEqual, regT0, TrustedImm32(GlobalPropertyWithVarInjectionChecks));
        emitCode(GlobalPropertyWithVarInjectionChecks);
        skipToEnd.append(jump());
        notGlobalPropertyWithVarInjections.link(this);

        Jump notGlobalLexicalVar = branch32(NotEqual, regT0, TrustedImm32(GlobalLexicalVar));
        emitCode(GlobalLexicalVar);
        skipToEnd.append(jump());
        notGlobalLexicalVar.link(this);

        Jump notGlobalLexicalVarWithVarInjections = branch32(NotEqual, regT0, TrustedImm32(GlobalLexicalVarWithVarInjectionChecks));
        emitCode(GlobalLexicalVarWithVarInjectionChecks);
        skipToEnd.append(jump());
        notGlobalLexicalVarWithVarInjections.link(this);

        addSlowCase(jump());
        skipToEnd.link(this);
        break;
    }

    default:
        emitCode(resolveType);
        break;
    }
}

void JIT::emitLoadWithStructureCheck(VirtualRegister scope, Structure** structureSlot)
{
    emitLoad(scope, regT1, regT0);
    loadPtr(structureSlot, regT2);
    addSlowCase(branchPtr(NotEqual, Address(regT0, JSCell::structureIDOffset()), regT2));
}

void JIT::emitGetVarFromPointer(JSValue* operand, GPRReg tag, GPRReg payload)
{
    uintptr_t rawAddress = bitwise_cast<uintptr_t>(operand);
    load32(bitwise_cast<void*>(rawAddress + TagOffset), tag);
    load32(bitwise_cast<void*>(rawAddress + PayloadOffset), payload);
}
void JIT::emitGetVarFromIndirectPointer(JSValue** operand, GPRReg tag, GPRReg payload)
{
    loadPtr(operand, payload);
    load32(Address(payload, TagOffset), tag);
    load32(Address(payload, PayloadOffset), payload);
}

void JIT::emitGetClosureVar(VirtualRegister scope, uintptr_t operand)
{
    emitLoad(scope, regT1, regT0);
    load32(Address(regT0, JSLexicalEnvironment::offsetOfVariables() + operand * sizeof(Register) + TagOffset), regT1);
    load32(Address(regT0, JSLexicalEnvironment::offsetOfVariables() + operand * sizeof(Register) + PayloadOffset), regT0);
}

void JIT::emit_op_get_from_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetFromScope>();
    auto& metadata = bytecode.metadata(m_profiledCodeBlock);
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister scope = bytecode.m_scope;
    ResolveType resolveType = metadata.m_getPutInfo.resolveType();
    Structure** structureSlot = metadata.m_structure.slot();
    uintptr_t* operandSlot = reinterpret_cast<uintptr_t*>(&metadata.m_operand);
    
    auto emitCode = [&] (ResolveType resolveType, bool indirectLoadForOperand) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            emitLoadWithStructureCheck(scope, structureSlot); // Structure check covers var injection.
            GPRReg base = regT2;
            GPRReg resultTag = regT1;
            GPRReg resultPayload = regT0;
            GPRReg offset = regT3;
            
            move(regT0, base);
            load32(operandSlot, offset);
            if (ASSERT_ENABLED) {
                Jump isOutOfLine = branch32(GreaterThanOrEqual, offset, TrustedImm32(firstOutOfLineOffset));
                abortWithReason(JITOffsetIsNotOutOfLine);
                isOutOfLine.link(this);
            }
            loadPtr(Address(base, JSObject::butterflyOffset()), base);
            neg32(offset);
            load32(BaseIndex(base, offset, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload) + (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)), resultPayload);
            load32(BaseIndex(base, offset, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag) + (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)), resultTag);
            break;
        }
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks:
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType), regT0);
            if (indirectLoadForOperand)
                emitGetVarFromIndirectPointer(bitwise_cast<JSValue**>(operandSlot), regT1, regT0);
            else
                emitGetVarFromPointer(bitwise_cast<JSValue*>(*operandSlot), regT1, regT0);
            if (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks) // TDZ check.
                addSlowCase(branchIfEmpty(regT1));
            break;
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType), regT0);
            emitGetClosureVar(scope, *operandSlot);
            break;
        case Dynamic:
            addSlowCase(jump());
            break;
        case ModuleVar:
        case ResolvedClosureVar:
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };

    switch (resolveType) {
    case GlobalProperty:
    case GlobalPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&metadata.m_getPutInfo, regT0);
        and32(TrustedImm32(GetPutInfo::typeBits), regT0); // Load ResolveType into T0

        Jump isNotGlobalProperty = branch32(NotEqual, regT0, TrustedImm32(resolveType));
        emitCode(resolveType, false);
        skipToEnd.append(jump());

        isNotGlobalProperty.link(this);
        emitCode(needsVarInjectionChecks(resolveType) ? GlobalLexicalVarWithVarInjectionChecks : GlobalLexicalVar, true);
        skipToEnd.link(this);
        break;
    }
    case UnresolvedProperty:
    case UnresolvedPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&metadata.m_getPutInfo, regT0);
        and32(TrustedImm32(GetPutInfo::typeBits), regT0); // Load ResolveType into T0

        Jump isGlobalProperty = branch32(Equal, regT0, TrustedImm32(GlobalProperty));
        Jump notGlobalPropertyWithVarInjections = branch32(NotEqual, regT0, TrustedImm32(GlobalPropertyWithVarInjectionChecks));
        isGlobalProperty.link(this);
        emitCode(GlobalProperty, false);
        skipToEnd.append(jump());
        notGlobalPropertyWithVarInjections.link(this);

        Jump notGlobalLexicalVar = branch32(NotEqual, regT0, TrustedImm32(GlobalLexicalVar));
        emitCode(GlobalLexicalVar, true);
        skipToEnd.append(jump());
        notGlobalLexicalVar.link(this);

        Jump notGlobalLexicalVarWithVarInjections = branch32(NotEqual, regT0, TrustedImm32(GlobalLexicalVarWithVarInjectionChecks));
        emitCode(GlobalLexicalVarWithVarInjectionChecks, true);
        skipToEnd.append(jump());
        notGlobalLexicalVarWithVarInjections.link(this);

        addSlowCase(jump());

        skipToEnd.link(this);
        break;
    }

    default:
        emitCode(resolveType, false);
        break;
    }
    emitValueProfilingSite(bytecode, JSValueRegs(regT1, regT0));
    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_get_from_scope(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpGetFromScope>();
    VirtualRegister dst = bytecode.m_dst;
    callOperationWithProfile(bytecode, operationGetFromScope, dst, m_profiledCodeBlock->globalObject(), currentInstruction);
}

void JIT::emitPutGlobalVariable(JSValue* operand, VirtualRegister value, WatchpointSet* set)
{
    emitLoad(value, regT1, regT0);
    emitNotifyWrite(set);
    uintptr_t rawAddress = bitwise_cast<uintptr_t>(operand);
    store32(regT1, bitwise_cast<void*>(rawAddress + TagOffset));
    store32(regT0, bitwise_cast<void*>(rawAddress + PayloadOffset));
}

void JIT::emitPutGlobalVariableIndirect(JSValue** addressOfOperand, VirtualRegister value, WatchpointSet** indirectWatchpointSet)
{
    emitLoad(value, regT1, regT0);
    loadPtr(indirectWatchpointSet, regT2);
    emitNotifyWrite(*indirectWatchpointSet); // FIXME: ??
    loadPtr(addressOfOperand, regT2);
    store32(regT1, Address(regT2, TagOffset));
    store32(regT0, Address(regT2, PayloadOffset));
}

void JIT::emitPutClosureVar(VirtualRegister scope, uintptr_t operand, VirtualRegister value, WatchpointSet* set)
{
    emitLoad(value, regT3, regT2);
    emitLoad(scope, regT1, regT0);
    emitNotifyWrite(set);
    store32(regT3, Address(regT0, JSLexicalEnvironment::offsetOfVariables() + operand * sizeof(Register) + TagOffset));
    store32(regT2, Address(regT0, JSLexicalEnvironment::offsetOfVariables() + operand * sizeof(Register) + PayloadOffset));
}

void JIT::emit_op_put_to_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutToScope>();
    auto& metadata = bytecode.metadata(m_profiledCodeBlock);
    VirtualRegister scope = bytecode.m_scope;
    VirtualRegister value = bytecode.m_value;
    GetPutInfo getPutInfo = bytecode.metadata(m_profiledCodeBlock).m_getPutInfo;
    ResolveType resolveType = getPutInfo.resolveType();
    Structure** structureSlot = metadata.m_structure.slot();
    uintptr_t* operandSlot = reinterpret_cast<uintptr_t*>(&metadata.m_operand);
    
    auto emitCode = [&] (ResolveType resolveType, bool indirectLoadForOperand) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            emitWriteBarrier(m_profiledCodeBlock->globalObject(), value, ShouldFilterValue);
            emitLoadWithStructureCheck(scope, structureSlot); // Structure check covers var injection.
            emitLoad(value, regT3, regT2);
            
            loadPtr(Address(regT0, JSObject::butterflyOffset()), regT0);
            loadPtr(operandSlot, regT1);
            negPtr(regT1);
            store32(regT3, BaseIndex(regT0, regT1, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
            store32(regT2, BaseIndex(regT0, regT1, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
            break;
        }
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks: {
            JSScope* constantScope = JSScope::constantScopeForCodeBlock(resolveType, m_profiledCodeBlock);
            RELEASE_ASSERT(constantScope);
            emitWriteBarrier(constantScope, value, ShouldFilterValue);
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType), regT0);
            emitVarReadOnlyCheck(resolveType, regT0);
            if (!isInitialization(getPutInfo.initializationMode()) && (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)) {
                // We need to do a TDZ check here because we can't always prove we need to emit TDZ checks statically.
                if (indirectLoadForOperand)
                    emitGetVarFromIndirectPointer(bitwise_cast<JSValue**>(operandSlot), regT1, regT0);
                else
                    emitGetVarFromPointer(bitwise_cast<JSValue*>(*operandSlot), regT1, regT0);
                addSlowCase(branchIfEmpty(regT1));
            }
            if (indirectLoadForOperand)
                emitPutGlobalVariableIndirect(bitwise_cast<JSValue**>(operandSlot), value, &metadata.m_watchpointSet);
            else
                emitPutGlobalVariable(bitwise_cast<JSValue*>(*operandSlot), value, metadata.m_watchpointSet);
            break;
        }
        case ResolvedClosureVar:
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitWriteBarrier(scope, value, ShouldFilterValue);
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType), regT0);
            emitPutClosureVar(scope, *operandSlot, value, metadata.m_watchpointSet);
            break;
        case ModuleVar:
        case Dynamic:
            addSlowCase(jump());
            break;
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };

    switch (resolveType) {
    case GlobalProperty:
    case GlobalPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&metadata.m_getPutInfo, regT0);
        and32(TrustedImm32(GetPutInfo::typeBits), regT0); // Load ResolveType into T0

        Jump isGlobalProperty = branch32(Equal, regT0, TrustedImm32(resolveType));
        Jump isGlobalLexicalVar = branch32(Equal, regT0, TrustedImm32(needsVarInjectionChecks(resolveType) ? GlobalLexicalVarWithVarInjectionChecks : GlobalLexicalVar));
        addSlowCase(jump()); // Dynamic, it can happen if we attempt to put a value to already-initialized const binding.

        isGlobalLexicalVar.link(this);
        emitCode(needsVarInjectionChecks(resolveType) ? GlobalLexicalVarWithVarInjectionChecks : GlobalLexicalVar, true);
        skipToEnd.append(jump());

        isGlobalProperty.link(this);
        emitCode(resolveType, false);
        skipToEnd.link(this);
        break;
    }
    case UnresolvedProperty:
    case UnresolvedPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&metadata.m_getPutInfo, regT0);
        and32(TrustedImm32(GetPutInfo::typeBits), regT0); // Load ResolveType into T0

        Jump isGlobalProperty = branch32(Equal, regT0, TrustedImm32(GlobalProperty));
        Jump notGlobalPropertyWithVarInjections = branch32(NotEqual, regT0, TrustedImm32(GlobalPropertyWithVarInjectionChecks));
        isGlobalProperty.link(this);
        emitCode(GlobalProperty, false);
        skipToEnd.append(jump());
        notGlobalPropertyWithVarInjections.link(this);

        Jump notGlobalLexicalVar = branch32(NotEqual, regT0, TrustedImm32(GlobalLexicalVar));
        emitCode(GlobalLexicalVar, true);
        skipToEnd.append(jump());
        notGlobalLexicalVar.link(this);

        Jump notGlobalLexicalVarWithVarInjections = branch32(NotEqual, regT0, TrustedImm32(GlobalLexicalVarWithVarInjectionChecks));
        emitCode(GlobalLexicalVarWithVarInjectionChecks, true);
        skipToEnd.append(jump());
        notGlobalLexicalVarWithVarInjections.link(this);

        addSlowCase(jump());

        skipToEnd.link(this);
        break;
    }

    default:
        emitCode(resolveType, false);
        break;
    }
}

void JIT::emitSlow_op_put_to_scope(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpPutToScope>();
    ResolveType resolveType = bytecode.metadata(m_profiledCodeBlock).m_getPutInfo.resolveType();
    if (resolveType == ModuleVar) {
        JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_throw_strict_mode_readonly_property_write_error);
        slowPathCall.call();
    } else
        callOperation(operationPutToScope, m_profiledCodeBlock->globalObject(), currentInstruction);
}

void JIT::emit_op_get_from_arguments(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetFromArguments>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister arguments = bytecode.m_arguments;
    int index = bytecode.m_index;

    JSValueRegs resultRegs = JSValueRegs(regT1, regT0);
    
    emitLoadPayload(arguments, regT0);
    load32(Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + TagOffset), resultRegs.tagGPR());
    load32(Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + PayloadOffset), resultRegs.payloadGPR());
    emitValueProfilingSite(bytecode, resultRegs);
    emitStore(dst, resultRegs.tagGPR(), resultRegs.payloadGPR());
}

void JIT::emit_op_put_to_arguments(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutToArguments>();
    VirtualRegister arguments = bytecode.m_arguments;
    int index = bytecode.m_index;
    VirtualRegister value = bytecode.m_value;
    
    emitWriteBarrier(arguments, value, ShouldFilterValue);
    
    emitLoadPayload(arguments, regT0);
    emitLoad(value, regT1, regT2);
    store32(regT1, Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + TagOffset));
    store32(regT2, Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + PayloadOffset));
}

void JIT::emit_op_get_internal_field(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetInternalField>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    unsigned index = bytecode.m_index;

    JSValueRegs resultRegs = JSValueRegs(regT1, regT0);

    emitLoadPayload(base, regT2);
    load32(Address(regT2, JSInternalFieldObjectImpl<>::offsetOfInternalField(index) + TagOffset), resultRegs.tagGPR());
    load32(Address(regT2, JSInternalFieldObjectImpl<>::offsetOfInternalField(index) + PayloadOffset), resultRegs.payloadGPR());
    emitValueProfilingSite(bytecode, resultRegs);
    emitStore(dst, resultRegs.tagGPR(), resultRegs.payloadGPR());
}

void JIT::emit_op_put_internal_field(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutInternalField>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister value = bytecode.m_value;
    unsigned index = bytecode.m_index;

    emitLoadPayload(base, regT0);
    emitLoad(value, regT1, regT2);
    store32(regT1, Address(regT0, JSInternalFieldObjectImpl<>::offsetOfInternalField(index) + TagOffset));
    store32(regT2, Address(regT0, JSInternalFieldObjectImpl<>::offsetOfInternalField(index) + PayloadOffset));
    emitWriteBarrier(base, value, ShouldFilterValue);
}

void JIT::emit_op_get_property_enumerator(const Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_get_property_enumerator);
    slowPathCall.call();
}

void JIT::emit_op_enumerator_next(const Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_enumerator_next);
    slowPathCall.call();
}

void JIT::emit_op_enumerator_get_by_val(const Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_enumerator_get_by_val);
    slowPathCall.call();
}

void JIT::emitSlow_op_enumerator_get_by_val(const Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    UNREACHABLE_FOR_PLATFORM();
}

void JIT::emit_op_enumerator_in_by_val(const Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_enumerator_in_by_val);
    slowPathCall.call();
}

void JIT::emit_op_enumerator_has_own_property(const Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_enumerator_has_own_property);
    slowPathCall.call();
}

} // namespace JSC

#endif // USE(JSVALUE32_64)
#endif // ENABLE(JIT)
