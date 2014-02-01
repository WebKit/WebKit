/*
 * Copyright (C) 2008, 2009, 2010, 2012, 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CodeBlock.h"

#include "BytecodeGenerator.h"
#include "BytecodeUseDef.h"
#include "CallLinkStatus.h"
#include "DFGCapabilities.h"
#include "DFGCommon.h"
#include "DFGDriver.h"
#include "DFGNode.h"
#include "DFGWorklist.h"
#include "Debugger.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JITStubs.h"
#include "JSActivation.h"
#include "JSCJSValue.h"
#include "JSFunction.h"
#include "JSNameScope.h"
#include "LLIntEntrypoint.h"
#include "LowLevelInterpreter.h"
#include "Operations.h"
#include "PolymorphicPutByIdList.h"
#include "ReduceWhitespace.h"
#include "Repatch.h"
#include "RepatchBuffer.h"
#include "SlotVisitorInlines.h"
#include "UnlinkedInstructionStream.h"
#include <wtf/BagToHashMap.h>
#include <wtf/CommaPrinter.h>
#include <wtf/StringExtras.h>
#include <wtf/StringPrintStream.h>

#if ENABLE(DFG_JIT)
#include "DFGOperations.h"
#endif

#if ENABLE(FTL_JIT)
#include "FTLJITCode.h"
#endif

namespace JSC {

CString CodeBlock::inferredName() const
{
    switch (codeType()) {
    case GlobalCode:
        return "<global>";
    case EvalCode:
        return "<eval>";
    case FunctionCode:
        return jsCast<FunctionExecutable*>(ownerExecutable())->inferredName().utf8();
    default:
        CRASH();
        return CString("", 0);
    }
}

bool CodeBlock::hasHash() const
{
    return !!m_hash;
}

bool CodeBlock::isSafeToComputeHash() const
{
    return !isCompilationThread();
}

CodeBlockHash CodeBlock::hash() const
{
    if (!m_hash) {
        RELEASE_ASSERT(isSafeToComputeHash());
        m_hash = CodeBlockHash(ownerExecutable()->source(), specializationKind());
    }
    return m_hash;
}

CString CodeBlock::sourceCodeForTools() const
{
    if (codeType() != FunctionCode)
        return ownerExecutable()->source().toUTF8();
    
    SourceProvider* provider = source();
    FunctionExecutable* executable = jsCast<FunctionExecutable*>(ownerExecutable());
    UnlinkedFunctionExecutable* unlinked = executable->unlinkedExecutable();
    unsigned unlinkedStartOffset = unlinked->startOffset();
    unsigned linkedStartOffset = executable->source().startOffset();
    int delta = linkedStartOffset - unlinkedStartOffset;
    unsigned rangeStart = delta + unlinked->unlinkedFunctionNameStart();
    unsigned rangeEnd = delta + unlinked->startOffset() + unlinked->sourceLength();
    return toCString(
        "function ",
        provider->source().impl()->utf8ForRange(rangeStart, rangeEnd - rangeStart));
}

CString CodeBlock::sourceCodeOnOneLine() const
{
    return reduceWhitespace(sourceCodeForTools());
}

void CodeBlock::dumpAssumingJITType(PrintStream& out, JITCode::JITType jitType) const
{
    out.print(inferredName(), "#");
    if (hasHash() || isSafeToComputeHash())
        out.print(hash());
    else
        out.print("<no-hash>");
    out.print(":[", RawPointer(this), "->");
    if (!!m_alternative)
        out.print(RawPointer(m_alternative.get()), "->");
    out.print(RawPointer(ownerExecutable()), ", ", jitType, codeType());

    if (codeType() == FunctionCode)
        out.print(specializationKind());
    out.print(", ", instructionCount());
    if (this->jitType() == JITCode::BaselineJIT && m_shouldAlwaysBeInlined)
        out.print(" (SABI)");
    if (ownerExecutable()->neverInline())
        out.print(" (NeverInline)");
    if (ownerExecutable()->isStrictMode())
        out.print(" (StrictMode)");
    if (this->jitType() == JITCode::BaselineJIT && m_didFailFTLCompilation)
        out.print(" (FTLFail)");
    if (this->jitType() == JITCode::BaselineJIT && m_hasBeenCompiledWithFTL)
        out.print(" (HadFTLReplacement)");
    out.print("]");
}

void CodeBlock::dump(PrintStream& out) const
{
    dumpAssumingJITType(out, jitType());
}

static CString constantName(int k, JSValue value)
{
    return toCString(value, "(@k", k - FirstConstantRegisterIndex, ")");
}

static CString idName(int id0, const Identifier& ident)
{
    return toCString(ident.impl(), "(@id", id0, ")");
}

CString CodeBlock::registerName(int r) const
{
    if (r == missingThisObjectMarker())
        return "<null>";

    if (isConstantRegisterIndex(r))
        return constantName(r, getConstant(r));

    if (operandIsArgument(r)) {
        if (!VirtualRegister(r).toArgument())
            return "this";
        return toCString("arg", VirtualRegister(r).toArgument());
    }

    return toCString("loc", VirtualRegister(r).toLocal());
}

static CString regexpToSourceString(RegExp* regExp)
{
    char postfix[5] = { '/', 0, 0, 0, 0 };
    int index = 1;
    if (regExp->global())
        postfix[index++] = 'g';
    if (regExp->ignoreCase())
        postfix[index++] = 'i';
    if (regExp->multiline())
        postfix[index] = 'm';

    return toCString("/", regExp->pattern().impl(), postfix);
}

static CString regexpName(int re, RegExp* regexp)
{
    return toCString(regexpToSourceString(regexp), "(@re", re, ")");
}

NEVER_INLINE static const char* debugHookName(int debugHookID)
{
    switch (static_cast<DebugHookID>(debugHookID)) {
        case DidEnterCallFrame:
            return "didEnterCallFrame";
        case WillLeaveCallFrame:
            return "willLeaveCallFrame";
        case WillExecuteStatement:
            return "willExecuteStatement";
        case WillExecuteProgram:
            return "willExecuteProgram";
        case DidExecuteProgram:
            return "didExecuteProgram";
        case DidReachBreakpoint:
            return "didReachBreakpoint";
    }

    RELEASE_ASSERT_NOT_REACHED();
    return "";
}

void CodeBlock::printUnaryOp(PrintStream& out, ExecState* exec, int location, const Instruction*& it, const char* op)
{
    int r0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;

    printLocationAndOp(out, exec, location, it, op);
    out.printf("%s, %s", registerName(r0).data(), registerName(r1).data());
}

void CodeBlock::printBinaryOp(PrintStream& out, ExecState* exec, int location, const Instruction*& it, const char* op)
{
    int r0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;
    int r2 = (++it)->u.operand;
    printLocationAndOp(out, exec, location, it, op);
    out.printf("%s, %s, %s", registerName(r0).data(), registerName(r1).data(), registerName(r2).data());
}

void CodeBlock::printConditionalJump(PrintStream& out, ExecState* exec, const Instruction*, const Instruction*& it, int location, const char* op)
{
    int r0 = (++it)->u.operand;
    int offset = (++it)->u.operand;
    printLocationAndOp(out, exec, location, it, op);
    out.printf("%s, %d(->%d)", registerName(r0).data(), offset, location + offset);
}

void CodeBlock::printGetByIdOp(PrintStream& out, ExecState* exec, int location, const Instruction*& it)
{
    const char* op;
    switch (exec->interpreter()->getOpcodeID(it->u.opcode)) {
    case op_get_by_id:
        op = "get_by_id";
        break;
    case op_get_by_id_out_of_line:
        op = "get_by_id_out_of_line";
        break;
    case op_get_by_id_self:
        op = "get_by_id_self";
        break;
    case op_get_by_id_proto:
        op = "get_by_id_proto";
        break;
    case op_get_by_id_chain:
        op = "get_by_id_chain";
        break;
    case op_get_by_id_getter_self:
        op = "get_by_id_getter_self";
        break;
    case op_get_by_id_getter_proto:
        op = "get_by_id_getter_proto";
        break;
    case op_get_by_id_getter_chain:
        op = "get_by_id_getter_chain";
        break;
    case op_get_by_id_custom_self:
        op = "get_by_id_custom_self";
        break;
    case op_get_by_id_custom_proto:
        op = "get_by_id_custom_proto";
        break;
    case op_get_by_id_custom_chain:
        op = "get_by_id_custom_chain";
        break;
    case op_get_by_id_generic:
        op = "get_by_id_generic";
        break;
    case op_get_array_length:
        op = "array_length";
        break;
    case op_get_string_length:
        op = "string_length";
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        op = 0;
    }
    int r0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;
    int id0 = (++it)->u.operand;
    printLocationAndOp(out, exec, location, it, op);
    out.printf("%s, %s, %s", registerName(r0).data(), registerName(r1).data(), idName(id0, identifier(id0)).data());
    it += 4; // Increment up to the value profiler.
}

#if ENABLE(JIT) || ENABLE(LLINT) // unused in some configurations
static void dumpStructure(PrintStream& out, const char* name, ExecState* exec, Structure* structure, const Identifier& ident)
{
    if (!structure)
        return;
    
    out.printf("%s = %p", name, structure);
    
    PropertyOffset offset = structure->getConcurrently(exec->vm(), ident.impl());
    if (offset != invalidOffset)
        out.printf(" (offset = %d)", offset);
}
#endif

#if ENABLE(JIT) // unused when not ENABLE(JIT), leading to silly warnings
static void dumpChain(PrintStream& out, ExecState* exec, StructureChain* chain, const Identifier& ident)
{
    out.printf("chain = %p: [", chain);
    bool first = true;
    for (WriteBarrier<Structure>* currentStructure = chain->head();
         *currentStructure;
         ++currentStructure) {
        if (first)
            first = false;
        else
            out.printf(", ");
        dumpStructure(out, "struct", exec, currentStructure->get(), ident);
    }
    out.printf("]");
}
#endif

void CodeBlock::printGetByIdCacheStatus(PrintStream& out, ExecState* exec, int location, const StubInfoMap& map)
{
    Instruction* instruction = instructions().begin() + location;

    const Identifier& ident = identifier(instruction[3].u.operand);
    
    UNUSED_PARAM(ident); // tell the compiler to shut up in certain platform configurations.
    
#if ENABLE(LLINT)
    if (exec->interpreter()->getOpcodeID(instruction[0].u.opcode) == op_get_array_length)
        out.printf(" llint(array_length)");
    else if (Structure* structure = instruction[4].u.structure.get()) {
        out.printf(" llint(");
        dumpStructure(out, "struct", exec, structure, ident);
        out.printf(")");
    }
#endif

#if ENABLE(JIT)
    if (StructureStubInfo* stubPtr = map.get(CodeOrigin(location))) {
        StructureStubInfo& stubInfo = *stubPtr;
        if (stubInfo.seen) {
            out.printf(" jit(");
            
            Structure* baseStructure = 0;
            Structure* prototypeStructure = 0;
            StructureChain* chain = 0;
            PolymorphicAccessStructureList* structureList = 0;
            int listSize = 0;
            
            switch (stubInfo.accessType) {
            case access_get_by_id_self:
                out.printf("self");
                baseStructure = stubInfo.u.getByIdSelf.baseObjectStructure.get();
                break;
            case access_get_by_id_proto:
                out.printf("proto");
                baseStructure = stubInfo.u.getByIdProto.baseObjectStructure.get();
                prototypeStructure = stubInfo.u.getByIdProto.prototypeStructure.get();
                break;
            case access_get_by_id_chain:
                out.printf("chain");
                baseStructure = stubInfo.u.getByIdChain.baseObjectStructure.get();
                chain = stubInfo.u.getByIdChain.chain.get();
                break;
            case access_get_by_id_self_list:
                out.printf("self_list");
                structureList = stubInfo.u.getByIdSelfList.structureList;
                listSize = stubInfo.u.getByIdSelfList.listSize;
                break;
            case access_get_by_id_proto_list:
                out.printf("proto_list");
                structureList = stubInfo.u.getByIdProtoList.structureList;
                listSize = stubInfo.u.getByIdProtoList.listSize;
                break;
            case access_unset:
                out.printf("unset");
                break;
            case access_get_by_id_generic:
                out.printf("generic");
                break;
            case access_get_array_length:
                out.printf("array_length");
                break;
            case access_get_string_length:
                out.printf("string_length");
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            
            if (baseStructure) {
                out.printf(", ");
                dumpStructure(out, "struct", exec, baseStructure, ident);
            }
            
            if (prototypeStructure) {
                out.printf(", ");
                dumpStructure(out, "prototypeStruct", exec, baseStructure, ident);
            }
            
            if (chain) {
                out.printf(", ");
                dumpChain(out, exec, chain, ident);
            }
            
            if (structureList) {
                out.printf(", list = %p: [", structureList);
                for (int i = 0; i < listSize; ++i) {
                    if (i)
                        out.printf(", ");
                    out.printf("(");
                    dumpStructure(out, "base", exec, structureList->list[i].base.get(), ident);
                    if (structureList->list[i].isChain) {
                        if (structureList->list[i].u.chain.get()) {
                            out.printf(", ");
                            dumpChain(out, exec, structureList->list[i].u.chain.get(), ident);
                        }
                    } else {
                        if (structureList->list[i].u.proto.get()) {
                            out.printf(", ");
                            dumpStructure(out, "proto", exec, structureList->list[i].u.proto.get(), ident);
                        }
                    }
                    out.printf(")");
                }
                out.printf("]");
            }
            out.printf(")");
        }
    }
#else
    UNUSED_PARAM(map);
#endif
}

void CodeBlock::printCallOp(PrintStream& out, ExecState* exec, int location, const Instruction*& it, const char* op, CacheDumpMode cacheDumpMode, bool& hasPrintedProfiling)
{
    int dst = (++it)->u.operand;
    int func = (++it)->u.operand;
    int argCount = (++it)->u.operand;
    int registerOffset = (++it)->u.operand;
    printLocationAndOp(out, exec, location, it, op);
    out.printf("%s, %s, %d, %d", registerName(dst).data(), registerName(func).data(), argCount, registerOffset);
    if (cacheDumpMode == DumpCaches) {
#if ENABLE(LLINT)
        LLIntCallLinkInfo* callLinkInfo = it[1].u.callLinkInfo;
        if (callLinkInfo->lastSeenCallee) {
            out.printf(
                " llint(%p, exec %p)",
                callLinkInfo->lastSeenCallee.get(),
                callLinkInfo->lastSeenCallee->executable());
        }
#endif
#if ENABLE(JIT)
        if (numberOfCallLinkInfos()) {
            JSFunction* target = getCallLinkInfo(location).lastSeenCallee.get();
            if (target)
                out.printf(" jit(%p, exec %p)", target, target->executable());
        }
        out.print(" status(", CallLinkStatus::computeFor(this, location), ")");
#endif
    }
    ++it;
    dumpArrayProfiling(out, it, hasPrintedProfiling);
    dumpValueProfiling(out, it, hasPrintedProfiling);
}

void CodeBlock::printPutByIdOp(PrintStream& out, ExecState* exec, int location, const Instruction*& it, const char* op)
{
    int r0 = (++it)->u.operand;
    int id0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;
    printLocationAndOp(out, exec, location, it, op);
    out.printf("%s, %s, %s", registerName(r0).data(), idName(id0, identifier(id0)).data(), registerName(r1).data());
    it += 5;
}

void CodeBlock::dumpBytecode(PrintStream& out)
{
    // We only use the ExecState* for things that don't actually lead to JS execution,
    // like converting a JSString to a String. Hence the globalExec is appropriate.
    ExecState* exec = m_globalObject->globalExec();
    
    size_t instructionCount = 0;

    for (size_t i = 0; i < instructions().size(); i += opcodeLengths[exec->interpreter()->getOpcodeID(instructions()[i].u.opcode)])
        ++instructionCount;

    out.print(*this);
    out.printf(
        ": %lu m_instructions; %lu bytes; %d parameter(s); %d callee register(s); %d variable(s)",
        static_cast<unsigned long>(instructions().size()),
        static_cast<unsigned long>(instructions().size() * sizeof(Instruction)),
        m_numParameters, m_numCalleeRegisters, m_numVars);
    if (symbolTable() && symbolTable()->captureCount()) {
        out.printf(
            "; %d captured var(s) (from r%d to r%d, inclusive)",
            symbolTable()->captureCount(), symbolTable()->captureStart(), symbolTable()->captureEnd() + 1);
    }
    if (usesArguments()) {
        out.printf(
            "; uses arguments, in r%d, r%d",
            argumentsRegister().offset(),
            unmodifiedArgumentsRegister(argumentsRegister()).offset());
    }
    if (needsActivation() && codeType() == FunctionCode)
        out.printf("; activation in r%d", activationRegister().offset());
    out.printf("\n");
    
    StubInfoMap stubInfos;
    getStubInfoMap(stubInfos);
    
    const Instruction* begin = instructions().begin();
    const Instruction* end = instructions().end();
    for (const Instruction* it = begin; it != end; ++it)
        dumpBytecode(out, exec, begin, it, stubInfos);
    
    if (numberOfIdentifiers()) {
        out.printf("\nIdentifiers:\n");
        size_t i = 0;
        do {
            out.printf("  id%u = %s\n", static_cast<unsigned>(i), identifier(i).string().utf8().data());
            ++i;
        } while (i != numberOfIdentifiers());
    }

    if (!m_constantRegisters.isEmpty()) {
        out.printf("\nConstants:\n");
        size_t i = 0;
        do {
            out.printf("   k%u = %s\n", static_cast<unsigned>(i), toCString(m_constantRegisters[i].get()).data());
            ++i;
        } while (i < m_constantRegisters.size());
    }

    if (size_t count = m_unlinkedCode->numberOfRegExps()) {
        out.printf("\nm_regexps:\n");
        size_t i = 0;
        do {
            out.printf("  re%u = %s\n", static_cast<unsigned>(i), regexpToSourceString(m_unlinkedCode->regexp(i)).data());
            ++i;
        } while (i < count);
    }

    if (m_rareData && !m_rareData->m_exceptionHandlers.isEmpty()) {
        out.printf("\nException Handlers:\n");
        unsigned i = 0;
        do {
            out.printf("\t %d: { start: [%4d] end: [%4d] target: [%4d] depth: [%4d] }\n", i + 1, m_rareData->m_exceptionHandlers[i].start, m_rareData->m_exceptionHandlers[i].end, m_rareData->m_exceptionHandlers[i].target, m_rareData->m_exceptionHandlers[i].scopeDepth);
            ++i;
        } while (i < m_rareData->m_exceptionHandlers.size());
    }
    
    if (m_rareData && !m_rareData->m_switchJumpTables.isEmpty()) {
        out.printf("Switch Jump Tables:\n");
        unsigned i = 0;
        do {
            out.printf("  %1d = {\n", i);
            int entry = 0;
            Vector<int32_t>::const_iterator end = m_rareData->m_switchJumpTables[i].branchOffsets.end();
            for (Vector<int32_t>::const_iterator iter = m_rareData->m_switchJumpTables[i].branchOffsets.begin(); iter != end; ++iter, ++entry) {
                if (!*iter)
                    continue;
                out.printf("\t\t%4d => %04d\n", entry + m_rareData->m_switchJumpTables[i].min, *iter);
            }
            out.printf("      }\n");
            ++i;
        } while (i < m_rareData->m_switchJumpTables.size());
    }
    
    if (m_rareData && !m_rareData->m_stringSwitchJumpTables.isEmpty()) {
        out.printf("\nString Switch Jump Tables:\n");
        unsigned i = 0;
        do {
            out.printf("  %1d = {\n", i);
            StringJumpTable::StringOffsetTable::const_iterator end = m_rareData->m_stringSwitchJumpTables[i].offsetTable.end();
            for (StringJumpTable::StringOffsetTable::const_iterator iter = m_rareData->m_stringSwitchJumpTables[i].offsetTable.begin(); iter != end; ++iter)
                out.printf("\t\t\"%s\" => %04d\n", String(iter->key).utf8().data(), iter->value.branchOffset);
            out.printf("      }\n");
            ++i;
        } while (i < m_rareData->m_stringSwitchJumpTables.size());
    }

    out.printf("\n");
}

void CodeBlock::beginDumpProfiling(PrintStream& out, bool& hasPrintedProfiling)
{
    if (hasPrintedProfiling) {
        out.print("; ");
        return;
    }
    
    out.print("    ");
    hasPrintedProfiling = true;
}

void CodeBlock::dumpValueProfiling(PrintStream& out, const Instruction*& it, bool& hasPrintedProfiling)
{
    ConcurrentJITLocker locker(m_lock);
    
    ++it;
    CString description = it->u.profile->briefDescription(locker);
    if (!description.length())
        return;
    beginDumpProfiling(out, hasPrintedProfiling);
    out.print(description);
}

void CodeBlock::dumpArrayProfiling(PrintStream& out, const Instruction*& it, bool& hasPrintedProfiling)
{
    ConcurrentJITLocker locker(m_lock);
    
    ++it;
    if (!it->u.arrayProfile)
        return;
    CString description = it->u.arrayProfile->briefDescription(locker, this);
    if (!description.length())
        return;
    beginDumpProfiling(out, hasPrintedProfiling);
    out.print(description);
}

void CodeBlock::dumpRareCaseProfile(PrintStream& out, const char* name, RareCaseProfile* profile, bool& hasPrintedProfiling)
{
    if (!profile || !profile->m_counter)
        return;

    beginDumpProfiling(out, hasPrintedProfiling);
    out.print(name, profile->m_counter);
}

void CodeBlock::dumpBytecode(PrintStream& out, ExecState* exec, const Instruction* begin, const Instruction*& it, const StubInfoMap& map)
{
    int location = it - begin;
    bool hasPrintedProfiling = false;
    switch (exec->interpreter()->getOpcodeID(it->u.opcode)) {
        case op_enter: {
            printLocationAndOp(out, exec, location, it, "enter");
            break;
        }
        case op_touch_entry: {
            printLocationAndOp(out, exec, location, it, "touch_entry");
            break;
        }
        case op_create_activation: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "create_activation", r0);
            break;
        }
        case op_create_arguments: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "create_arguments", r0);
            break;
        }
        case op_init_lazy_reg: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "init_lazy_reg", r0);
            break;
        }
        case op_get_callee: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "get_callee", r0);
            ++it;
            break;
        }
        case op_create_this: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            unsigned inferredInlineCapacity = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "create_this");
            out.printf("%s, %s, %u", registerName(r0).data(), registerName(r1).data(), inferredInlineCapacity);
            break;
        }
        case op_to_this: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "to_this", r0);
            ++it; // Skip value profile.
            break;
        }
        case op_new_object: {
            int r0 = (++it)->u.operand;
            unsigned inferredInlineCapacity = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "new_object");
            out.printf("%s, %u", registerName(r0).data(), inferredInlineCapacity);
            ++it; // Skip object allocation profile.
            break;
        }
        case op_new_array: {
            int dst = (++it)->u.operand;
            int argv = (++it)->u.operand;
            int argc = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "new_array");
            out.printf("%s, %s, %d", registerName(dst).data(), registerName(argv).data(), argc);
            ++it; // Skip array allocation profile.
            break;
        }
        case op_new_array_with_size: {
            int dst = (++it)->u.operand;
            int length = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "new_array_with_size");
            out.printf("%s, %s", registerName(dst).data(), registerName(length).data());
            ++it; // Skip array allocation profile.
            break;
        }
        case op_new_array_buffer: {
            int dst = (++it)->u.operand;
            int argv = (++it)->u.operand;
            int argc = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "new_array_buffer");
            out.printf("%s, %d, %d", registerName(dst).data(), argv, argc);
            ++it; // Skip array allocation profile.
            break;
        }
        case op_new_regexp: {
            int r0 = (++it)->u.operand;
            int re0 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "new_regexp");
            out.printf("%s, ", registerName(r0).data());
            if (r0 >=0 && r0 < (int)m_unlinkedCode->numberOfRegExps())
                out.printf("%s", regexpName(re0, regexp(re0)).data());
            else
                out.printf("bad_regexp(%d)", re0);
            break;
        }
        case op_mov: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "mov");
            out.printf("%s, %s", registerName(r0).data(), registerName(r1).data());
            break;
        }
        case op_captured_mov: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "captured_mov");
            out.printf("%s, %s", registerName(r0).data(), registerName(r1).data());
            ++it;
            break;
        }
        case op_not: {
            printUnaryOp(out, exec, location, it, "not");
            break;
        }
        case op_eq: {
            printBinaryOp(out, exec, location, it, "eq");
            break;
        }
        case op_eq_null: {
            printUnaryOp(out, exec, location, it, "eq_null");
            break;
        }
        case op_neq: {
            printBinaryOp(out, exec, location, it, "neq");
            break;
        }
        case op_neq_null: {
            printUnaryOp(out, exec, location, it, "neq_null");
            break;
        }
        case op_stricteq: {
            printBinaryOp(out, exec, location, it, "stricteq");
            break;
        }
        case op_nstricteq: {
            printBinaryOp(out, exec, location, it, "nstricteq");
            break;
        }
        case op_less: {
            printBinaryOp(out, exec, location, it, "less");
            break;
        }
        case op_lesseq: {
            printBinaryOp(out, exec, location, it, "lesseq");
            break;
        }
        case op_greater: {
            printBinaryOp(out, exec, location, it, "greater");
            break;
        }
        case op_greatereq: {
            printBinaryOp(out, exec, location, it, "greatereq");
            break;
        }
        case op_inc: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "inc", r0);
            break;
        }
        case op_dec: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "dec", r0);
            break;
        }
        case op_to_number: {
            printUnaryOp(out, exec, location, it, "to_number");
            break;
        }
        case op_negate: {
            printUnaryOp(out, exec, location, it, "negate");
            break;
        }
        case op_add: {
            printBinaryOp(out, exec, location, it, "add");
            ++it;
            break;
        }
        case op_mul: {
            printBinaryOp(out, exec, location, it, "mul");
            ++it;
            break;
        }
        case op_div: {
            printBinaryOp(out, exec, location, it, "div");
            ++it;
            break;
        }
        case op_mod: {
            printBinaryOp(out, exec, location, it, "mod");
            break;
        }
        case op_sub: {
            printBinaryOp(out, exec, location, it, "sub");
            ++it;
            break;
        }
        case op_lshift: {
            printBinaryOp(out, exec, location, it, "lshift");
            break;            
        }
        case op_rshift: {
            printBinaryOp(out, exec, location, it, "rshift");
            break;
        }
        case op_urshift: {
            printBinaryOp(out, exec, location, it, "urshift");
            break;
        }
        case op_bitand: {
            printBinaryOp(out, exec, location, it, "bitand");
            ++it;
            break;
        }
        case op_bitxor: {
            printBinaryOp(out, exec, location, it, "bitxor");
            ++it;
            break;
        }
        case op_bitor: {
            printBinaryOp(out, exec, location, it, "bitor");
            ++it;
            break;
        }
        case op_check_has_instance: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "check_has_instance");
            out.printf("%s, %s, %s, %d(->%d)", registerName(r0).data(), registerName(r1).data(), registerName(r2).data(), offset, location + offset);
            break;
        }
        case op_instanceof: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "instanceof");
            out.printf("%s, %s, %s", registerName(r0).data(), registerName(r1).data(), registerName(r2).data());
            break;
        }
        case op_unsigned: {
            printUnaryOp(out, exec, location, it, "unsigned");
            break;
        }
        case op_typeof: {
            printUnaryOp(out, exec, location, it, "typeof");
            break;
        }
        case op_is_undefined: {
            printUnaryOp(out, exec, location, it, "is_undefined");
            break;
        }
        case op_is_boolean: {
            printUnaryOp(out, exec, location, it, "is_boolean");
            break;
        }
        case op_is_number: {
            printUnaryOp(out, exec, location, it, "is_number");
            break;
        }
        case op_is_string: {
            printUnaryOp(out, exec, location, it, "is_string");
            break;
        }
        case op_is_object: {
            printUnaryOp(out, exec, location, it, "is_object");
            break;
        }
        case op_is_function: {
            printUnaryOp(out, exec, location, it, "is_function");
            break;
        }
        case op_in: {
            printBinaryOp(out, exec, location, it, "in");
            break;
        }
        case op_init_global_const_nop: {
            printLocationAndOp(out, exec, location, it, "init_global_const_nop");
            it++;
            it++;
            it++;
            it++;
            break;
        }
        case op_init_global_const: {
            WriteBarrier<Unknown>* registerPointer = (++it)->u.registerPointer;
            int r0 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "init_global_const");
            out.printf("g%d(%p), %s", m_globalObject->findRegisterIndex(registerPointer), registerPointer, registerName(r0).data());
            it++;
            it++;
            break;
        }
        case op_get_by_id:
        case op_get_by_id_out_of_line:
        case op_get_by_id_self:
        case op_get_by_id_proto:
        case op_get_by_id_chain:
        case op_get_by_id_getter_self:
        case op_get_by_id_getter_proto:
        case op_get_by_id_getter_chain:
        case op_get_by_id_custom_self:
        case op_get_by_id_custom_proto:
        case op_get_by_id_custom_chain:
        case op_get_by_id_generic:
        case op_get_array_length:
        case op_get_string_length: {
            printGetByIdOp(out, exec, location, it);
            printGetByIdCacheStatus(out, exec, location, map);
            dumpValueProfiling(out, it, hasPrintedProfiling);
            break;
        }
        case op_get_arguments_length: {
            printUnaryOp(out, exec, location, it, "get_arguments_length");
            it++;
            break;
        }
        case op_put_by_id: {
            printPutByIdOp(out, exec, location, it, "put_by_id");
            break;
        }
        case op_put_by_id_out_of_line: {
            printPutByIdOp(out, exec, location, it, "put_by_id_out_of_line");
            break;
        }
        case op_put_by_id_replace: {
            printPutByIdOp(out, exec, location, it, "put_by_id_replace");
            break;
        }
        case op_put_by_id_transition: {
            printPutByIdOp(out, exec, location, it, "put_by_id_transition");
            break;
        }
        case op_put_by_id_transition_direct: {
            printPutByIdOp(out, exec, location, it, "put_by_id_transition_direct");
            break;
        }
        case op_put_by_id_transition_direct_out_of_line: {
            printPutByIdOp(out, exec, location, it, "put_by_id_transition_direct_out_of_line");
            break;
        }
        case op_put_by_id_transition_normal: {
            printPutByIdOp(out, exec, location, it, "put_by_id_transition_normal");
            break;
        }
        case op_put_by_id_transition_normal_out_of_line: {
            printPutByIdOp(out, exec, location, it, "put_by_id_transition_normal_out_of_line");
            break;
        }
        case op_put_by_id_generic: {
            printPutByIdOp(out, exec, location, it, "put_by_id_generic");
            break;
        }
        case op_put_getter_setter: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "put_getter_setter");
            out.printf("%s, %s, %s, %s", registerName(r0).data(), idName(id0, identifier(id0)).data(), registerName(r1).data(), registerName(r2).data());
            break;
        }
        case op_del_by_id: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "del_by_id");
            out.printf("%s, %s, %s", registerName(r0).data(), registerName(r1).data(), idName(id0, identifier(id0)).data());
            break;
        }
        case op_get_by_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "get_by_val");
            out.printf("%s, %s, %s", registerName(r0).data(), registerName(r1).data(), registerName(r2).data());
            dumpArrayProfiling(out, it, hasPrintedProfiling);
            dumpValueProfiling(out, it, hasPrintedProfiling);
            break;
        }
        case op_get_argument_by_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "get_argument_by_val");
            out.printf("%s, %s, %s", registerName(r0).data(), registerName(r1).data(), registerName(r2).data());
            ++it;
            dumpValueProfiling(out, it, hasPrintedProfiling);
            break;
        }
        case op_get_by_pname: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            int r3 = (++it)->u.operand;
            int r4 = (++it)->u.operand;
            int r5 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "get_by_pname");
            out.printf("%s, %s, %s, %s, %s, %s", registerName(r0).data(), registerName(r1).data(), registerName(r2).data(), registerName(r3).data(), registerName(r4).data(), registerName(r5).data());
            break;
        }
        case op_put_by_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "put_by_val");
            out.printf("%s, %s, %s", registerName(r0).data(), registerName(r1).data(), registerName(r2).data());
            dumpArrayProfiling(out, it, hasPrintedProfiling);
            break;
        }
        case op_put_by_val_direct: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "put_by_val_direct");
            out.printf("%s, %s, %s", registerName(r0).data(), registerName(r1).data(), registerName(r2).data());
            dumpArrayProfiling(out, it, hasPrintedProfiling);
            break;
        }
        case op_del_by_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "del_by_val");
            out.printf("%s, %s, %s", registerName(r0).data(), registerName(r1).data(), registerName(r2).data());
            break;
        }
        case op_put_by_index: {
            int r0 = (++it)->u.operand;
            unsigned n0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "put_by_index");
            out.printf("%s, %u, %s", registerName(r0).data(), n0, registerName(r1).data());
            break;
        }
        case op_jmp: {
            int offset = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "jmp");
            out.printf("%d(->%d)", offset, location + offset);
            break;
        }
        case op_jtrue: {
            printConditionalJump(out, exec, begin, it, location, "jtrue");
            break;
        }
        case op_jfalse: {
            printConditionalJump(out, exec, begin, it, location, "jfalse");
            break;
        }
        case op_jeq_null: {
            printConditionalJump(out, exec, begin, it, location, "jeq_null");
            break;
        }
        case op_jneq_null: {
            printConditionalJump(out, exec, begin, it, location, "jneq_null");
            break;
        }
        case op_jneq_ptr: {
            int r0 = (++it)->u.operand;
            Special::Pointer pointer = (++it)->u.specialPointer;
            int offset = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "jneq_ptr");
            out.printf("%s, %d (%p), %d(->%d)", registerName(r0).data(), pointer, m_globalObject->actualPointerFor(pointer), offset, location + offset);
            break;
        }
        case op_jless: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "jless");
            out.printf("%s, %s, %d(->%d)", registerName(r0).data(), registerName(r1).data(), offset, location + offset);
            break;
        }
        case op_jlesseq: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "jlesseq");
            out.printf("%s, %s, %d(->%d)", registerName(r0).data(), registerName(r1).data(), offset, location + offset);
            break;
        }
        case op_jgreater: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "jgreater");
            out.printf("%s, %s, %d(->%d)", registerName(r0).data(), registerName(r1).data(), offset, location + offset);
            break;
        }
        case op_jgreatereq: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "jgreatereq");
            out.printf("%s, %s, %d(->%d)", registerName(r0).data(), registerName(r1).data(), offset, location + offset);
            break;
        }
        case op_jnless: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "jnless");
            out.printf("%s, %s, %d(->%d)", registerName(r0).data(), registerName(r1).data(), offset, location + offset);
            break;
        }
        case op_jnlesseq: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "jnlesseq");
            out.printf("%s, %s, %d(->%d)", registerName(r0).data(), registerName(r1).data(), offset, location + offset);
            break;
        }
        case op_jngreater: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "jngreater");
            out.printf("%s, %s, %d(->%d)", registerName(r0).data(), registerName(r1).data(), offset, location + offset);
            break;
        }
        case op_jngreatereq: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "jngreatereq");
            out.printf("%s, %s, %d(->%d)", registerName(r0).data(), registerName(r1).data(), offset, location + offset);
            break;
        }
        case op_loop_hint: {
            printLocationAndOp(out, exec, location, it, "loop_hint");
            break;
        }
        case op_switch_imm: {
            int tableIndex = (++it)->u.operand;
            int defaultTarget = (++it)->u.operand;
            int scrutineeRegister = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "switch_imm");
            out.printf("%d, %d(->%d), %s", tableIndex, defaultTarget, location + defaultTarget, registerName(scrutineeRegister).data());
            break;
        }
        case op_switch_char: {
            int tableIndex = (++it)->u.operand;
            int defaultTarget = (++it)->u.operand;
            int scrutineeRegister = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "switch_char");
            out.printf("%d, %d(->%d), %s", tableIndex, defaultTarget, location + defaultTarget, registerName(scrutineeRegister).data());
            break;
        }
        case op_switch_string: {
            int tableIndex = (++it)->u.operand;
            int defaultTarget = (++it)->u.operand;
            int scrutineeRegister = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "switch_string");
            out.printf("%d, %d(->%d), %s", tableIndex, defaultTarget, location + defaultTarget, registerName(scrutineeRegister).data());
            break;
        }
        case op_new_func: {
            int r0 = (++it)->u.operand;
            int f0 = (++it)->u.operand;
            int shouldCheck = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "new_func");
            out.printf("%s, f%d, %s", registerName(r0).data(), f0, shouldCheck ? "<Checked>" : "<Unchecked>");
            break;
        }
        case op_new_captured_func: {
            int r0 = (++it)->u.operand;
            int f0 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "new_captured_func");
            out.printf("%s, f%d", registerName(r0).data(), f0);
            ++it;
            break;
        }
        case op_new_func_exp: {
            int r0 = (++it)->u.operand;
            int f0 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "new_func_exp");
            out.printf("%s, f%d", registerName(r0).data(), f0);
            break;
        }
        case op_call: {
            printCallOp(out, exec, location, it, "call", DumpCaches, hasPrintedProfiling);
            break;
        }
        case op_call_eval: {
            printCallOp(out, exec, location, it, "call_eval", DontDumpCaches, hasPrintedProfiling);
            break;
        }
        case op_call_varargs: {
            int result = (++it)->u.operand;
            int callee = (++it)->u.operand;
            int thisValue = (++it)->u.operand;
            int arguments = (++it)->u.operand;
            int firstFreeRegister = (++it)->u.operand;
            ++it;
            printLocationAndOp(out, exec, location, it, "call_varargs");
            out.printf("%s, %s, %s, %s, %d", registerName(result).data(), registerName(callee).data(), registerName(thisValue).data(), registerName(arguments).data(), firstFreeRegister);
            dumpValueProfiling(out, it, hasPrintedProfiling);
            break;
        }
        case op_tear_off_activation: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "tear_off_activation", r0);
            break;
        }
        case op_tear_off_arguments: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "tear_off_arguments");
            out.printf("%s, %s", registerName(r0).data(), registerName(r1).data());
            break;
        }
        case op_ret: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "ret", r0);
            break;
        }
        case op_ret_object_or_this: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "constructor_ret");
            out.printf("%s %s", registerName(r0).data(), registerName(r1).data());
            break;
        }
        case op_construct: {
            printCallOp(out, exec, location, it, "construct", DumpCaches, hasPrintedProfiling);
            break;
        }
        case op_strcat: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int count = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "strcat");
            out.printf("%s, %s, %d", registerName(r0).data(), registerName(r1).data(), count);
            break;
        }
        case op_to_primitive: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "to_primitive");
            out.printf("%s, %s", registerName(r0).data(), registerName(r1).data());
            break;
        }
        case op_get_pnames: {
            int r0 = it[1].u.operand;
            int r1 = it[2].u.operand;
            int r2 = it[3].u.operand;
            int r3 = it[4].u.operand;
            int offset = it[5].u.operand;
            printLocationAndOp(out, exec, location, it, "get_pnames");
            out.printf("%s, %s, %s, %s, %d(->%d)", registerName(r0).data(), registerName(r1).data(), registerName(r2).data(), registerName(r3).data(), offset, location + offset);
            it += OPCODE_LENGTH(op_get_pnames) - 1;
            break;
        }
        case op_next_pname: {
            int dest = it[1].u.operand;
            int base = it[2].u.operand;
            int i = it[3].u.operand;
            int size = it[4].u.operand;
            int iter = it[5].u.operand;
            int offset = it[6].u.operand;
            printLocationAndOp(out, exec, location, it, "next_pname");
            out.printf("%s, %s, %s, %s, %s, %d(->%d)", registerName(dest).data(), registerName(base).data(), registerName(i).data(), registerName(size).data(), registerName(iter).data(), offset, location + offset);
            it += OPCODE_LENGTH(op_next_pname) - 1;
            break;
        }
        case op_push_with_scope: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "push_with_scope", r0);
            break;
        }
        case op_pop_scope: {
            printLocationAndOp(out, exec, location, it, "pop_scope");
            break;
        }
        case op_push_name_scope: {
            int id0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            unsigned attributes = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "push_name_scope");
            out.printf("%s, %s, %u", idName(id0, identifier(id0)).data(), registerName(r1).data(), attributes);
            break;
        }
        case op_catch: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "catch", r0);
            break;
        }
        case op_throw: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "throw", r0);
            break;
        }
        case op_throw_static_error: {
            int k0 = (++it)->u.operand;
            int k1 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "throw_static_error");
            out.printf("%s, %s", constantName(k0, getConstant(k0)).data(), k1 ? "true" : "false");
            break;
        }
        case op_debug: {
            int debugHookID = (++it)->u.operand;
            int hasBreakpointFlag = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "debug");
            out.printf("%s %d", debugHookName(debugHookID), hasBreakpointFlag);
            break;
        }
        case op_profile_will_call: {
            int function = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "profile_will_call", function);
            break;
        }
        case op_profile_did_call: {
            int function = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "profile_did_call", function);
            break;
        }
        case op_end: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "end", r0);
            break;
        }
        case op_resolve_scope: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            ResolveModeAndType modeAndType = ResolveModeAndType((++it)->u.operand);
            int depth = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "resolve_scope");
            out.printf("%s, %s, %u<%s|%s>, %d", registerName(r0).data(), idName(id0, identifier(id0)).data(),
                modeAndType.operand(), resolveModeName(modeAndType.mode()), resolveTypeName(modeAndType.type()),
                depth);
            ++it;
            break;
        }
        case op_get_from_scope: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            ResolveModeAndType modeAndType = ResolveModeAndType((++it)->u.operand);
            ++it; // Structure
            int operand = (++it)->u.operand; // Operand
            ++it; // Skip value profile.
            printLocationAndOp(out, exec, location, it, "get_from_scope");
            out.printf("%s, %s, %s, %u<%s|%s>, <structure>, %d",
                registerName(r0).data(), registerName(r1).data(), idName(id0, identifier(id0)).data(),
                modeAndType.operand(), resolveModeName(modeAndType.mode()), resolveTypeName(modeAndType.type()),
                operand);
            break;
        }
        case op_put_to_scope: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            ResolveModeAndType modeAndType = ResolveModeAndType((++it)->u.operand);
            ++it; // Structure
            int operand = (++it)->u.operand; // Operand
            printLocationAndOp(out, exec, location, it, "put_to_scope");
            out.printf("%s, %s, %s, %u<%s|%s>, <structure>, %d",
                registerName(r0).data(), idName(id0, identifier(id0)).data(), registerName(r1).data(),
                modeAndType.operand(), resolveModeName(modeAndType.mode()), resolveTypeName(modeAndType.type()),
                operand);
            break;
        }
#if ENABLE(LLINT_C_LOOP)
        default:
            RELEASE_ASSERT_NOT_REACHED();
#endif
    }

    dumpRareCaseProfile(out, "rare case: ", rareCaseProfileForBytecodeOffset(location), hasPrintedProfiling);
    dumpRareCaseProfile(out, "special fast case: ", specialFastCaseProfileForBytecodeOffset(location), hasPrintedProfiling);
    
#if ENABLE(DFG_JIT)
    Vector<DFG::FrequentExitSite> exitSites = exitProfile().exitSitesFor(location);
    if (!exitSites.isEmpty()) {
        out.print(" !! frequent exits: ");
        CommaPrinter comma;
        for (unsigned i = 0; i < exitSites.size(); ++i)
            out.print(comma, exitSites[i].kind());
    }
#else // ENABLE(DFG_JIT)
    UNUSED_PARAM(location);
#endif // ENABLE(DFG_JIT)
    out.print("\n");
}

void CodeBlock::dumpBytecode(PrintStream& out, unsigned bytecodeOffset)
{
    ExecState* exec = m_globalObject->globalExec();
    const Instruction* it = instructions().begin() + bytecodeOffset;
    dumpBytecode(out, exec, instructions().begin(), it);
}

#define FOR_EACH_MEMBER_VECTOR(macro) \
    macro(instructions) \
    macro(callLinkInfos) \
    macro(linkedCallerList) \
    macro(identifiers) \
    macro(functionExpressions) \
    macro(constantRegisters)

#define FOR_EACH_MEMBER_VECTOR_RARE_DATA(macro) \
    macro(regexps) \
    macro(functions) \
    macro(exceptionHandlers) \
    macro(switchJumpTables) \
    macro(stringSwitchJumpTables) \
    macro(evalCodeCache) \
    macro(expressionInfo) \
    macro(lineInfo) \
    macro(callReturnIndexVector)

template<typename T>
static size_t sizeInBytes(const Vector<T>& vector)
{
    return vector.capacity() * sizeof(T);
}

CodeBlock::CodeBlock(CopyParsedBlockTag, CodeBlock& other)
    : m_globalObject(other.m_globalObject)
    , m_heap(other.m_heap)
    , m_numCalleeRegisters(other.m_numCalleeRegisters)
    , m_numVars(other.m_numVars)
    , m_isConstructor(other.m_isConstructor)
    , m_shouldAlwaysBeInlined(true)
    , m_didFailFTLCompilation(false)
    , m_hasBeenCompiledWithFTL(false)
    , m_unlinkedCode(*other.m_vm, other.m_ownerExecutable.get(), other.m_unlinkedCode.get())
    , m_steppingMode(SteppingModeDisabled)
    , m_numBreakpoints(0)
    , m_ownerExecutable(*other.m_vm, other.m_ownerExecutable.get(), other.m_ownerExecutable.get())
    , m_vm(other.m_vm)
    , m_instructions(other.m_instructions)
    , m_thisRegister(other.m_thisRegister)
    , m_argumentsRegister(other.m_argumentsRegister)
    , m_activationRegister(other.m_activationRegister)
    , m_isStrictMode(other.m_isStrictMode)
    , m_needsActivation(other.m_needsActivation)
    , m_source(other.m_source)
    , m_sourceOffset(other.m_sourceOffset)
    , m_firstLineColumnOffset(other.m_firstLineColumnOffset)
    , m_codeType(other.m_codeType)
    , m_constantRegisters(other.m_constantRegisters)
    , m_functionDecls(other.m_functionDecls)
    , m_functionExprs(other.m_functionExprs)
    , m_osrExitCounter(0)
    , m_optimizationDelayCounter(0)
    , m_reoptimizationRetryCounter(0)
    , m_hash(other.m_hash)
#if ENABLE(JIT)
    , m_capabilityLevelState(DFG::CapabilityLevelNotSet)
#endif
{
    ASSERT(m_heap->isDeferred());
    
    if (SymbolTable* symbolTable = other.symbolTable())
        m_symbolTable.set(*m_vm, m_ownerExecutable.get(), symbolTable);
    
    setNumParameters(other.numParameters());
    optimizeAfterWarmUp();
    jitAfterWarmUp();

    if (other.m_rareData) {
        createRareDataIfNecessary();
        
        m_rareData->m_exceptionHandlers = other.m_rareData->m_exceptionHandlers;
        m_rareData->m_constantBuffers = other.m_rareData->m_constantBuffers;
        m_rareData->m_switchJumpTables = other.m_rareData->m_switchJumpTables;
        m_rareData->m_stringSwitchJumpTables = other.m_rareData->m_stringSwitchJumpTables;
    }
    
    m_heap->m_codeBlocks.add(this);
    m_heap->reportExtraMemoryCost(sizeof(CodeBlock));
}

CodeBlock::CodeBlock(ScriptExecutable* ownerExecutable, UnlinkedCodeBlock* unlinkedCodeBlock, JSScope* scope, PassRefPtr<SourceProvider> sourceProvider, unsigned sourceOffset, unsigned firstLineColumnOffset)
    : m_globalObject(scope->globalObject()->vm(), ownerExecutable, scope->globalObject())
    , m_heap(&m_globalObject->vm().heap)
    , m_numCalleeRegisters(unlinkedCodeBlock->m_numCalleeRegisters)
    , m_numVars(unlinkedCodeBlock->m_numVars)
    , m_isConstructor(unlinkedCodeBlock->isConstructor())
    , m_shouldAlwaysBeInlined(true)
    , m_didFailFTLCompilation(false)
    , m_hasBeenCompiledWithFTL(false)
    , m_unlinkedCode(m_globalObject->vm(), ownerExecutable, unlinkedCodeBlock)
    , m_steppingMode(SteppingModeDisabled)
    , m_numBreakpoints(0)
    , m_ownerExecutable(m_globalObject->vm(), ownerExecutable, ownerExecutable)
    , m_vm(unlinkedCodeBlock->vm())
    , m_thisRegister(unlinkedCodeBlock->thisRegister())
    , m_argumentsRegister(unlinkedCodeBlock->argumentsRegister())
    , m_activationRegister(unlinkedCodeBlock->activationRegister())
    , m_isStrictMode(unlinkedCodeBlock->isStrictMode())
    , m_needsActivation(unlinkedCodeBlock->hasActivationRegister() && unlinkedCodeBlock->codeType() == FunctionCode)
    , m_source(sourceProvider)
    , m_sourceOffset(sourceOffset)
    , m_firstLineColumnOffset(firstLineColumnOffset)
    , m_codeType(unlinkedCodeBlock->codeType())
    , m_osrExitCounter(0)
    , m_optimizationDelayCounter(0)
    , m_reoptimizationRetryCounter(0)
#if ENABLE(JIT)
    , m_capabilityLevelState(DFG::CapabilityLevelNotSet)
#endif
{
    ASSERT(m_heap->isDeferred());

    bool didCloneSymbolTable = false;
    
    if (SymbolTable* symbolTable = unlinkedCodeBlock->symbolTable()) {
        if (codeType() == FunctionCode && symbolTable->captureCount()) {
            m_symbolTable.set(*m_vm, m_ownerExecutable.get(), symbolTable->cloneCapturedNames(*m_vm));
            didCloneSymbolTable = true;
        } else
            m_symbolTable.set(*m_vm, m_ownerExecutable.get(), symbolTable);
    }
    
    ASSERT(m_source);
    setNumParameters(unlinkedCodeBlock->numParameters());

    setConstantRegisters(unlinkedCodeBlock->constantRegisters());
    if (unlinkedCodeBlock->usesGlobalObject())
        m_constantRegisters[unlinkedCodeBlock->globalObjectRegister().offset()].set(*m_vm, ownerExecutable, m_globalObject.get());
    m_functionDecls.resizeToFit(unlinkedCodeBlock->numberOfFunctionDecls());
    for (size_t count = unlinkedCodeBlock->numberOfFunctionDecls(), i = 0; i < count; ++i) {
        UnlinkedFunctionExecutable* unlinkedExecutable = unlinkedCodeBlock->functionDecl(i);
        unsigned lineCount = unlinkedExecutable->lineCount();
        unsigned firstLine = ownerExecutable->lineNo() + unlinkedExecutable->firstLineOffset();
        bool startColumnIsOnOwnerStartLine = !unlinkedExecutable->firstLineOffset();
        unsigned startColumn = unlinkedExecutable->unlinkedBodyStartColumn() + (startColumnIsOnOwnerStartLine ? ownerExecutable->startColumn() : 1);
        bool endColumnIsOnStartLine = !lineCount;
        unsigned endColumn = unlinkedExecutable->unlinkedBodyEndColumn() + (endColumnIsOnStartLine ? startColumn : 1);
        unsigned startOffset = sourceOffset + unlinkedExecutable->startOffset();
        unsigned sourceLength = unlinkedExecutable->sourceLength();
        SourceCode code(m_source, startOffset, startOffset + sourceLength, firstLine, startColumn);
        FunctionExecutable* executable = FunctionExecutable::create(*m_vm, code, unlinkedExecutable, firstLine, firstLine + lineCount, startColumn, endColumn);
        m_functionDecls[i].set(*m_vm, ownerExecutable, executable);
    }

    m_functionExprs.resizeToFit(unlinkedCodeBlock->numberOfFunctionExprs());
    for (size_t count = unlinkedCodeBlock->numberOfFunctionExprs(), i = 0; i < count; ++i) {
        UnlinkedFunctionExecutable* unlinkedExecutable = unlinkedCodeBlock->functionExpr(i);
        unsigned lineCount = unlinkedExecutable->lineCount();
        unsigned firstLine = ownerExecutable->lineNo() + unlinkedExecutable->firstLineOffset();
        bool startColumnIsOnOwnerStartLine = !unlinkedExecutable->firstLineOffset();
        unsigned startColumn = unlinkedExecutable->unlinkedBodyStartColumn() + (startColumnIsOnOwnerStartLine ? ownerExecutable->startColumn() : 1);
        bool endColumnIsOnStartLine = !lineCount;
        unsigned endColumn = unlinkedExecutable->unlinkedBodyEndColumn() + (endColumnIsOnStartLine ? startColumn : 1);
        unsigned startOffset = sourceOffset + unlinkedExecutable->startOffset();
        unsigned sourceLength = unlinkedExecutable->sourceLength();
        SourceCode code(m_source, startOffset, startOffset + sourceLength, firstLine, startColumn);
        FunctionExecutable* executable = FunctionExecutable::create(*m_vm, code, unlinkedExecutable, firstLine, firstLine + lineCount, startColumn, endColumn);
        m_functionExprs[i].set(*m_vm, ownerExecutable, executable);
    }

    if (unlinkedCodeBlock->hasRareData()) {
        createRareDataIfNecessary();
        if (size_t count = unlinkedCodeBlock->constantBufferCount()) {
            m_rareData->m_constantBuffers.grow(count);
            for (size_t i = 0; i < count; i++) {
                const UnlinkedCodeBlock::ConstantBuffer& buffer = unlinkedCodeBlock->constantBuffer(i);
                m_rareData->m_constantBuffers[i] = buffer;
            }
        }
        if (size_t count = unlinkedCodeBlock->numberOfExceptionHandlers()) {
            m_rareData->m_exceptionHandlers.resizeToFit(count);
            size_t nonLocalScopeDepth = scope->depth();
            for (size_t i = 0; i < count; i++) {
                const UnlinkedHandlerInfo& handler = unlinkedCodeBlock->exceptionHandler(i);
                m_rareData->m_exceptionHandlers[i].start = handler.start;
                m_rareData->m_exceptionHandlers[i].end = handler.end;
                m_rareData->m_exceptionHandlers[i].target = handler.target;
                m_rareData->m_exceptionHandlers[i].scopeDepth = nonLocalScopeDepth + handler.scopeDepth;
#if ENABLE(JIT) && ENABLE(LLINT)
                m_rareData->m_exceptionHandlers[i].nativeCode = CodeLocationLabel(MacroAssemblerCodePtr::createFromExecutableAddress(LLInt::getCodePtr(llint_op_catch)));
#endif
            }
        }

        if (size_t count = unlinkedCodeBlock->numberOfStringSwitchJumpTables()) {
            m_rareData->m_stringSwitchJumpTables.grow(count);
            for (size_t i = 0; i < count; i++) {
                UnlinkedStringJumpTable::StringOffsetTable::iterator ptr = unlinkedCodeBlock->stringSwitchJumpTable(i).offsetTable.begin();
                UnlinkedStringJumpTable::StringOffsetTable::iterator end = unlinkedCodeBlock->stringSwitchJumpTable(i).offsetTable.end();
                for (; ptr != end; ++ptr) {
                    OffsetLocation offset;
                    offset.branchOffset = ptr->value;
                    m_rareData->m_stringSwitchJumpTables[i].offsetTable.add(ptr->key, offset);
                }
            }
        }

        if (size_t count = unlinkedCodeBlock->numberOfSwitchJumpTables()) {
            m_rareData->m_switchJumpTables.grow(count);
            for (size_t i = 0; i < count; i++) {
                UnlinkedSimpleJumpTable& sourceTable = unlinkedCodeBlock->switchJumpTable(i);
                SimpleJumpTable& destTable = m_rareData->m_switchJumpTables[i];
                destTable.branchOffsets = sourceTable.branchOffsets;
                destTable.min = sourceTable.min;
            }
        }
    }

    // Allocate metadata buffers for the bytecode
#if ENABLE(LLINT)
    if (size_t size = unlinkedCodeBlock->numberOfLLintCallLinkInfos())
        m_llintCallLinkInfos.resizeToFit(size);
#endif
    if (size_t size = unlinkedCodeBlock->numberOfArrayProfiles())
        m_arrayProfiles.grow(size);
    if (size_t size = unlinkedCodeBlock->numberOfArrayAllocationProfiles())
        m_arrayAllocationProfiles.resizeToFit(size);
    if (size_t size = unlinkedCodeBlock->numberOfValueProfiles())
        m_valueProfiles.resizeToFit(size);
    if (size_t size = unlinkedCodeBlock->numberOfObjectAllocationProfiles())
        m_objectAllocationProfiles.resizeToFit(size);

    // Copy and translate the UnlinkedInstructions
    unsigned instructionCount = unlinkedCodeBlock->instructions().count();
    UnlinkedInstructionStream::Reader instructionReader(unlinkedCodeBlock->instructions());

    Vector<Instruction, 0, UnsafeVectorOverflow> instructions(instructionCount);
    for (unsigned i = 0; !instructionReader.atEnd(); ) {
        const UnlinkedInstruction* pc = instructionReader.next();

        unsigned opLength = opcodeLength(pc[0].u.opcode);

        instructions[i] = vm()->interpreter->getOpcode(pc[0].u.opcode);
        for (size_t j = 1; j < opLength; ++j) {
            if (sizeof(int32_t) != sizeof(intptr_t))
                instructions[i + j].u.pointer = 0;
            instructions[i + j].u.operand = pc[j].u.operand;
        }
        switch (pc[0].u.opcode) {
        case op_call_varargs:
        case op_get_by_val:
        case op_get_argument_by_val: {
            int arrayProfileIndex = pc[opLength - 2].u.operand;
            m_arrayProfiles[arrayProfileIndex] = ArrayProfile(i);

            instructions[i + opLength - 2] = &m_arrayProfiles[arrayProfileIndex];
            FALLTHROUGH;
        }
        case op_get_by_id: {
            ValueProfile* profile = &m_valueProfiles[pc[opLength - 1].u.operand];
            ASSERT(profile->m_bytecodeOffset == -1);
            profile->m_bytecodeOffset = i;
            instructions[i + opLength - 1] = profile;
            break;
        }
        case op_put_by_val: {
            int arrayProfileIndex = pc[opLength - 1].u.operand;
            m_arrayProfiles[arrayProfileIndex] = ArrayProfile(i);
            instructions[i + opLength - 1] = &m_arrayProfiles[arrayProfileIndex];
            break;
        }
        case op_put_by_val_direct: {
            int arrayProfileIndex = pc[opLength - 1].u.operand;
            m_arrayProfiles[arrayProfileIndex] = ArrayProfile(i);
            instructions[i + opLength - 1] = &m_arrayProfiles[arrayProfileIndex];
            break;
        }

        case op_new_array:
        case op_new_array_buffer:
        case op_new_array_with_size: {
            int arrayAllocationProfileIndex = pc[opLength - 1].u.operand;
            instructions[i + opLength - 1] = &m_arrayAllocationProfiles[arrayAllocationProfileIndex];
            break;
        }
        case op_new_object: {
            int objectAllocationProfileIndex = pc[opLength - 1].u.operand;
            ObjectAllocationProfile* objectAllocationProfile = &m_objectAllocationProfiles[objectAllocationProfileIndex];
            int inferredInlineCapacity = pc[opLength - 2].u.operand;

            instructions[i + opLength - 1] = objectAllocationProfile;
            objectAllocationProfile->initialize(*vm(),
                m_ownerExecutable.get(), m_globalObject->objectPrototype(), inferredInlineCapacity);
            break;
        }

        case op_call:
        case op_call_eval: {
            ValueProfile* profile = &m_valueProfiles[pc[opLength - 1].u.operand];
            ASSERT(profile->m_bytecodeOffset == -1);
            profile->m_bytecodeOffset = i;
            instructions[i + opLength - 1] = profile;
            int arrayProfileIndex = pc[opLength - 2].u.operand;
            m_arrayProfiles[arrayProfileIndex] = ArrayProfile(i);
            instructions[i + opLength - 2] = &m_arrayProfiles[arrayProfileIndex];
#if ENABLE(LLINT)
            instructions[i + 5] = &m_llintCallLinkInfos[pc[5].u.operand];
#endif
            break;
        }
        case op_construct: {
#if ENABLE(LLINT)
            instructions[i + 5] = &m_llintCallLinkInfos[pc[5].u.operand];
#endif
            ValueProfile* profile = &m_valueProfiles[pc[opLength - 1].u.operand];
            ASSERT(profile->m_bytecodeOffset == -1);
            profile->m_bytecodeOffset = i;
            instructions[i + opLength - 1] = profile;
            break;
        }
        case op_get_by_id_out_of_line:
        case op_get_by_id_self:
        case op_get_by_id_proto:
        case op_get_by_id_chain:
        case op_get_by_id_getter_self:
        case op_get_by_id_getter_proto:
        case op_get_by_id_getter_chain:
        case op_get_by_id_custom_self:
        case op_get_by_id_custom_proto:
        case op_get_by_id_custom_chain:
        case op_get_by_id_generic:
        case op_get_array_length:
        case op_get_string_length:
            CRASH();

        case op_init_global_const_nop: {
            ASSERT(codeType() == GlobalCode);
            Identifier ident = identifier(pc[4].u.operand);
            SymbolTableEntry entry = m_globalObject->symbolTable()->get(ident.impl());
            if (entry.isNull())
                break;

            instructions[i + 0] = vm()->interpreter->getOpcode(op_init_global_const);
            instructions[i + 1] = &m_globalObject->registerAt(entry.getIndex());
            break;
        }

        case op_resolve_scope: {
            const Identifier& ident = identifier(pc[2].u.operand);
            ResolveType type = static_cast<ResolveType>(pc[3].u.operand);

            ResolveOp op = JSScope::abstractResolve(m_globalObject->globalExec(), scope, ident, Get, type);
            instructions[i + 3].u.operand = op.type;
            instructions[i + 4].u.operand = op.depth;
            if (op.activation)
                instructions[i + 5].u.activation.set(*vm(), ownerExecutable, op.activation);
            break;
        }

        case op_get_from_scope: {
            ValueProfile* profile = &m_valueProfiles[pc[opLength - 1].u.operand];
            ASSERT(profile->m_bytecodeOffset == -1);
            profile->m_bytecodeOffset = i;
            instructions[i + opLength - 1] = profile;

            // get_from_scope dst, scope, id, ResolveModeAndType, Structure, Operand
            const Identifier& ident = identifier(pc[3].u.operand);
            ResolveModeAndType modeAndType = ResolveModeAndType(pc[4].u.operand);
            ResolveOp op = JSScope::abstractResolve(m_globalObject->globalExec(), scope, ident, Get, modeAndType.type());

            instructions[i + 4].u.operand = ResolveModeAndType(modeAndType.mode(), op.type).operand();
            if (op.type == GlobalVar || op.type == GlobalVarWithVarInjectionChecks)
                instructions[i + 5].u.watchpointSet = op.watchpointSet;
            else if (op.structure)
                instructions[i + 5].u.structure.set(*vm(), ownerExecutable, op.structure);
            instructions[i + 6].u.pointer = reinterpret_cast<void*>(op.operand);
            break;
        }

        case op_put_to_scope: {
            // put_to_scope scope, id, value, ResolveModeAndType, Structure, Operand
            const Identifier& ident = identifier(pc[2].u.operand);
            ResolveModeAndType modeAndType = ResolveModeAndType(pc[4].u.operand);
            ResolveOp op = JSScope::abstractResolve(m_globalObject->globalExec(), scope, ident, Put, modeAndType.type());

            instructions[i + 4].u.operand = ResolveModeAndType(modeAndType.mode(), op.type).operand();
            if (op.type == GlobalVar || op.type == GlobalVarWithVarInjectionChecks)
                instructions[i + 5].u.watchpointSet = op.watchpointSet;
            else if (op.type == ClosureVar || op.type == ClosureVarWithVarInjectionChecks) {
                if (op.watchpointSet)
                    op.watchpointSet->invalidate();
            } else if (op.structure)
                instructions[i + 5].u.structure.set(*vm(), ownerExecutable, op.structure);
            instructions[i + 6].u.pointer = reinterpret_cast<void*>(op.operand);
            break;
        }
            
        case op_captured_mov:
        case op_new_captured_func: {
            if (pc[3].u.index == UINT_MAX) {
                instructions[i + 3].u.watchpointSet = 0;
                break;
            }
            StringImpl* uid = identifier(pc[3].u.index).impl();
            RELEASE_ASSERT(didCloneSymbolTable);
            ConcurrentJITLocker locker(m_symbolTable->m_lock);
            SymbolTable::Map::iterator iter = m_symbolTable->find(locker, uid);
            ASSERT(iter != m_symbolTable->end(locker));
            iter->value.prepareToWatch();
            instructions[i + 3].u.watchpointSet = iter->value.watchpointSet();
            break;
        }

        default:
            break;
        }
        i += opLength;
    }
    m_instructions = WTF::RefCountedArray<Instruction>(instructions);

    // Set optimization thresholds only after m_instructions is initialized, since these
    // rely on the instruction count (and are in theory permitted to also inspect the
    // instruction stream to more accurate assess the cost of tier-up).
    optimizeAfterWarmUp();
    jitAfterWarmUp();

    // If the concurrent thread will want the code block's hash, then compute it here
    // synchronously.
    if (Options::alwaysComputeHash())
        hash();

    if (Options::dumpGeneratedBytecodes())
        dumpBytecode();
    
    m_heap->m_codeBlocks.add(this);
    m_heap->reportExtraMemoryCost(sizeof(CodeBlock) + m_instructions.size() * sizeof(Instruction));
}

CodeBlock::~CodeBlock()
{
    if (m_vm->m_perBytecodeProfiler)
        m_vm->m_perBytecodeProfiler->notifyDestruction(this);
    
#if ENABLE(VERBOSE_VALUE_PROFILE)
    dumpValueProfiles();
#endif

#if ENABLE(LLINT)    
    while (m_incomingLLIntCalls.begin() != m_incomingLLIntCalls.end())
        m_incomingLLIntCalls.begin()->remove();
#endif // ENABLE(LLINT)
#if ENABLE(JIT)
    // We may be destroyed before any CodeBlocks that refer to us are destroyed.
    // Consider that two CodeBlocks become unreachable at the same time. There
    // is no guarantee about the order in which the CodeBlocks are destroyed.
    // So, if we don't remove incoming calls, and get destroyed before the
    // CodeBlock(s) that have calls into us, then the CallLinkInfo vector's
    // destructor will try to remove nodes from our (no longer valid) linked list.
    while (m_incomingCalls.begin() != m_incomingCalls.end())
        m_incomingCalls.begin()->remove();
    
    // Note that our outgoing calls will be removed from other CodeBlocks'
    // m_incomingCalls linked lists through the execution of the ~CallLinkInfo
    // destructors.

    for (Bag<StructureStubInfo>::iterator iter = m_stubInfos.begin(); !!iter; ++iter)
        (*iter)->deref();
#endif // ENABLE(JIT)
}

void CodeBlock::setNumParameters(int newValue)
{
    m_numParameters = newValue;

    m_argumentValueProfiles.resizeToFit(newValue);
}

void EvalCodeCache::visitAggregate(SlotVisitor& visitor)
{
    EvalCacheMap::iterator end = m_cacheMap.end();
    for (EvalCacheMap::iterator ptr = m_cacheMap.begin(); ptr != end; ++ptr)
        visitor.append(&ptr->value);
}

CodeBlock* CodeBlock::specialOSREntryBlockOrNull()
{
#if ENABLE(FTL_JIT)
    if (jitType() != JITCode::DFGJIT)
        return 0;
    DFG::JITCode* jitCode = m_jitCode->dfg();
    return jitCode->osrEntryBlock.get();
#else // ENABLE(FTL_JIT)
    return 0;
#endif // ENABLE(FTL_JIT)
}

void CodeBlock::visitAggregate(SlotVisitor& visitor)
{
#if ENABLE(PARALLEL_GC)
    // I may be asked to scan myself more than once, and it may even happen concurrently.
    // To this end, use a CAS loop to check if I've been called already. Only one thread
    // may proceed past this point - whichever one wins the CAS race.
    unsigned oldValue;
    do {
        oldValue = m_visitAggregateHasBeenCalled;
        if (oldValue) {
            // Looks like someone else won! Return immediately to ensure that we don't
            // trace the same CodeBlock concurrently. Doing so is hazardous since we will
            // be mutating the state of ValueProfiles, which contain JSValues, which can
            // have word-tearing on 32-bit, leading to awesome timing-dependent crashes
            // that are nearly impossible to track down.
            
            // Also note that it must be safe to return early as soon as we see the
            // value true (well, (unsigned)1), since once a GC thread is in this method
            // and has won the CAS race (i.e. was responsible for setting the value true)
            // it will definitely complete the rest of this method before declaring
            // termination.
            return;
        }
    } while (!WTF::weakCompareAndSwap(&m_visitAggregateHasBeenCalled, 0, 1));
#endif // ENABLE(PARALLEL_GC)
    
    if (!!m_alternative)
        m_alternative->visitAggregate(visitor);
    
    if (CodeBlock* otherBlock = specialOSREntryBlockOrNull())
        otherBlock->visitAggregate(visitor);

    visitor.reportExtraMemoryUsage(ownerExecutable(), sizeof(CodeBlock));
    if (m_jitCode)
        visitor.reportExtraMemoryUsage(ownerExecutable(), m_jitCode->size());
    if (m_instructions.size()) {
        // Divide by refCount() because m_instructions points to something that is shared
        // by multiple CodeBlocks, and we only want to count it towards the heap size once.
        // Having each CodeBlock report only its proportional share of the size is one way
        // of accomplishing this.
        visitor.reportExtraMemoryUsage(ownerExecutable(), m_instructions.size() * sizeof(Instruction) / m_instructions.refCount());
    }

    visitor.append(&m_unlinkedCode);

    // There are three things that may use unconditional finalizers: lazy bytecode freeing,
    // inline cache clearing, and jettisoning. The probability of us wanting to do at
    // least one of those things is probably quite close to 1. So we add one no matter what
    // and when it runs, it figures out whether it has any work to do.
    visitor.addUnconditionalFinalizer(this);
    
    // There are two things that we use weak reference harvesters for: DFG fixpoint for
    // jettisoning, and trying to find structures that would be live based on some
    // inline cache. So it makes sense to register them regardless.
    visitor.addWeakReferenceHarvester(this);
    m_allTransitionsHaveBeenMarked = false;
    
    if (shouldImmediatelyAssumeLivenessDuringScan()) {
        // This code block is live, so scan all references strongly and return.
        stronglyVisitStrongReferences(visitor);
        stronglyVisitWeakReferences(visitor);
        propagateTransitions(visitor);
        return;
    }
    
#if ENABLE(DFG_JIT)
    // We get here if we're live in the sense that our owner executable is live,
    // but we're not yet live for sure in another sense: we may yet decide that this
    // code block should be jettisoned based on its outgoing weak references being
    // stale. Set a flag to indicate that we're still assuming that we're dead, and
    // perform one round of determining if we're live. The GC may determine, based on
    // either us marking additional objects, or by other objects being marked for
    // other reasons, that this iteration should run again; it will notify us of this
    // decision by calling harvestWeakReferences().
    
    m_jitCode->dfgCommon()->livenessHasBeenProved = false;
    
    propagateTransitions(visitor);
    determineLiveness(visitor);
#else // ENABLE(DFG_JIT)
    RELEASE_ASSERT_NOT_REACHED();
#endif // ENABLE(DFG_JIT)
}

void CodeBlock::propagateTransitions(SlotVisitor& visitor)
{
    UNUSED_PARAM(visitor);

    if (m_allTransitionsHaveBeenMarked)
        return;

    bool allAreMarkedSoFar = true;
        
#if ENABLE(LLINT)
    Interpreter* interpreter = m_vm->interpreter;
    if (jitType() == JITCode::InterpreterThunk) {
        const Vector<unsigned>& propertyAccessInstructions = m_unlinkedCode->propertyAccessInstructions();
        for (size_t i = 0; i < propertyAccessInstructions.size(); ++i) {
            Instruction* instruction = &instructions()[propertyAccessInstructions[i]];
            switch (interpreter->getOpcodeID(instruction[0].u.opcode)) {
            case op_put_by_id_transition_direct:
            case op_put_by_id_transition_normal:
            case op_put_by_id_transition_direct_out_of_line:
            case op_put_by_id_transition_normal_out_of_line: {
                if (Heap::isMarked(instruction[4].u.structure.get()))
                    visitor.append(&instruction[6].u.structure);
                else
                    allAreMarkedSoFar = false;
                break;
            }
            default:
                break;
            }
        }
    }
#endif // ENABLE(LLINT)

#if ENABLE(JIT)
    if (JITCode::isJIT(jitType())) {
        for (Bag<StructureStubInfo>::iterator iter = m_stubInfos.begin(); !!iter; ++iter) {
            StructureStubInfo& stubInfo = **iter;
            switch (stubInfo.accessType) {
            case access_put_by_id_transition_normal:
            case access_put_by_id_transition_direct: {
                JSCell* origin = stubInfo.codeOrigin.codeOriginOwner();
                if ((!origin || Heap::isMarked(origin))
                    && Heap::isMarked(stubInfo.u.putByIdTransition.previousStructure.get()))
                    visitor.append(&stubInfo.u.putByIdTransition.structure);
                else
                    allAreMarkedSoFar = false;
                break;
            }

            case access_put_by_id_list: {
                PolymorphicPutByIdList* list = stubInfo.u.putByIdList.list;
                JSCell* origin = stubInfo.codeOrigin.codeOriginOwner();
                if (origin && !Heap::isMarked(origin)) {
                    allAreMarkedSoFar = false;
                    break;
                }
                for (unsigned j = list->size(); j--;) {
                    PutByIdAccess& access = list->m_list[j];
                    if (!access.isTransition())
                        continue;
                    if (Heap::isMarked(access.oldStructure()))
                        visitor.append(&access.m_newStructure);
                    else
                        allAreMarkedSoFar = false;
                }
                break;
            }
            
            default:
                break;
            }
        }
    }
#endif // ENABLE(JIT)
    
#if ENABLE(DFG_JIT)
    if (JITCode::isOptimizingJIT(jitType())) {
        DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
        for (unsigned i = 0; i < dfgCommon->transitions.size(); ++i) {
            if ((!dfgCommon->transitions[i].m_codeOrigin
                 || Heap::isMarked(dfgCommon->transitions[i].m_codeOrigin.get()))
                && Heap::isMarked(dfgCommon->transitions[i].m_from.get())) {
                // If the following three things are live, then the target of the
                // transition is also live:
                // - This code block. We know it's live already because otherwise
                //   we wouldn't be scanning ourselves.
                // - The code origin of the transition. Transitions may arise from
                //   code that was inlined. They are not relevant if the user's
                //   object that is required for the inlinee to run is no longer
                //   live.
                // - The source of the transition. The transition checks if some
                //   heap location holds the source, and if so, stores the target.
                //   Hence the source must be live for the transition to be live.
                visitor.append(&dfgCommon->transitions[i].m_to);
            } else
                allAreMarkedSoFar = false;
        }
    }
#endif // ENABLE(DFG_JIT)
    
    if (allAreMarkedSoFar)
        m_allTransitionsHaveBeenMarked = true;
}

void CodeBlock::determineLiveness(SlotVisitor& visitor)
{
    UNUSED_PARAM(visitor);
    
    if (shouldImmediatelyAssumeLivenessDuringScan())
        return;
    
#if ENABLE(DFG_JIT)
    // Check if we have any remaining work to do.
    DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
    if (dfgCommon->livenessHasBeenProved)
        return;
    
    // Now check all of our weak references. If all of them are live, then we
    // have proved liveness and so we scan our strong references. If at end of
    // GC we still have not proved liveness, then this code block is toast.
    bool allAreLiveSoFar = true;
    for (unsigned i = 0; i < dfgCommon->weakReferences.size(); ++i) {
        if (!Heap::isMarked(dfgCommon->weakReferences[i].get())) {
            allAreLiveSoFar = false;
            break;
        }
    }
    
    // If some weak references are dead, then this fixpoint iteration was
    // unsuccessful.
    if (!allAreLiveSoFar)
        return;
    
    // All weak references are live. Record this information so we don't
    // come back here again, and scan the strong references.
    dfgCommon->livenessHasBeenProved = true;
    stronglyVisitStrongReferences(visitor);
#endif // ENABLE(DFG_JIT)
}

void CodeBlock::visitWeakReferences(SlotVisitor& visitor)
{
    propagateTransitions(visitor);
    determineLiveness(visitor);
}

void CodeBlock::finalizeUnconditionally()
{
    Interpreter* interpreter = m_vm->interpreter;
    if (JITCode::couldBeInterpreted(jitType())) {
        const Vector<unsigned>& propertyAccessInstructions = m_unlinkedCode->propertyAccessInstructions();
        for (size_t size = propertyAccessInstructions.size(), i = 0; i < size; ++i) {
            Instruction* curInstruction = &instructions()[propertyAccessInstructions[i]];
            switch (interpreter->getOpcodeID(curInstruction[0].u.opcode)) {
            case op_get_by_id:
            case op_get_by_id_out_of_line:
            case op_put_by_id:
            case op_put_by_id_out_of_line:
                if (!curInstruction[4].u.structure || Heap::isMarked(curInstruction[4].u.structure.get()))
                    break;
                if (Options::verboseOSR())
                    dataLogF("Clearing LLInt property access with structure %p.\n", curInstruction[4].u.structure.get());
                curInstruction[4].u.structure.clear();
                curInstruction[5].u.operand = 0;
                break;
            case op_put_by_id_transition_direct:
            case op_put_by_id_transition_normal:
            case op_put_by_id_transition_direct_out_of_line:
            case op_put_by_id_transition_normal_out_of_line:
                if (Heap::isMarked(curInstruction[4].u.structure.get())
                    && Heap::isMarked(curInstruction[6].u.structure.get())
                    && Heap::isMarked(curInstruction[7].u.structureChain.get()))
                    break;
                if (Options::verboseOSR()) {
                    dataLogF("Clearing LLInt put transition with structures %p -> %p, chain %p.\n",
                            curInstruction[4].u.structure.get(),
                            curInstruction[6].u.structure.get(),
                            curInstruction[7].u.structureChain.get());
                }
                curInstruction[4].u.structure.clear();
                curInstruction[6].u.structure.clear();
                curInstruction[7].u.structureChain.clear();
                curInstruction[0].u.opcode = interpreter->getOpcode(op_put_by_id);
                break;
            case op_get_array_length:
                break;
            case op_to_this:
                if (!curInstruction[2].u.structure || Heap::isMarked(curInstruction[2].u.structure.get()))
                    break;
                if (Options::verboseOSR())
                    dataLogF("Clearing LLInt to_this with structure %p.\n", curInstruction[2].u.structure.get());
                curInstruction[2].u.structure.clear();
                break;
            case op_get_callee:
                if (!curInstruction[2].u.jsCell || Heap::isMarked(curInstruction[2].u.jsCell.get()))
                    break;
                if (Options::verboseOSR())
                    dataLogF("Clearing LLInt get callee with function %p.\n", curInstruction[2].u.jsCell.get());
                curInstruction[2].u.jsCell.clear();
                break;
            case op_resolve_scope: {
                WriteBarrierBase<JSActivation>& activation = curInstruction[5].u.activation;
                if (!activation || Heap::isMarked(activation.get()))
                    break;
                if (Options::verboseOSR())
                    dataLogF("Clearing dead activation %p.\n", activation.get());
                activation.clear();
                break;
            }
            case op_get_from_scope:
            case op_put_to_scope: {
                ResolveModeAndType modeAndType =
                    ResolveModeAndType(curInstruction[4].u.operand);
                if (modeAndType.type() == GlobalVar || modeAndType.type() == GlobalVarWithVarInjectionChecks)
                    continue;
                WriteBarrierBase<Structure>& structure = curInstruction[5].u.structure;
                if (!structure || Heap::isMarked(structure.get()))
                    break;
                if (Options::verboseOSR())
                    dataLogF("Clearing scope access with structure %p.\n", structure.get());
                structure.clear();
                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }

#if ENABLE(LLINT)
        for (unsigned i = 0; i < m_llintCallLinkInfos.size(); ++i) {
            if (m_llintCallLinkInfos[i].isLinked() && !Heap::isMarked(m_llintCallLinkInfos[i].callee.get())) {
                if (Options::verboseOSR())
                    dataLog("Clearing LLInt call from ", *this, "\n");
                m_llintCallLinkInfos[i].unlink();
            }
            if (!!m_llintCallLinkInfos[i].lastSeenCallee && !Heap::isMarked(m_llintCallLinkInfos[i].lastSeenCallee.get()))
                m_llintCallLinkInfos[i].lastSeenCallee.clear();
        }
#endif // ENABLE(LLINT)
    }

#if ENABLE(DFG_JIT)
    // Check if we're not live. If we are, then jettison.
    if (!(shouldImmediatelyAssumeLivenessDuringScan() || m_jitCode->dfgCommon()->livenessHasBeenProved)) {
        if (Options::verboseOSR())
            dataLog(*this, " has dead weak references, jettisoning during GC.\n");

        if (DFG::shouldShowDisassembly()) {
            dataLog(*this, " will be jettisoned because of the following dead references:\n");
            DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
            for (unsigned i = 0; i < dfgCommon->transitions.size(); ++i) {
                DFG::WeakReferenceTransition& transition = dfgCommon->transitions[i];
                JSCell* origin = transition.m_codeOrigin.get();
                JSCell* from = transition.m_from.get();
                JSCell* to = transition.m_to.get();
                if ((!origin || Heap::isMarked(origin)) && Heap::isMarked(from))
                    continue;
                dataLog("    Transition under ", RawPointer(origin), ", ", RawPointer(from), " -> ", RawPointer(to), ".\n");
            }
            for (unsigned i = 0; i < dfgCommon->weakReferences.size(); ++i) {
                JSCell* weak = dfgCommon->weakReferences[i].get();
                if (Heap::isMarked(weak))
                    continue;
                dataLog("    Weak reference ", RawPointer(weak), ".\n");
            }
        }
        
        jettison();
        return;
    }
#endif // ENABLE(DFG_JIT)

#if ENABLE(JIT)
    // Handle inline caches.
    if (!!jitCode()) {
        RepatchBuffer repatchBuffer(this);
        for (unsigned i = 0; i < numberOfCallLinkInfos(); ++i) {
            if (callLinkInfo(i).isLinked()) {
                if (ClosureCallStubRoutine* stub = callLinkInfo(i).stub.get()) {
                    if (!Heap::isMarked(stub->structure())
                        || !Heap::isMarked(stub->executable())) {
                        if (Options::verboseOSR()) {
                            dataLog(
                                "Clearing closure call from ", *this, " to ",
                                stub->executable()->hashFor(callLinkInfo(i).specializationKind()),
                                ", stub routine ", RawPointer(stub), ".\n");
                        }
                        callLinkInfo(i).unlink(*m_vm, repatchBuffer);
                    }
                } else if (!Heap::isMarked(callLinkInfo(i).callee.get())) {
                    if (Options::verboseOSR()) {
                        dataLog(
                            "Clearing call from ", *this, " to ",
                            RawPointer(callLinkInfo(i).callee.get()), " (",
                            callLinkInfo(i).callee.get()->executable()->hashFor(
                                callLinkInfo(i).specializationKind()),
                            ").\n");
                    }
                    callLinkInfo(i).unlink(*m_vm, repatchBuffer);
                }
            }
            if (!!callLinkInfo(i).lastSeenCallee
                && !Heap::isMarked(callLinkInfo(i).lastSeenCallee.get()))
                callLinkInfo(i).lastSeenCallee.clear();
        }
        for (Bag<StructureStubInfo>::iterator iter = m_stubInfos.begin(); !!iter; ++iter) {
            StructureStubInfo& stubInfo = **iter;
            
            if (stubInfo.visitWeakReferences())
                continue;
            
            resetStubDuringGCInternal(repatchBuffer, stubInfo);
        }
    }
#endif
}

void CodeBlock::getStubInfoMap(const ConcurrentJITLocker&, StubInfoMap& result)
{
#if ENABLE(JIT)
    toHashMap(m_stubInfos, getStructureStubInfoCodeOrigin, result);
#else
    UNUSED_PARAM(result);
#endif
}

void CodeBlock::getStubInfoMap(StubInfoMap& result)
{
    ConcurrentJITLocker locker(m_lock);
    getStubInfoMap(locker, result);
}

#if ENABLE(JIT)
StructureStubInfo* CodeBlock::addStubInfo()
{
    ConcurrentJITLocker locker(m_lock);
    return m_stubInfos.add();
}

void CodeBlock::resetStub(StructureStubInfo& stubInfo)
{
    if (stubInfo.accessType == access_unset)
        return;
    
    ConcurrentJITLocker locker(m_lock);
    
    RepatchBuffer repatchBuffer(this);
    resetStubInternal(repatchBuffer, stubInfo);
}

void CodeBlock::resetStubInternal(RepatchBuffer& repatchBuffer, StructureStubInfo& stubInfo)
{
    AccessType accessType = static_cast<AccessType>(stubInfo.accessType);
    
    if (Options::verboseOSR()) {
        // This can be called from GC destructor calls, so we don't try to do a full dump
        // of the CodeBlock.
        dataLog("Clearing structure cache (kind ", static_cast<int>(stubInfo.accessType), ") in ", RawPointer(this), ".\n");
    }
    
    RELEASE_ASSERT(JITCode::isJIT(jitType()));
    
    if (isGetByIdAccess(accessType))
        resetGetByID(repatchBuffer, stubInfo);
    else if (isPutByIdAccess(accessType))
        resetPutByID(repatchBuffer, stubInfo);
    else {
        RELEASE_ASSERT(isInAccess(accessType));
        resetIn(repatchBuffer, stubInfo);
    }
    
    stubInfo.reset();
}

void CodeBlock::resetStubDuringGCInternal(RepatchBuffer& repatchBuffer, StructureStubInfo& stubInfo)
{
    resetStubInternal(repatchBuffer, stubInfo);
    stubInfo.resetByGC = true;
}
#endif

void CodeBlock::stronglyVisitStrongReferences(SlotVisitor& visitor)
{
    visitor.append(&m_globalObject);
    visitor.append(&m_ownerExecutable);
    visitor.append(&m_symbolTable);
    visitor.append(&m_unlinkedCode);
    if (m_rareData)
        m_rareData->m_evalCodeCache.visitAggregate(visitor);
    visitor.appendValues(m_constantRegisters.data(), m_constantRegisters.size());
    for (size_t i = 0; i < m_functionExprs.size(); ++i)
        visitor.append(&m_functionExprs[i]);
    for (size_t i = 0; i < m_functionDecls.size(); ++i)
        visitor.append(&m_functionDecls[i]);
    for (unsigned i = 0; i < m_objectAllocationProfiles.size(); ++i)
        m_objectAllocationProfiles[i].visitAggregate(visitor);

    updateAllPredictions();
}

void CodeBlock::stronglyVisitWeakReferences(SlotVisitor& visitor)
{
    UNUSED_PARAM(visitor);

#if ENABLE(DFG_JIT)
    if (!JITCode::isOptimizingJIT(jitType()))
        return;
    
    DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();

    for (unsigned i = 0; i < dfgCommon->transitions.size(); ++i) {
        if (!!dfgCommon->transitions[i].m_codeOrigin)
            visitor.append(&dfgCommon->transitions[i].m_codeOrigin); // Almost certainly not necessary, since the code origin should also be a weak reference. Better to be safe, though.
        visitor.append(&dfgCommon->transitions[i].m_from);
        visitor.append(&dfgCommon->transitions[i].m_to);
    }
    
    for (unsigned i = 0; i < dfgCommon->weakReferences.size(); ++i)
        visitor.append(&dfgCommon->weakReferences[i]);
#endif    
}

CodeBlock* CodeBlock::baselineAlternative()
{
#if ENABLE(JIT)
    CodeBlock* result = this;
    while (result->alternative())
        result = result->alternative();
    RELEASE_ASSERT(result);
    RELEASE_ASSERT(JITCode::isBaselineCode(result->jitType()) || result->jitType() == JITCode::None);
    return result;
#else
    return this;
#endif
}

CodeBlock* CodeBlock::baselineVersion()
{
#if ENABLE(JIT)
    if (JITCode::isBaselineCode(jitType()))
        return this;
    CodeBlock* result = replacement();
    if (!result) {
        // This can happen if we're creating the original CodeBlock for an executable.
        // Assume that we're the baseline CodeBlock.
        RELEASE_ASSERT(jitType() == JITCode::None);
        return this;
    }
    result = result->baselineAlternative();
    return result;
#else
    return this;
#endif
}

#if ENABLE(JIT)
bool CodeBlock::hasOptimizedReplacement(JITCode::JITType typeToReplace)
{
    return JITCode::isHigherTier(replacement()->jitType(), typeToReplace);
}

bool CodeBlock::hasOptimizedReplacement()
{
    return hasOptimizedReplacement(jitType());
}
#endif

bool CodeBlock::isCaptured(VirtualRegister operand, InlineCallFrame* inlineCallFrame) const
{
    if (operand.isArgument())
        return operand.toArgument() && usesArguments();

    if (inlineCallFrame)
        return inlineCallFrame->capturedVars.get(operand.toLocal());

    // The activation object isn't in the captured region, but it's "captured"
    // in the sense that stores to its location can be observed indirectly.
    if (needsActivation() && operand == activationRegister())
        return true;

    // Ditto for the arguments object.
    if (usesArguments() && operand == argumentsRegister())
        return true;

    // Ditto for the arguments object.
    if (usesArguments() && operand == unmodifiedArgumentsRegister(argumentsRegister()))
        return true;

    // We're in global code so there are no locals to capture
    if (!symbolTable())
        return false;

    return symbolTable()->isCaptured(operand.offset());
}

int CodeBlock::framePointerOffsetToGetActivationRegisters(int machineCaptureStart)
{
    // We'll be adding this to the stack pointer to get a registers pointer that looks
    // like it would have looked in the baseline engine. For example, if bytecode would
    // have put the first captured variable at offset -5 but we put it at offset -1, then
    // we'll have an offset of 4.
    int32_t offset = 0;
    
    // Compute where we put the captured variables. This offset will point the registers
    // pointer directly at the first captured var.
    offset += machineCaptureStart;
    
    // Now compute the offset needed to make the runtime see the captured variables at the
    // same offset that the bytecode would have used.
    offset -= symbolTable()->captureStart();
    
    return offset;
}

int CodeBlock::framePointerOffsetToGetActivationRegisters()
{
    if (!JITCode::isOptimizingJIT(jitType()))
        return 0;
#if ENABLE(DFG_JIT)
    return framePointerOffsetToGetActivationRegisters(jitCode()->dfgCommon()->machineCaptureStart);
#else
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
#endif
}

HandlerInfo* CodeBlock::handlerForBytecodeOffset(unsigned bytecodeOffset)
{
    RELEASE_ASSERT(bytecodeOffset < instructions().size());

    if (!m_rareData)
        return 0;
    
    Vector<HandlerInfo>& exceptionHandlers = m_rareData->m_exceptionHandlers;
    for (size_t i = 0; i < exceptionHandlers.size(); ++i) {
        // Handlers are ordered innermost first, so the first handler we encounter
        // that contains the source address is the correct handler to use.
        if (exceptionHandlers[i].start <= bytecodeOffset && exceptionHandlers[i].end > bytecodeOffset)
            return &exceptionHandlers[i];
    }

    return 0;
}

unsigned CodeBlock::lineNumberForBytecodeOffset(unsigned bytecodeOffset)
{
    RELEASE_ASSERT(bytecodeOffset < instructions().size());
    return m_ownerExecutable->lineNo() + m_unlinkedCode->lineNumberForBytecodeOffset(bytecodeOffset);
}

unsigned CodeBlock::columnNumberForBytecodeOffset(unsigned bytecodeOffset)
{
    int divot;
    int startOffset;
    int endOffset;
    unsigned line;
    unsigned column;
    expressionRangeForBytecodeOffset(bytecodeOffset, divot, startOffset, endOffset, line, column);
    return column;
}

void CodeBlock::expressionRangeForBytecodeOffset(unsigned bytecodeOffset, int& divot, int& startOffset, int& endOffset, unsigned& line, unsigned& column)
{
    m_unlinkedCode->expressionRangeForBytecodeOffset(bytecodeOffset, divot, startOffset, endOffset, line, column);
    divot += m_sourceOffset;
    column += line ? 1 : firstLineColumnOffset();
    line += m_ownerExecutable->lineNo();
}

bool CodeBlock::hasOpDebugForLineAndColumn(unsigned line, unsigned column)
{
    Interpreter* interpreter = vm()->interpreter;
    const Instruction* begin = instructions().begin();
    const Instruction* end = instructions().end();
    for (const Instruction* it = begin; it != end;) {
        OpcodeID opcodeID = interpreter->getOpcodeID(it->u.opcode);
        if (opcodeID == op_debug) {
            unsigned bytecodeOffset = it - begin;
            int unused;
            unsigned opDebugLine;
            unsigned opDebugColumn;
            expressionRangeForBytecodeOffset(bytecodeOffset, unused, unused, unused, opDebugLine, opDebugColumn);
            if (line == opDebugLine && (column == Breakpoint::unspecifiedColumn || column == opDebugColumn))
                return true;
        }
        it += opcodeLengths[opcodeID];
    }
    return false;
}

void CodeBlock::shrinkToFit(ShrinkMode shrinkMode)
{
    m_rareCaseProfiles.shrinkToFit();
    m_specialFastCaseProfiles.shrinkToFit();
    
    if (shrinkMode == EarlyShrink) {
        m_constantRegisters.shrinkToFit();
        
        if (m_rareData) {
            m_rareData->m_switchJumpTables.shrinkToFit();
            m_rareData->m_stringSwitchJumpTables.shrinkToFit();
        }
    } // else don't shrink these, because we would have already pointed pointers into these tables.
}

unsigned CodeBlock::addOrFindConstant(JSValue v)
{
    unsigned result;
    if (findConstant(v, result))
        return result;
    return addConstant(v);
}

bool CodeBlock::findConstant(JSValue v, unsigned& index)
{
    unsigned numberOfConstants = numberOfConstantRegisters();
    for (unsigned i = 0; i < numberOfConstants; ++i) {
        if (getConstant(FirstConstantRegisterIndex + i) == v) {
            index = i;
            return true;
        }
    }
    index = numberOfConstants;
    return false;
}

#if ENABLE(JIT)
void CodeBlock::unlinkCalls()
{
    if (!!m_alternative)
        m_alternative->unlinkCalls();
#if ENABLE(LLINT)
    for (size_t i = 0; i < m_llintCallLinkInfos.size(); ++i) {
        if (m_llintCallLinkInfos[i].isLinked())
            m_llintCallLinkInfos[i].unlink();
    }
#endif
    if (!m_callLinkInfos.size())
        return;
    if (!m_vm->canUseJIT())
        return;
    RepatchBuffer repatchBuffer(this);
    for (size_t i = 0; i < m_callLinkInfos.size(); i++) {
        if (!m_callLinkInfos[i].isLinked())
            continue;
        m_callLinkInfos[i].unlink(*m_vm, repatchBuffer);
    }
}

void CodeBlock::linkIncomingCall(ExecState* callerFrame, CallLinkInfo* incoming)
{
    noticeIncomingCall(callerFrame);
    m_incomingCalls.push(incoming);
}
#endif // ENABLE(JIT)

void CodeBlock::unlinkIncomingCalls()
{
#if ENABLE(LLINT)
    while (m_incomingLLIntCalls.begin() != m_incomingLLIntCalls.end())
        m_incomingLLIntCalls.begin()->unlink();
#endif // ENABLE(LLINT)
#if ENABLE(JIT)
    if (m_incomingCalls.isEmpty())
        return;
    RepatchBuffer repatchBuffer(this);
    while (m_incomingCalls.begin() != m_incomingCalls.end())
        m_incomingCalls.begin()->unlink(*m_vm, repatchBuffer);
#endif // ENABLE(JIT)
}

#if ENABLE(LLINT)
void CodeBlock::linkIncomingCall(ExecState* callerFrame, LLIntCallLinkInfo* incoming)
{
    noticeIncomingCall(callerFrame);
    m_incomingLLIntCalls.push(incoming);
}
#endif // ENABLE(LLINT)

void CodeBlock::clearEvalCache()
{
    if (!!m_alternative)
        m_alternative->clearEvalCache();
    if (CodeBlock* otherBlock = specialOSREntryBlockOrNull())
        otherBlock->clearEvalCache();
    if (!m_rareData)
        return;
    m_rareData->m_evalCodeCache.clear();
}

void CodeBlock::install()
{
    ownerExecutable()->installCode(this);
}

PassRefPtr<CodeBlock> CodeBlock::newReplacement()
{
    return ownerExecutable()->newReplacementCodeBlockFor(specializationKind());
}

const SlowArgument* CodeBlock::machineSlowArguments()
{
    if (!JITCode::isOptimizingJIT(jitType()))
        return symbolTable()->slowArguments();
    
#if ENABLE(DFG_JIT)
    return jitCode()->dfgCommon()->slowArguments.get();
#else // ENABLE(DFG_JIT)
    return 0;
#endif // ENABLE(DFG_JIT)
}

#if ENABLE(JIT)
CodeBlock* ProgramCodeBlock::replacement()
{
    return jsCast<ProgramExecutable*>(ownerExecutable())->codeBlock();
}

CodeBlock* EvalCodeBlock::replacement()
{
    return jsCast<EvalExecutable*>(ownerExecutable())->codeBlock();
}

CodeBlock* FunctionCodeBlock::replacement()
{
    return jsCast<FunctionExecutable*>(ownerExecutable())->codeBlockFor(m_isConstructor ? CodeForConstruct : CodeForCall);
}

DFG::CapabilityLevel ProgramCodeBlock::capabilityLevelInternal()
{
    return DFG::programCapabilityLevel(this);
}

DFG::CapabilityLevel EvalCodeBlock::capabilityLevelInternal()
{
    return DFG::evalCapabilityLevel(this);
}

DFG::CapabilityLevel FunctionCodeBlock::capabilityLevelInternal()
{
    if (m_isConstructor)
        return DFG::functionForConstructCapabilityLevel(this);
    return DFG::functionForCallCapabilityLevel(this);
}
#endif

void CodeBlock::jettison(ReoptimizationMode mode)
{
#if ENABLE(DFG_JIT)
    if (DFG::shouldShowDisassembly()) {
        dataLog("Jettisoning ", *this);
        if (mode == CountReoptimization)
            dataLog(" and counting reoptimization");
        dataLog(".\n");
    }
    
    DeferGCForAWhile deferGC(*m_heap);
    RELEASE_ASSERT(JITCode::isOptimizingJIT(jitType()));
    
    // We want to accomplish two things here:
    // 1) Make sure that if this CodeBlock is on the stack right now, then if we return to it
    //    we should OSR exit at the top of the next bytecode instruction after the return.
    // 2) Make sure that if we call the owner executable, then we shouldn't call this CodeBlock.
    
    // This accomplishes the OSR-exit-on-return part, and does its own book-keeping about
    // whether the invalidation has already happened.
    if (!jitCode()->dfgCommon()->invalidate()) {
        // Nothing to do since we've already been invalidated. That means that we cannot be
        // the optimized replacement.
        RELEASE_ASSERT(this != replacement());
        return;
    }
    
    if (DFG::shouldShowDisassembly())
        dataLog("    Did invalidate ", *this, "\n");
    
    // Count the reoptimization if that's what the user wanted.
    if (mode == CountReoptimization) {
        // FIXME: Maybe this should call alternative().
        // https://bugs.webkit.org/show_bug.cgi?id=123677
        baselineAlternative()->countReoptimization();
        if (DFG::shouldShowDisassembly())
            dataLog("    Did count reoptimization for ", *this, "\n");
    }
    
    // Now take care of the entrypoint.
    if (this != replacement()) {
        // This means that we were never the entrypoint. This can happen for OSR entry code
        // blocks.
        return;
    }
    alternative()->optimizeAfterWarmUp();
    tallyFrequentExitSites();
    alternative()->install();
    if (DFG::shouldShowDisassembly())
        dataLog("    Did install baseline version of ", *this, "\n");
#else // ENABLE(DFG_JIT)
    UNUSED_PARAM(mode);
    UNREACHABLE_FOR_PLATFORM();
#endif // ENABLE(DFG_JIT)
}

JSGlobalObject* CodeBlock::globalObjectFor(CodeOrigin codeOrigin)
{
    if (!codeOrigin.inlineCallFrame)
        return globalObject();
    return jsCast<FunctionExecutable*>(codeOrigin.inlineCallFrame->executable.get())->eitherCodeBlock()->globalObject();
}

void CodeBlock::noticeIncomingCall(ExecState* callerFrame)
{
    CodeBlock* callerCodeBlock = callerFrame->codeBlock();
    
    if (Options::verboseCallLink())
        dataLog("Noticing call link from ", *callerCodeBlock, " to ", *this, "\n");
    
    if (!m_shouldAlwaysBeInlined)
        return;

#if ENABLE(DFG_JIT)
    if (!hasBaselineJITProfiling())
        return;

    if (!DFG::mightInlineFunction(this))
        return;

    if (!canInline(m_capabilityLevelState))
        return;

    if (callerCodeBlock->jitType() == JITCode::InterpreterThunk) {
        // If the caller is still in the interpreter, then we can't expect inlining to
        // happen anytime soon. Assume it's profitable to optimize it separately. This
        // ensures that a function is SABI only if it is called no more frequently than
        // any of its callers.
        m_shouldAlwaysBeInlined = false;
        if (Options::verboseCallLink())
            dataLog("    Marking SABI because caller is in LLInt.\n");
        return;
    }
    
    if (callerCodeBlock->codeType() != FunctionCode) {
        // If the caller is either eval or global code, assume that that won't be
        // optimized anytime soon. For eval code this is particularly true since we
        // delay eval optimization by a *lot*.
        m_shouldAlwaysBeInlined = false;
        if (Options::verboseCallLink())
            dataLog("    Marking SABI because caller is not a function.\n");
        return;
    }
    
    ExecState* frame = callerFrame;
    for (unsigned i = Options::maximumInliningDepth(); i--; frame = frame->callerFrame()) {
        if (frame->isVMEntrySentinel())
            break;
        if (frame->codeBlock() == this) {
            // Recursive calls won't be inlined.
            if (Options::verboseCallLink())
                dataLog("    Marking SABI because recursion was detected.\n");
            m_shouldAlwaysBeInlined = false;
            return;
        }
    }
    
    RELEASE_ASSERT(callerCodeBlock->m_capabilityLevelState != DFG::CapabilityLevelNotSet);
    
    if (canCompile(callerCodeBlock->m_capabilityLevelState))
        return;
    
    if (Options::verboseCallLink())
        dataLog("    Marking SABI because the caller is not a DFG candidate.\n");
    
    m_shouldAlwaysBeInlined = false;
#endif
}

#if ENABLE(JIT)
unsigned CodeBlock::reoptimizationRetryCounter() const
{
    ASSERT(m_reoptimizationRetryCounter <= Options::reoptimizationRetryCounterMax());
    return m_reoptimizationRetryCounter;
}

void CodeBlock::countReoptimization()
{
    m_reoptimizationRetryCounter++;
    if (m_reoptimizationRetryCounter > Options::reoptimizationRetryCounterMax())
        m_reoptimizationRetryCounter = Options::reoptimizationRetryCounterMax();
}

unsigned CodeBlock::numberOfDFGCompiles()
{
    ASSERT(JITCode::isBaselineCode(jitType()));
    if (Options::testTheFTL()) {
        if (m_didFailFTLCompilation)
            return 1000000;
        return (m_hasBeenCompiledWithFTL ? 1 : 0) + m_reoptimizationRetryCounter;
    }
    return (JITCode::isOptimizingJIT(replacement()->jitType()) ? 1 : 0) + m_reoptimizationRetryCounter;
}

int32_t CodeBlock::codeTypeThresholdMultiplier() const
{
    if (codeType() == EvalCode)
        return Options::evalThresholdMultiplier();
    
    return 1;
}

double CodeBlock::optimizationThresholdScalingFactor()
{
    // This expression arises from doing a least-squares fit of
    //
    // F[x_] =: a * Sqrt[x + b] + Abs[c * x] + d
    //
    // against the data points:
    //
    //    x       F[x_]
    //    10       0.9          (smallest reasonable code block)
    //   200       1.0          (typical small-ish code block)
    //   320       1.2          (something I saw in 3d-cube that I wanted to optimize)
    //  1268       5.0          (something I saw in 3d-cube that I didn't want to optimize)
    //  4000       5.5          (random large size, used to cause the function to converge to a shallow curve of some sort)
    // 10000       6.0          (similar to above)
    //
    // I achieve the minimization using the following Mathematica code:
    //
    // MyFunctionTemplate[x_, a_, b_, c_, d_] := a*Sqrt[x + b] + Abs[c*x] + d
    //
    // samples = {{10, 0.9}, {200, 1}, {320, 1.2}, {1268, 5}, {4000, 5.5}, {10000, 6}}
    //
    // solution = 
    //     Minimize[Plus @@ ((MyFunctionTemplate[#[[1]], a, b, c, d] - #[[2]])^2 & /@ samples),
    //         {a, b, c, d}][[2]]
    //
    // And the code below (to initialize a, b, c, d) is generated by:
    //
    // Print["const double " <> ToString[#[[1]]] <> " = " <>
    //     If[#[[2]] < 0.00001, "0.0", ToString[#[[2]]]] <> ";"] & /@ solution
    //
    // We've long known the following to be true:
    // - Small code blocks are cheap to optimize and so we should do it sooner rather
    //   than later.
    // - Large code blocks are expensive to optimize and so we should postpone doing so,
    //   and sometimes have a large enough threshold that we never optimize them.
    // - The difference in cost is not totally linear because (a) just invoking the
    //   DFG incurs some base cost and (b) for large code blocks there is enough slop
    //   in the correlation between instruction count and the actual compilation cost
    //   that for those large blocks, the instruction count should not have a strong
    //   influence on our threshold.
    //
    // I knew the goals but I didn't know how to achieve them; so I picked an interesting
    // example where the heuristics were right (code block in 3d-cube with instruction
    // count 320, which got compiled early as it should have been) and one where they were
    // totally wrong (code block in 3d-cube with instruction count 1268, which was expensive
    // to compile and didn't run often enough to warrant compilation in my opinion), and
    // then threw in additional data points that represented my own guess of what our
    // heuristics should do for some round-numbered examples.
    //
    // The expression to which I decided to fit the data arose because I started with an
    // affine function, and then did two things: put the linear part in an Abs to ensure
    // that the fit didn't end up choosing a negative value of c (which would result in
    // the function turning over and going negative for large x) and I threw in a Sqrt
    // term because Sqrt represents my intution that the function should be more sensitive
    // to small changes in small values of x, but less sensitive when x gets large.
    
    // Note that the current fit essentially eliminates the linear portion of the
    // expression (c == 0.0).
    const double a = 0.061504;
    const double b = 1.02406;
    const double c = 0.0;
    const double d = 0.825914;
    
    double instructionCount = this->instructionCount();
    
    ASSERT(instructionCount); // Make sure this is called only after we have an instruction stream; otherwise it'll just return the value of d, which makes no sense.
    
    double result = d + a * sqrt(instructionCount + b) + c * instructionCount;
    if (Options::verboseOSR()) {
        dataLog(
            *this, ": instruction count is ", instructionCount,
            ", scaling execution counter by ", result, " * ", codeTypeThresholdMultiplier(),
            "\n");
    }
    return result * codeTypeThresholdMultiplier();
}

static int32_t clipThreshold(double threshold)
{
    if (threshold < 1.0)
        return 1;
    
    if (threshold > static_cast<double>(std::numeric_limits<int32_t>::max()))
        return std::numeric_limits<int32_t>::max();
    
    return static_cast<int32_t>(threshold);
}

int32_t CodeBlock::adjustedCounterValue(int32_t desiredThreshold)
{
    return clipThreshold(
        static_cast<double>(desiredThreshold) *
        optimizationThresholdScalingFactor() *
        (1 << reoptimizationRetryCounter()));
}

bool CodeBlock::checkIfOptimizationThresholdReached()
{
#if ENABLE(DFG_JIT)
    if (DFG::Worklist* worklist = DFG::existingGlobalDFGWorklistOrNull()) {
        if (worklist->compilationState(DFG::CompilationKey(this, DFG::DFGMode))
            == DFG::Worklist::Compiled) {
            optimizeNextInvocation();
            return true;
        }
    }
#endif
    
    return m_jitExecuteCounter.checkIfThresholdCrossedAndSet(this);
}

void CodeBlock::optimizeNextInvocation()
{
    if (Options::verboseOSR())
        dataLog(*this, ": Optimizing next invocation.\n");
    m_jitExecuteCounter.setNewThreshold(0, this);
}

void CodeBlock::dontOptimizeAnytimeSoon()
{
    if (Options::verboseOSR())
        dataLog(*this, ": Not optimizing anytime soon.\n");
    m_jitExecuteCounter.deferIndefinitely();
}

void CodeBlock::optimizeAfterWarmUp()
{
    if (Options::verboseOSR())
        dataLog(*this, ": Optimizing after warm-up.\n");
#if ENABLE(DFG_JIT)
    m_jitExecuteCounter.setNewThreshold(
        adjustedCounterValue(Options::thresholdForOptimizeAfterWarmUp()), this);
#endif
}

void CodeBlock::optimizeAfterLongWarmUp()
{
    if (Options::verboseOSR())
        dataLog(*this, ": Optimizing after long warm-up.\n");
#if ENABLE(DFG_JIT)
    m_jitExecuteCounter.setNewThreshold(
        adjustedCounterValue(Options::thresholdForOptimizeAfterLongWarmUp()), this);
#endif
}

void CodeBlock::optimizeSoon()
{
    if (Options::verboseOSR())
        dataLog(*this, ": Optimizing soon.\n");
#if ENABLE(DFG_JIT)
    m_jitExecuteCounter.setNewThreshold(
        adjustedCounterValue(Options::thresholdForOptimizeSoon()), this);
#endif
}

void CodeBlock::forceOptimizationSlowPathConcurrently()
{
    if (Options::verboseOSR())
        dataLog(*this, ": Forcing slow path concurrently.\n");
    m_jitExecuteCounter.forceSlowPathConcurrently();
}

#if ENABLE(DFG_JIT)
void CodeBlock::setOptimizationThresholdBasedOnCompilationResult(CompilationResult result)
{
    RELEASE_ASSERT(jitType() == JITCode::BaselineJIT);
    RELEASE_ASSERT((result == CompilationSuccessful) == (replacement() != this));
    switch (result) {
    case CompilationSuccessful:
        RELEASE_ASSERT(JITCode::isOptimizingJIT(replacement()->jitType()));
        optimizeNextInvocation();
        return;
    case CompilationFailed:
        dontOptimizeAnytimeSoon();
        return;
    case CompilationDeferred:
        // We'd like to do dontOptimizeAnytimeSoon() but we cannot because
        // forceOptimizationSlowPathConcurrently() is inherently racy. It won't
        // necessarily guarantee anything. So, we make sure that even if that
        // function ends up being a no-op, we still eventually retry and realize
        // that we have optimized code ready.
        optimizeAfterWarmUp();
        return;
    case CompilationInvalidated:
        // Retry with exponential backoff.
        countReoptimization();
        optimizeAfterWarmUp();
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

#endif
    
uint32_t CodeBlock::adjustedExitCountThreshold(uint32_t desiredThreshold)
{
    ASSERT(JITCode::isOptimizingJIT(jitType()));
    // Compute this the lame way so we don't saturate. This is called infrequently
    // enough that this loop won't hurt us.
    unsigned result = desiredThreshold;
    for (unsigned n = baselineVersion()->reoptimizationRetryCounter(); n--;) {
        unsigned newResult = result << 1;
        if (newResult < result)
            return std::numeric_limits<uint32_t>::max();
        result = newResult;
    }
    return result;
}

uint32_t CodeBlock::exitCountThresholdForReoptimization()
{
    return adjustedExitCountThreshold(Options::osrExitCountForReoptimization() * codeTypeThresholdMultiplier());
}

uint32_t CodeBlock::exitCountThresholdForReoptimizationFromLoop()
{
    return adjustedExitCountThreshold(Options::osrExitCountForReoptimizationFromLoop() * codeTypeThresholdMultiplier());
}

bool CodeBlock::shouldReoptimizeNow()
{
    return osrExitCounter() >= exitCountThresholdForReoptimization();
}

bool CodeBlock::shouldReoptimizeFromLoopNow()
{
    return osrExitCounter() >= exitCountThresholdForReoptimizationFromLoop();
}
#endif

ArrayProfile* CodeBlock::getArrayProfile(unsigned bytecodeOffset)
{
    for (unsigned i = 0; i < m_arrayProfiles.size(); ++i) {
        if (m_arrayProfiles[i].bytecodeOffset() == bytecodeOffset)
            return &m_arrayProfiles[i];
    }
    return 0;
}

ArrayProfile* CodeBlock::getOrAddArrayProfile(unsigned bytecodeOffset)
{
    ArrayProfile* result = getArrayProfile(bytecodeOffset);
    if (result)
        return result;
    return addArrayProfile(bytecodeOffset);
}

void CodeBlock::updateAllPredictionsAndCountLiveness(unsigned& numberOfLiveNonArgumentValueProfiles, unsigned& numberOfSamplesInProfiles)
{
    ConcurrentJITLocker locker(m_lock);
    
    numberOfLiveNonArgumentValueProfiles = 0;
    numberOfSamplesInProfiles = 0; // If this divided by ValueProfile::numberOfBuckets equals numberOfValueProfiles() then value profiles are full.
    for (unsigned i = 0; i < totalNumberOfValueProfiles(); ++i) {
        ValueProfile* profile = getFromAllValueProfiles(i);
        unsigned numSamples = profile->totalNumberOfSamples();
        if (numSamples > ValueProfile::numberOfBuckets)
            numSamples = ValueProfile::numberOfBuckets; // We don't want profiles that are extremely hot to be given more weight.
        numberOfSamplesInProfiles += numSamples;
        if (profile->m_bytecodeOffset < 0) {
            profile->computeUpdatedPrediction(locker);
            continue;
        }
        if (profile->numberOfSamples() || profile->m_prediction != SpecNone)
            numberOfLiveNonArgumentValueProfiles++;
        profile->computeUpdatedPrediction(locker);
    }
    
#if ENABLE(DFG_JIT)
    m_lazyOperandValueProfiles.computeUpdatedPredictions(locker);
#endif
}

void CodeBlock::updateAllValueProfilePredictions()
{
    unsigned ignoredValue1, ignoredValue2;
    updateAllPredictionsAndCountLiveness(ignoredValue1, ignoredValue2);
}

void CodeBlock::updateAllArrayPredictions()
{
    ConcurrentJITLocker locker(m_lock);
    
    for (unsigned i = m_arrayProfiles.size(); i--;)
        m_arrayProfiles[i].computeUpdatedPrediction(locker, this);
    
    // Don't count these either, for similar reasons.
    for (unsigned i = m_arrayAllocationProfiles.size(); i--;)
        m_arrayAllocationProfiles[i].updateIndexingType();
}

void CodeBlock::updateAllPredictions()
{
    updateAllValueProfilePredictions();
    updateAllArrayPredictions();
}

bool CodeBlock::shouldOptimizeNow()
{
    if (Options::verboseOSR())
        dataLog("Considering optimizing ", *this, "...\n");

    if (m_optimizationDelayCounter >= Options::maximumOptimizationDelay())
        return true;
    
    updateAllArrayPredictions();
    
    unsigned numberOfLiveNonArgumentValueProfiles;
    unsigned numberOfSamplesInProfiles;
    updateAllPredictionsAndCountLiveness(numberOfLiveNonArgumentValueProfiles, numberOfSamplesInProfiles);

    if (Options::verboseOSR()) {
        dataLogF(
            "Profile hotness: %lf (%u / %u), %lf (%u / %u)\n",
            (double)numberOfLiveNonArgumentValueProfiles / numberOfValueProfiles(),
            numberOfLiveNonArgumentValueProfiles, numberOfValueProfiles(),
            (double)numberOfSamplesInProfiles / ValueProfile::numberOfBuckets / numberOfValueProfiles(),
            numberOfSamplesInProfiles, ValueProfile::numberOfBuckets * numberOfValueProfiles());
    }

    if ((!numberOfValueProfiles() || (double)numberOfLiveNonArgumentValueProfiles / numberOfValueProfiles() >= Options::desiredProfileLivenessRate())
        && (!totalNumberOfValueProfiles() || (double)numberOfSamplesInProfiles / ValueProfile::numberOfBuckets / totalNumberOfValueProfiles() >= Options::desiredProfileFullnessRate())
        && static_cast<unsigned>(m_optimizationDelayCounter) + 1 >= Options::minimumOptimizationDelay())
        return true;
    
    ASSERT(m_optimizationDelayCounter < std::numeric_limits<uint8_t>::max());
    m_optimizationDelayCounter++;
    optimizeAfterWarmUp();
    return false;
}

#if ENABLE(DFG_JIT)
void CodeBlock::tallyFrequentExitSites()
{
    ASSERT(JITCode::isOptimizingJIT(jitType()));
    ASSERT(alternative()->jitType() == JITCode::BaselineJIT);
    
    CodeBlock* profiledBlock = alternative();
    
    switch (jitType()) {
    case JITCode::DFGJIT: {
        DFG::JITCode* jitCode = m_jitCode->dfg();
        for (unsigned i = 0; i < jitCode->osrExit.size(); ++i) {
            DFG::OSRExit& exit = jitCode->osrExit[i];
            
            if (!exit.considerAddingAsFrequentExitSite(profiledBlock))
                continue;
        }
        break;
    }

#if ENABLE(FTL_JIT)
    case JITCode::FTLJIT: {
        // There is no easy way to avoid duplicating this code since the FTL::JITCode::osrExit
        // vector contains a totally different type, that just so happens to behave like
        // DFG::JITCode::osrExit.
        FTL::JITCode* jitCode = m_jitCode->ftl();
        for (unsigned i = 0; i < jitCode->osrExit.size(); ++i) {
            FTL::OSRExit& exit = jitCode->osrExit[i];
            
            if (!exit.considerAddingAsFrequentExitSite(profiledBlock))
                continue;
        }
        break;
    }
#endif
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}
#endif // ENABLE(DFG_JIT)

#if ENABLE(VERBOSE_VALUE_PROFILE)
void CodeBlock::dumpValueProfiles()
{
    dataLog("ValueProfile for ", *this, ":\n");
    for (unsigned i = 0; i < totalNumberOfValueProfiles(); ++i) {
        ValueProfile* profile = getFromAllValueProfiles(i);
        if (profile->m_bytecodeOffset < 0) {
            ASSERT(profile->m_bytecodeOffset == -1);
            dataLogF("   arg = %u: ", i);
        } else
            dataLogF("   bc = %d: ", profile->m_bytecodeOffset);
        if (!profile->numberOfSamples() && profile->m_prediction == SpecNone) {
            dataLogF("<empty>\n");
            continue;
        }
        profile->dump(WTF::dataFile());
        dataLogF("\n");
    }
    dataLog("RareCaseProfile for ", *this, ":\n");
    for (unsigned i = 0; i < numberOfRareCaseProfiles(); ++i) {
        RareCaseProfile* profile = rareCaseProfile(i);
        dataLogF("   bc = %d: %u\n", profile->m_bytecodeOffset, profile->m_counter);
    }
    dataLog("SpecialFastCaseProfile for ", *this, ":\n");
    for (unsigned i = 0; i < numberOfSpecialFastCaseProfiles(); ++i) {
        RareCaseProfile* profile = specialFastCaseProfile(i);
        dataLogF("   bc = %d: %u\n", profile->m_bytecodeOffset, profile->m_counter);
    }
}
#endif // ENABLE(VERBOSE_VALUE_PROFILE)

unsigned CodeBlock::frameRegisterCount()
{
    switch (jitType()) {
#if ENABLE(LLINT)
    case JITCode::InterpreterThunk:
        return LLInt::frameRegisterCountFor(this);
#endif // ENABLE(LLINT)

#if ENABLE(JIT)
    case JITCode::BaselineJIT:
        return JIT::frameRegisterCountFor(this);
#endif // ENABLE(JIT)

#if ENABLE(DFG_JIT)
    case JITCode::DFGJIT:
    case JITCode::FTLJIT:
        return jitCode()->dfgCommon()->frameRegisterCount;
#endif // ENABLE(DFG_JIT)
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }
}

int CodeBlock::stackPointerOffset()
{
    return virtualRegisterForLocal(frameRegisterCount() - 1).offset();
}

size_t CodeBlock::predictedMachineCodeSize()
{
    // This will be called from CodeBlock::CodeBlock before either m_vm or the
    // instructions have been initialized. It's OK to return 0 because what will really
    // matter is the recomputation of this value when the slow path is triggered.
    if (!m_vm)
        return 0;
    
    if (!m_vm->machineCodeBytesPerBytecodeWordForBaselineJIT)
        return 0; // It's as good of a prediction as we'll get.
    
    // Be conservative: return a size that will be an overestimation 84% of the time.
    double multiplier = m_vm->machineCodeBytesPerBytecodeWordForBaselineJIT.mean() +
        m_vm->machineCodeBytesPerBytecodeWordForBaselineJIT.standardDeviation();
    
    // Be paranoid: silently reject bogus multipiers. Silently doing the "wrong" thing
    // here is OK, since this whole method is just a heuristic.
    if (multiplier < 0 || multiplier > 1000)
        return 0;
    
    double doubleResult = multiplier * m_instructions.size();
    
    // Be even more paranoid: silently reject values that won't fit into a size_t. If
    // the function is so huge that we can't even fit it into virtual memory then we
    // should probably have some other guards in place to prevent us from even getting
    // to this point.
    if (doubleResult > std::numeric_limits<size_t>::max())
        return 0;
    
    return static_cast<size_t>(doubleResult);
}

bool CodeBlock::usesOpcode(OpcodeID opcodeID)
{
    Interpreter* interpreter = vm()->interpreter;
    Instruction* instructionsBegin = instructions().begin();
    unsigned instructionCount = instructions().size();
    
    for (unsigned bytecodeOffset = 0; bytecodeOffset < instructionCount; ) {
        switch (interpreter->getOpcodeID(instructionsBegin[bytecodeOffset].u.opcode)) {
#define DEFINE_OP(curOpcode, length)        \
        case curOpcode:                     \
            if (curOpcode == opcodeID)      \
                return true;                \
            bytecodeOffset += length;       \
            break;
            FOR_EACH_OPCODE_ID(DEFINE_OP)
#undef DEFINE_OP
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    return false;
}

String CodeBlock::nameForRegister(VirtualRegister virtualRegister)
{
    ConcurrentJITLocker locker(symbolTable()->m_lock);
    SymbolTable::Map::iterator end = symbolTable()->end(locker);
    for (SymbolTable::Map::iterator ptr = symbolTable()->begin(locker); ptr != end; ++ptr) {
        if (ptr->value.getIndex() == virtualRegister.offset()) {
            // FIXME: This won't work from the compilation thread.
            // https://bugs.webkit.org/show_bug.cgi?id=115300
            return String(ptr->key);
        }
    }
    if (needsActivation() && virtualRegister == activationRegister())
        return ASCIILiteral("activation");
    if (virtualRegister == thisRegister())
        return ASCIILiteral("this");
    if (usesArguments()) {
        if (virtualRegister == argumentsRegister())
            return ASCIILiteral("arguments");
        if (unmodifiedArgumentsRegister(argumentsRegister()) == virtualRegister)
            return ASCIILiteral("real arguments");
    }
    if (virtualRegister.isArgument())
        return String::format("arguments[%3d]", virtualRegister.toArgument()).impl();

    return "";
}

namespace {

struct VerifyCapturedDef {
    void operator()(CodeBlock* codeBlock, Instruction* instruction, OpcodeID opcodeID, int operand)
    {
        unsigned bytecodeOffset = instruction - codeBlock->instructions().begin();
        
        if (codeBlock->isConstantRegisterIndex(operand)) {
            codeBlock->beginValidationDidFail();
            dataLog("    At bc#", bytecodeOffset, " encountered a definition of a constant.\n");
            codeBlock->endValidationDidFail();
            return;
        }

        switch (opcodeID) {
        case op_enter:
        case op_captured_mov:
        case op_init_lazy_reg:
        case op_create_arguments:
        case op_new_captured_func:
            return;
        default:
            break;
        }
        
        VirtualRegister virtualReg(operand);
        if (!virtualReg.isLocal())
            return;
        
        if (codeBlock->captureCount() && codeBlock->symbolTable()->isCaptured(operand)) {
            codeBlock->beginValidationDidFail();
            dataLog("    At bc#", bytecodeOffset, " encountered invalid assignment to captured variable loc", virtualReg.toLocal(), ".\n");
            codeBlock->endValidationDidFail();
            return;
        }
        
        return;
    }
};

} // anonymous namespace

void CodeBlock::validate()
{
    BytecodeLivenessAnalysis liveness(this); // Compute directly from scratch so it doesn't effect CodeBlock footprint.
    
    FastBitVector liveAtHead = liveness.getLivenessInfoAtBytecodeOffset(0);
    
    if (liveAtHead.numBits() != static_cast<size_t>(m_numCalleeRegisters)) {
        beginValidationDidFail();
        dataLog("    Wrong number of bits in result!\n");
        dataLog("    Result: ", liveAtHead, "\n");
        dataLog("    Bit count: ", liveAtHead.numBits(), "\n");
        endValidationDidFail();
    }
    
    for (unsigned i = m_numCalleeRegisters; i--;) {
        bool isCaptured = false;
        VirtualRegister reg = virtualRegisterForLocal(i);
        
        if (captureCount())
            isCaptured = reg.offset() <= captureStart() && reg.offset() > captureEnd();
        
        if (isCaptured) {
            if (!liveAtHead.get(i)) {
                beginValidationDidFail();
                dataLog("    Variable loc", i, " is expected to be live because it is captured, but it isn't live.\n");
                dataLog("    Result: ", liveAtHead, "\n");
                endValidationDidFail();
            }
        } else {
            if (liveAtHead.get(i)) {
                beginValidationDidFail();
                dataLog("    Variable loc", i, " is expected to be dead.\n");
                dataLog("    Result: ", liveAtHead, "\n");
                endValidationDidFail();
            }
        }
    }
    
    for (unsigned bytecodeOffset = 0; bytecodeOffset < instructions().size();) {
        Instruction* currentInstruction = instructions().begin() + bytecodeOffset;
        OpcodeID opcodeID = m_vm->interpreter->getOpcodeID(currentInstruction->u.opcode);
        
        VerifyCapturedDef verifyCapturedDef;
        computeDefsForBytecodeOffset(this, bytecodeOffset, verifyCapturedDef);
        
        bytecodeOffset += opcodeLength(opcodeID);
    }
}

void CodeBlock::beginValidationDidFail()
{
    dataLog("Validation failure in ", *this, ":\n");
    dataLog("\n");
}

void CodeBlock::endValidationDidFail()
{
    dataLog("\n");
    dumpBytecode();
    dataLog("\n");
    dataLog("Validation failure.\n");
    RELEASE_ASSERT_NOT_REACHED();
}

void CodeBlock::addBreakpoint(unsigned numBreakpoints)
{
    m_numBreakpoints += numBreakpoints;
    ASSERT(m_numBreakpoints);
    if (JITCode::isOptimizingJIT(jitType()))
        jettison();
}

void CodeBlock::setSteppingMode(CodeBlock::SteppingMode mode)
{
    m_steppingMode = mode;
    if (mode == SteppingModeEnabled && JITCode::isOptimizingJIT(jitType()))
        jettison();
}

} // namespace JSC
