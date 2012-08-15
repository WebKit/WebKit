/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "DFGCapabilities.h"
#include "DFGNode.h"
#include "DFGRepatch.h"
#include "Debugger.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JITStubs.h"
#include "JSActivation.h"
#include "JSFunction.h"
#include "JSStaticScopeObject.h"
#include "JSValue.h"
#include "LowLevelInterpreter.h"
#include "MethodCallLinkStatus.h"
#include "RepatchBuffer.h"
#include "UStringConcatenate.h"
#include <stdio.h>
#include <wtf/StringExtras.h>
#include <wtf/UnusedParam.h>

#if ENABLE(DFG_JIT)
#include "DFGOperations.h"
#endif

#define DUMP_CODE_BLOCK_STATISTICS 0

namespace JSC {

#if ENABLE(DFG_JIT)
using namespace DFG;
#endif

static UString escapeQuotes(const UString& str)
{
    UString result = str;
    size_t pos = 0;
    while ((pos = result.find('\"', pos)) != notFound) {
        result = makeUString(result.substringSharingImpl(0, pos), "\"\\\"\"", result.substringSharingImpl(pos + 1));
        pos += 4;
    }
    return result;
}

static UString valueToSourceString(ExecState* exec, JSValue val)
{
    if (!val)
        return "0";

    if (val.isString())
        return makeUString("\"", escapeQuotes(val.toString(exec)->value(exec)), "\"");

    return val.description();
}

static CString constantName(ExecState* exec, int k, JSValue value)
{
    return makeUString(valueToSourceString(exec, value), "(@k", UString::number(k - FirstConstantRegisterIndex), ")").utf8();
}

static CString idName(int id0, const Identifier& ident)
{
    return makeUString(ident.ustring(), "(@id", UString::number(id0), ")").utf8();
}

void CodeBlock::dumpBytecodeCommentAndNewLine(int location)
{
#if ENABLE(BYTECODE_COMMENTS)
    const char* comment = commentForBytecodeOffset(location);
    if (comment)
        dataLog("\t\t ; %s", comment);
#else
    UNUSED_PARAM(location);
#endif
    dataLog("\n");
}

CString CodeBlock::registerName(ExecState* exec, int r) const
{
    if (r == missingThisObjectMarker())
        return "<null>";

    if (isConstantRegisterIndex(r))
        return constantName(exec, r, getConstant(r));

    return makeUString("r", UString::number(r)).utf8();
}

static UString regexpToSourceString(RegExp* regExp)
{
    char postfix[5] = { '/', 0, 0, 0, 0 };
    int index = 1;
    if (regExp->global())
        postfix[index++] = 'g';
    if (regExp->ignoreCase())
        postfix[index++] = 'i';
    if (regExp->multiline())
        postfix[index] = 'm';

    return makeUString("/", regExp->pattern(), postfix);
}

static CString regexpName(int re, RegExp* regexp)
{
    return makeUString(regexpToSourceString(regexp), "(@re", UString::number(re), ")").utf8();
}

static UString pointerToSourceString(void* p)
{
    char buffer[2 + 2 * sizeof(void*) + 1]; // 0x [two characters per byte] \0
    snprintf(buffer, sizeof(buffer), "%p", p);
    return buffer;
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

    ASSERT_NOT_REACHED();
    return "";
}

void CodeBlock::printUnaryOp(ExecState* exec, int location, Vector<Instruction>::const_iterator& it, const char* op)
{
    int r0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;

    dataLog("[%4d] %s\t\t %s, %s", location, op, registerName(exec, r0).data(), registerName(exec, r1).data());
    dumpBytecodeCommentAndNewLine(location);
}

void CodeBlock::printBinaryOp(ExecState* exec, int location, Vector<Instruction>::const_iterator& it, const char* op)
{
    int r0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;
    int r2 = (++it)->u.operand;
    dataLog("[%4d] %s\t\t %s, %s, %s", location, op, registerName(exec, r0).data(), registerName(exec, r1).data(), registerName(exec, r2).data());
    dumpBytecodeCommentAndNewLine(location);
}

void CodeBlock::printConditionalJump(ExecState* exec, const Vector<Instruction>::const_iterator&, Vector<Instruction>::const_iterator& it, int location, const char* op)
{
    int r0 = (++it)->u.operand;
    int offset = (++it)->u.operand;
    dataLog("[%4d] %s\t\t %s, %d(->%d)", location, op, registerName(exec, r0).data(), offset, location + offset);
    dumpBytecodeCommentAndNewLine(location);
}

void CodeBlock::printGetByIdOp(ExecState* exec, int location, Vector<Instruction>::const_iterator& it)
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
        ASSERT_NOT_REACHED();
        op = 0;
    }
    int r0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;
    int id0 = (++it)->u.operand;
    dataLog("[%4d] %s\t %s, %s, %s", location, op, registerName(exec, r0).data(), registerName(exec, r1).data(), idName(id0, m_identifiers[id0]).data());
    it += 5;
}

#if ENABLE(JIT) || ENABLE(LLINT) // unused in some configurations
static void dumpStructure(const char* name, ExecState* exec, Structure* structure, Identifier& ident)
{
    if (!structure)
        return;
    
    dataLog("%s = %p", name, structure);
    
    PropertyOffset offset = structure->get(exec->globalData(), ident);
    if (offset != invalidOffset)
        dataLog(" (offset = %d)", offset);
}
#endif

#if ENABLE(JIT) // unused when not ENABLE(JIT), leading to silly warnings
static void dumpChain(ExecState* exec, StructureChain* chain, Identifier& ident)
{
    dataLog("chain = %p: [", chain);
    bool first = true;
    for (WriteBarrier<Structure>* currentStructure = chain->head();
         *currentStructure;
         ++currentStructure) {
        if (first)
            first = false;
        else
            dataLog(", ");
        dumpStructure("struct", exec, currentStructure->get(), ident);
    }
    dataLog("]");
}
#endif

void CodeBlock::printGetByIdCacheStatus(ExecState* exec, int location)
{
    Instruction* instruction = instructions().begin() + location;

    if (exec->interpreter()->getOpcodeID(instruction[0].u.opcode) == op_method_check)
        instruction++;
    
    Identifier& ident = identifier(instruction[3].u.operand);
    
    UNUSED_PARAM(ident); // tell the compiler to shut up in certain platform configurations.
    
#if ENABLE(LLINT)
    Structure* structure = instruction[4].u.structure.get();
    dataLog(" llint(");
    dumpStructure("struct", exec, structure, ident);
    dataLog(")");
#endif

#if ENABLE(JIT)
    if (numberOfStructureStubInfos()) {
        dataLog(" jit(");
        StructureStubInfo& stubInfo = getStubInfo(location);
        if (!stubInfo.seen)
            dataLog("not seen");
        else {
            Structure* baseStructure = 0;
            Structure* prototypeStructure = 0;
            StructureChain* chain = 0;
            PolymorphicAccessStructureList* structureList = 0;
            int listSize = 0;
            
            switch (stubInfo.accessType) {
            case access_get_by_id_self:
                dataLog("self");
                baseStructure = stubInfo.u.getByIdSelf.baseObjectStructure.get();
                break;
            case access_get_by_id_proto:
                dataLog("proto");
                baseStructure = stubInfo.u.getByIdProto.baseObjectStructure.get();
                prototypeStructure = stubInfo.u.getByIdProto.prototypeStructure.get();
                break;
            case access_get_by_id_chain:
                dataLog("chain");
                baseStructure = stubInfo.u.getByIdChain.baseObjectStructure.get();
                chain = stubInfo.u.getByIdChain.chain.get();
                break;
            case access_get_by_id_self_list:
                dataLog("self_list");
                structureList = stubInfo.u.getByIdSelfList.structureList;
                listSize = stubInfo.u.getByIdSelfList.listSize;
                break;
            case access_get_by_id_proto_list:
                dataLog("proto_list");
                structureList = stubInfo.u.getByIdProtoList.structureList;
                listSize = stubInfo.u.getByIdProtoList.listSize;
                break;
            case access_unset:
                dataLog("unset");
                break;
            case access_get_by_id_generic:
                dataLog("generic");
                break;
            case access_get_array_length:
                dataLog("array_length");
                break;
            case access_get_string_length:
                dataLog("string_length");
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
            
            if (baseStructure) {
                dataLog(", ");
                dumpStructure("struct", exec, baseStructure, ident);
            }
            
            if (prototypeStructure) {
                dataLog(", ");
                dumpStructure("prototypeStruct", exec, baseStructure, ident);
            }
            
            if (chain) {
                dataLog(", ");
                dumpChain(exec, chain, ident);
            }
            
            if (structureList) {
                dataLog(", list = %p: [", structureList);
                for (int i = 0; i < listSize; ++i) {
                    if (i)
                        dataLog(", ");
                    dataLog("(");
                    dumpStructure("base", exec, structureList->list[i].base.get(), ident);
                    if (structureList->list[i].isChain) {
                        if (structureList->list[i].u.chain.get()) {
                            dataLog(", ");
                            dumpChain(exec, structureList->list[i].u.chain.get(), ident);
                        }
                    } else {
                        if (structureList->list[i].u.proto.get()) {
                            dataLog(", ");
                            dumpStructure("proto", exec, structureList->list[i].u.proto.get(), ident);
                        }
                    }
                    dataLog(")");
                }
                dataLog("]");
            }
        }
        dataLog(")");
    }
#endif
}

void CodeBlock::printCallOp(ExecState* exec, int location, Vector<Instruction>::const_iterator& it, const char* op, CacheDumpMode cacheDumpMode)
{
    int func = (++it)->u.operand;
    int argCount = (++it)->u.operand;
    int registerOffset = (++it)->u.operand;
    dataLog("[%4d] %s\t %s, %d, %d", location, op, registerName(exec, func).data(), argCount, registerOffset);
    if (cacheDumpMode == DumpCaches) {
#if ENABLE(LLINT)
        LLIntCallLinkInfo* callLinkInfo = it[1].u.callLinkInfo;
        if (callLinkInfo->lastSeenCallee) {
            dataLog(" llint(%p, exec %p)",
                    callLinkInfo->lastSeenCallee.get(),
                    callLinkInfo->lastSeenCallee->executable());
        } else
            dataLog(" llint(not set)");
#endif
#if ENABLE(JIT)
        if (numberOfCallLinkInfos()) {
            JSFunction* target = getCallLinkInfo(location).lastSeenCallee.get();
            if (target)
                dataLog(" jit(%p, exec %p)", target, target->executable());
            else
                dataLog(" jit(not set)");
        }
#endif
    }
    dumpBytecodeCommentAndNewLine(location);
    it += 2;
}

void CodeBlock::printPutByIdOp(ExecState* exec, int location, Vector<Instruction>::const_iterator& it, const char* op)
{
    int r0 = (++it)->u.operand;
    int id0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;
    dataLog("[%4d] %s\t %s, %s, %s", location, op, registerName(exec, r0).data(), idName(id0, m_identifiers[id0]).data(), registerName(exec, r1).data());
    dumpBytecodeCommentAndNewLine(location);
    it += 5;
}

#if ENABLE(JIT)
static bool isGlobalResolve(OpcodeID opcodeID)
{
    return opcodeID == op_resolve_global || opcodeID == op_resolve_global_dynamic;
}

static unsigned instructionOffsetForNth(ExecState* exec, const RefCountedArray<Instruction>& instructions, int nth, bool (*predicate)(OpcodeID))
{
    size_t i = 0;
    while (i < instructions.size()) {
        OpcodeID currentOpcode = exec->interpreter()->getOpcodeID(instructions[i].u.opcode);
        if (predicate(currentOpcode)) {
            if (!--nth)
                return i;
        }
        i += opcodeLengths[currentOpcode];
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static void printGlobalResolveInfo(const GlobalResolveInfo& resolveInfo, unsigned instructionOffset)
{
    dataLog("  [%4d] %s: %s\n", instructionOffset, "resolve_global", pointerToSourceString(resolveInfo.structure).utf8().data());
}
#endif

void CodeBlock::printStructure(const char* name, const Instruction* vPC, int operand)
{
    unsigned instructionOffset = vPC - instructions().begin();
    dataLog("  [%4d] %s: %s\n", instructionOffset, name, pointerToSourceString(vPC[operand].u.structure).utf8().data());
}

void CodeBlock::printStructures(const Instruction* vPC)
{
    Interpreter* interpreter = m_globalData->interpreter;
    unsigned instructionOffset = vPC - instructions().begin();

    if (vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id)) {
        printStructure("get_by_id", vPC, 4);
        return;
    }
    if (vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id_self)) {
        printStructure("get_by_id_self", vPC, 4);
        return;
    }
    if (vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id_proto)) {
        dataLog("  [%4d] %s: %s, %s\n", instructionOffset, "get_by_id_proto", pointerToSourceString(vPC[4].u.structure).utf8().data(), pointerToSourceString(vPC[5].u.structure).utf8().data());
        return;
    }
    if (vPC[0].u.opcode == interpreter->getOpcode(op_put_by_id_transition)) {
        dataLog("  [%4d] %s: %s, %s, %s\n", instructionOffset, "put_by_id_transition", pointerToSourceString(vPC[4].u.structure).utf8().data(), pointerToSourceString(vPC[5].u.structure).utf8().data(), pointerToSourceString(vPC[6].u.structureChain).utf8().data());
        return;
    }
    if (vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id_chain)) {
        dataLog("  [%4d] %s: %s, %s\n", instructionOffset, "get_by_id_chain", pointerToSourceString(vPC[4].u.structure).utf8().data(), pointerToSourceString(vPC[5].u.structureChain).utf8().data());
        return;
    }
    if (vPC[0].u.opcode == interpreter->getOpcode(op_put_by_id)) {
        printStructure("put_by_id", vPC, 4);
        return;
    }
    if (vPC[0].u.opcode == interpreter->getOpcode(op_put_by_id_replace)) {
        printStructure("put_by_id_replace", vPC, 4);
        return;
    }
    if (vPC[0].u.opcode == interpreter->getOpcode(op_resolve_global)) {
        printStructure("resolve_global", vPC, 4);
        return;
    }
    if (vPC[0].u.opcode == interpreter->getOpcode(op_resolve_global_dynamic)) {
        printStructure("resolve_global_dynamic", vPC, 4);
        return;
    }

    // These m_instructions doesn't ref Structures.
    ASSERT(vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id_generic) || vPC[0].u.opcode == interpreter->getOpcode(op_put_by_id_generic) || vPC[0].u.opcode == interpreter->getOpcode(op_call) || vPC[0].u.opcode == interpreter->getOpcode(op_call_eval) || vPC[0].u.opcode == interpreter->getOpcode(op_construct));
}

void CodeBlock::dump(ExecState* exec)
{
    size_t instructionCount = 0;

    for (size_t i = 0; i < instructions().size(); i += opcodeLengths[exec->interpreter()->getOpcodeID(instructions()[i].u.opcode)])
        ++instructionCount;

    dataLog(
        "%lu m_instructions; %lu bytes at %p (%s); %d parameter(s); %d callee register(s); %d variable(s)",
        static_cast<unsigned long>(instructions().size()),
        static_cast<unsigned long>(instructions().size() * sizeof(Instruction)),
        this, codeTypeToString(codeType()), m_numParameters, m_numCalleeRegisters,
        m_numVars);
    if (m_numCapturedVars)
        dataLog("; %d captured var(s)", m_numCapturedVars);
    if (usesArguments()) {
        dataLog(
            "; uses arguments, in r%d, r%d",
            argumentsRegister(),
            unmodifiedArgumentsRegister(argumentsRegister()));
    }
    if (needsFullScopeChain() && codeType() == FunctionCode)
        dataLog("; activation in r%d", activationRegister());
    dataLog("\n\n");

    Vector<Instruction>::const_iterator begin = instructions().begin();
    Vector<Instruction>::const_iterator end = instructions().end();
    for (Vector<Instruction>::const_iterator it = begin; it != end; ++it)
        dump(exec, begin, it);

    if (!m_identifiers.isEmpty()) {
        dataLog("\nIdentifiers:\n");
        size_t i = 0;
        do {
            dataLog("  id%u = %s\n", static_cast<unsigned>(i), m_identifiers[i].ustring().utf8().data());
            ++i;
        } while (i != m_identifiers.size());
    }

    if (!m_constantRegisters.isEmpty()) {
        dataLog("\nConstants:\n");
        size_t i = 0;
        do {
            dataLog("   k%u = %s\n", static_cast<unsigned>(i), valueToSourceString(exec, m_constantRegisters[i].get()).utf8().data());
            ++i;
        } while (i < m_constantRegisters.size());
    }

    if (m_rareData && !m_rareData->m_regexps.isEmpty()) {
        dataLog("\nm_regexps:\n");
        size_t i = 0;
        do {
            dataLog("  re%u = %s\n", static_cast<unsigned>(i), regexpToSourceString(m_rareData->m_regexps[i].get()).utf8().data());
            ++i;
        } while (i < m_rareData->m_regexps.size());
    }

#if ENABLE(JIT)
    if (!m_globalResolveInfos.isEmpty() || !m_structureStubInfos.isEmpty())
        dataLog("\nStructures:\n");

    if (!m_globalResolveInfos.isEmpty()) {
        size_t i = 0;
        do {
             printGlobalResolveInfo(m_globalResolveInfos[i], instructionOffsetForNth(exec, instructions(), i + 1, isGlobalResolve));
             ++i;
        } while (i < m_globalResolveInfos.size());
    }
#endif
#if ENABLE(CLASSIC_INTERPRETER)
    if (!m_globalResolveInstructions.isEmpty() || !m_propertyAccessInstructions.isEmpty())
        dataLog("\nStructures:\n");

    if (!m_globalResolveInstructions.isEmpty()) {
        size_t i = 0;
        do {
             printStructures(&instructions()[m_globalResolveInstructions[i]]);
             ++i;
        } while (i < m_globalResolveInstructions.size());
    }
    if (!m_propertyAccessInstructions.isEmpty()) {
        size_t i = 0;
        do {
            printStructures(&instructions()[m_propertyAccessInstructions[i]]);
             ++i;
        } while (i < m_propertyAccessInstructions.size());
    }
#endif

    if (m_rareData && !m_rareData->m_exceptionHandlers.isEmpty()) {
        dataLog("\nException Handlers:\n");
        unsigned i = 0;
        do {
            dataLog("\t %d: { start: [%4d] end: [%4d] target: [%4d] }\n", i + 1, m_rareData->m_exceptionHandlers[i].start, m_rareData->m_exceptionHandlers[i].end, m_rareData->m_exceptionHandlers[i].target);
            ++i;
        } while (i < m_rareData->m_exceptionHandlers.size());
    }
    
    if (m_rareData && !m_rareData->m_immediateSwitchJumpTables.isEmpty()) {
        dataLog("Immediate Switch Jump Tables:\n");
        unsigned i = 0;
        do {
            dataLog("  %1d = {\n", i);
            int entry = 0;
            Vector<int32_t>::const_iterator end = m_rareData->m_immediateSwitchJumpTables[i].branchOffsets.end();
            for (Vector<int32_t>::const_iterator iter = m_rareData->m_immediateSwitchJumpTables[i].branchOffsets.begin(); iter != end; ++iter, ++entry) {
                if (!*iter)
                    continue;
                dataLog("\t\t%4d => %04d\n", entry + m_rareData->m_immediateSwitchJumpTables[i].min, *iter);
            }
            dataLog("      }\n");
            ++i;
        } while (i < m_rareData->m_immediateSwitchJumpTables.size());
    }
    
    if (m_rareData && !m_rareData->m_characterSwitchJumpTables.isEmpty()) {
        dataLog("\nCharacter Switch Jump Tables:\n");
        unsigned i = 0;
        do {
            dataLog("  %1d = {\n", i);
            int entry = 0;
            Vector<int32_t>::const_iterator end = m_rareData->m_characterSwitchJumpTables[i].branchOffsets.end();
            for (Vector<int32_t>::const_iterator iter = m_rareData->m_characterSwitchJumpTables[i].branchOffsets.begin(); iter != end; ++iter, ++entry) {
                if (!*iter)
                    continue;
                ASSERT(!((i + m_rareData->m_characterSwitchJumpTables[i].min) & ~0xFFFF));
                UChar ch = static_cast<UChar>(entry + m_rareData->m_characterSwitchJumpTables[i].min);
                dataLog("\t\t\"%s\" => %04d\n", UString(&ch, 1).utf8().data(), *iter);
        }
            dataLog("      }\n");
            ++i;
        } while (i < m_rareData->m_characterSwitchJumpTables.size());
    }
    
    if (m_rareData && !m_rareData->m_stringSwitchJumpTables.isEmpty()) {
        dataLog("\nString Switch Jump Tables:\n");
        unsigned i = 0;
        do {
            dataLog("  %1d = {\n", i);
            StringJumpTable::StringOffsetTable::const_iterator end = m_rareData->m_stringSwitchJumpTables[i].offsetTable.end();
            for (StringJumpTable::StringOffsetTable::const_iterator iter = m_rareData->m_stringSwitchJumpTables[i].offsetTable.begin(); iter != end; ++iter)
                dataLog("\t\t\"%s\" => %04d\n", UString(iter->first).utf8().data(), iter->second.branchOffset);
            dataLog("      }\n");
            ++i;
        } while (i < m_rareData->m_stringSwitchJumpTables.size());
    }

    dataLog("\n");
}

void CodeBlock::dump(ExecState* exec, const Vector<Instruction>::const_iterator& begin, Vector<Instruction>::const_iterator& it)
{
    int location = it - begin;
    switch (exec->interpreter()->getOpcodeID(it->u.opcode)) {
        case op_enter: {
            dataLog("[%4d] enter", location);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_create_activation: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] create_activation %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_create_arguments: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] create_arguments\t %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_init_lazy_reg: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] init_lazy_reg\t %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_create_this: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] create_this %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_convert_this: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] convert_this\t %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            ++it; // Skip value profile.
            break;
        }
        case op_new_object: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] new_object\t %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_new_array: {
            int dst = (++it)->u.operand;
            int argv = (++it)->u.operand;
            int argc = (++it)->u.operand;
            dataLog("[%4d] new_array\t %s, %s, %d", location, registerName(exec, dst).data(), registerName(exec, argv).data(), argc);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_new_array_buffer: {
            int dst = (++it)->u.operand;
            int argv = (++it)->u.operand;
            int argc = (++it)->u.operand;
            dataLog("[%4d] new_array_buffer %s, %d, %d", location, registerName(exec, dst).data(), argv, argc);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_new_regexp: {
            int r0 = (++it)->u.operand;
            int re0 = (++it)->u.operand;
            dataLog("[%4d] new_regexp\t %s, ", location, registerName(exec, r0).data());
            if (r0 >=0 && r0 < (int)numberOfRegExps())
                dataLog("%s", regexpName(re0, regexp(re0)).data());
            else
                dataLog("bad_regexp(%d)", re0);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_mov: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            dataLog("[%4d] mov\t\t %s, %s", location, registerName(exec, r0).data(), registerName(exec, r1).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_not: {
            printUnaryOp(exec, location, it, "not");
            break;
        }
        case op_eq: {
            printBinaryOp(exec, location, it, "eq");
            break;
        }
        case op_eq_null: {
            printUnaryOp(exec, location, it, "eq_null");
            break;
        }
        case op_neq: {
            printBinaryOp(exec, location, it, "neq");
            break;
        }
        case op_neq_null: {
            printUnaryOp(exec, location, it, "neq_null");
            break;
        }
        case op_stricteq: {
            printBinaryOp(exec, location, it, "stricteq");
            break;
        }
        case op_nstricteq: {
            printBinaryOp(exec, location, it, "nstricteq");
            break;
        }
        case op_less: {
            printBinaryOp(exec, location, it, "less");
            break;
        }
        case op_lesseq: {
            printBinaryOp(exec, location, it, "lesseq");
            break;
        }
        case op_greater: {
            printBinaryOp(exec, location, it, "greater");
            break;
        }
        case op_greatereq: {
            printBinaryOp(exec, location, it, "greatereq");
            break;
        }
        case op_pre_inc: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] pre_inc\t\t %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_pre_dec: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] pre_dec\t\t %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_post_inc: {
            printUnaryOp(exec, location, it, "post_inc");
            break;
        }
        case op_post_dec: {
            printUnaryOp(exec, location, it, "post_dec");
            break;
        }
        case op_to_jsnumber: {
            printUnaryOp(exec, location, it, "to_jsnumber");
            break;
        }
        case op_negate: {
            printUnaryOp(exec, location, it, "negate");
            break;
        }
        case op_add: {
            printBinaryOp(exec, location, it, "add");
            ++it;
            break;
        }
        case op_mul: {
            printBinaryOp(exec, location, it, "mul");
            ++it;
            break;
        }
        case op_div: {
            printBinaryOp(exec, location, it, "div");
            ++it;
            break;
        }
        case op_mod: {
            printBinaryOp(exec, location, it, "mod");
            break;
        }
        case op_sub: {
            printBinaryOp(exec, location, it, "sub");
            ++it;
            break;
        }
        case op_lshift: {
            printBinaryOp(exec, location, it, "lshift");
            break;            
        }
        case op_rshift: {
            printBinaryOp(exec, location, it, "rshift");
            break;
        }
        case op_urshift: {
            printBinaryOp(exec, location, it, "urshift");
            break;
        }
        case op_bitand: {
            printBinaryOp(exec, location, it, "bitand");
            ++it;
            break;
        }
        case op_bitxor: {
            printBinaryOp(exec, location, it, "bitxor");
            ++it;
            break;
        }
        case op_bitor: {
            printBinaryOp(exec, location, it, "bitor");
            ++it;
            break;
        }
        case op_check_has_instance: {
            int base = (++it)->u.operand;
            dataLog("[%4d] check_has_instance\t\t %s", location, registerName(exec, base).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_instanceof: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            int r3 = (++it)->u.operand;
            dataLog("[%4d] instanceof\t\t %s, %s, %s, %s", location, registerName(exec, r0).data(), registerName(exec, r1).data(), registerName(exec, r2).data(), registerName(exec, r3).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_typeof: {
            printUnaryOp(exec, location, it, "typeof");
            break;
        }
        case op_is_undefined: {
            printUnaryOp(exec, location, it, "is_undefined");
            break;
        }
        case op_is_boolean: {
            printUnaryOp(exec, location, it, "is_boolean");
            break;
        }
        case op_is_number: {
            printUnaryOp(exec, location, it, "is_number");
            break;
        }
        case op_is_string: {
            printUnaryOp(exec, location, it, "is_string");
            break;
        }
        case op_is_object: {
            printUnaryOp(exec, location, it, "is_object");
            break;
        }
        case op_is_function: {
            printUnaryOp(exec, location, it, "is_function");
            break;
        }
        case op_in: {
            printBinaryOp(exec, location, it, "in");
            break;
        }
        case op_resolve: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            dataLog("[%4d] resolve\t\t %s, %s", location, registerName(exec, r0).data(), idName(id0, m_identifiers[id0]).data());
            dumpBytecodeCommentAndNewLine(location);
            it++;
            break;
        }
        case op_resolve_skip: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int skipLevels = (++it)->u.operand;
            dataLog("[%4d] resolve_skip\t %s, %s, %d", location, registerName(exec, r0).data(), idName(id0, m_identifiers[id0]).data(), skipLevels);
            dumpBytecodeCommentAndNewLine(location);
            it++;
            break;
        }
        case op_resolve_global: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            dataLog("[%4d] resolve_global\t %s, %s", location, registerName(exec, r0).data(), idName(id0, m_identifiers[id0]).data());
            dumpBytecodeCommentAndNewLine(location);
            it += 3;
            break;
        }
        case op_resolve_global_dynamic: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            JSValue scope = JSValue((++it)->u.jsCell.get());
            ++it;
            int depth = (++it)->u.operand;
            dataLog("[%4d] resolve_global_dynamic\t %s, %s, %s, %d", location, registerName(exec, r0).data(), valueToSourceString(exec, scope).utf8().data(), idName(id0, m_identifiers[id0]).data(), depth);
            dumpBytecodeCommentAndNewLine(location);
            ++it;
            break;
        }
        case op_get_scoped_var: {
            int r0 = (++it)->u.operand;
            int index = (++it)->u.operand;
            int skipLevels = (++it)->u.operand;
            dataLog("[%4d] get_scoped_var\t %s, %d, %d", location, registerName(exec, r0).data(), index, skipLevels);
            dumpBytecodeCommentAndNewLine(location);
            it++;
            break;
        }
        case op_put_scoped_var: {
            int index = (++it)->u.operand;
            int skipLevels = (++it)->u.operand;
            int r0 = (++it)->u.operand;
            dataLog("[%4d] put_scoped_var\t %d, %d, %s", location, index, skipLevels, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_get_global_var: {
            int r0 = (++it)->u.operand;
            WriteBarrier<Unknown>* registerPointer = (++it)->u.registerPointer;
            dataLog("[%4d] get_global_var\t %s, g%d(%p)", location, registerName(exec, r0).data(), m_globalObject->findRegisterIndex(registerPointer), registerPointer);
            dumpBytecodeCommentAndNewLine(location);
            it++;
            break;
        }
        case op_get_global_var_watchable: {
            int r0 = (++it)->u.operand;
            WriteBarrier<Unknown>* registerPointer = (++it)->u.registerPointer;
            dataLog("[%4d] get_global_var_watchable\t %s, g%d(%p)", location, registerName(exec, r0).data(), m_globalObject->findRegisterIndex(registerPointer), registerPointer);
            dumpBytecodeCommentAndNewLine(location);
            it++;
            it++;
            break;
        }
        case op_put_global_var: {
            WriteBarrier<Unknown>* registerPointer = (++it)->u.registerPointer;
            int r0 = (++it)->u.operand;
            dataLog("[%4d] put_global_var\t g%d(%p), %s", location, m_globalObject->findRegisterIndex(registerPointer), registerPointer, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_put_global_var_check: {
            WriteBarrier<Unknown>* registerPointer = (++it)->u.registerPointer;
            int r0 = (++it)->u.operand;
            dataLog("[%4d] put_global_var_check\t g%d(%p), %s", location, m_globalObject->findRegisterIndex(registerPointer), registerPointer, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            it++;
            it++;
            break;
        }
        case op_resolve_base: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int isStrict = (++it)->u.operand;
            dataLog("[%4d] resolve_base%s\t %s, %s", location, isStrict ? "_strict" : "", registerName(exec, r0).data(), idName(id0, m_identifiers[id0]).data());
            dumpBytecodeCommentAndNewLine(location);
            it++;
            break;
        }
        case op_ensure_property_exists: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            dataLog("[%4d] ensure_property_exists\t %s, %s", location, registerName(exec, r0).data(), idName(id0, m_identifiers[id0]).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_resolve_with_base: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            dataLog("[%4d] resolve_with_base %s, %s, %s", location, registerName(exec, r0).data(), registerName(exec, r1).data(), idName(id0, m_identifiers[id0]).data());
            dumpBytecodeCommentAndNewLine(location);
            it++;
            break;
        }
        case op_resolve_with_this: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            dataLog("[%4d] resolve_with_this %s, %s, %s", location, registerName(exec, r0).data(), registerName(exec, r1).data(), idName(id0, m_identifiers[id0]).data());
            dumpBytecodeCommentAndNewLine(location);
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
            printGetByIdOp(exec, location, it);
            printGetByIdCacheStatus(exec, location);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_get_arguments_length: {
            printUnaryOp(exec, location, it, "get_arguments_length");
            it++;
            break;
        }
        case op_put_by_id: {
            printPutByIdOp(exec, location, it, "put_by_id");
            break;
        }
        case op_put_by_id_out_of_line: {
            printPutByIdOp(exec, location, it, "put_by_id_out_of_line");
            break;
        }
        case op_put_by_id_replace: {
            printPutByIdOp(exec, location, it, "put_by_id_replace");
            break;
        }
        case op_put_by_id_transition: {
            printPutByIdOp(exec, location, it, "put_by_id_transition");
            break;
        }
        case op_put_by_id_transition_direct: {
            printPutByIdOp(exec, location, it, "put_by_id_transition_direct");
            break;
        }
        case op_put_by_id_transition_direct_out_of_line: {
            printPutByIdOp(exec, location, it, "put_by_id_transition_direct_out_of_line");
            break;
        }
        case op_put_by_id_transition_normal: {
            printPutByIdOp(exec, location, it, "put_by_id_transition_normal");
            break;
        }
        case op_put_by_id_transition_normal_out_of_line: {
            printPutByIdOp(exec, location, it, "put_by_id_transition_normal_out_of_line");
            break;
        }
        case op_put_by_id_generic: {
            printPutByIdOp(exec, location, it, "put_by_id_generic");
            break;
        }
        case op_put_getter_setter: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            dataLog("[%4d] put_getter_setter\t %s, %s, %s, %s", location, registerName(exec, r0).data(), idName(id0, m_identifiers[id0]).data(), registerName(exec, r1).data(), registerName(exec, r2).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_method_check: {
            dataLog("[%4d] method_check", location);
#if ENABLE(JIT)
            if (numberOfMethodCallLinkInfos()) {
                MethodCallLinkInfo& methodCall = getMethodCallLinkInfo(location);
                dataLog(" jit(");
                if (!methodCall.seen)
                    dataLog("not seen");
                else {
                    // Use the fact that MethodCallLinkStatus already does smart things
                    // for decoding seen method calls.
                    MethodCallLinkStatus status = MethodCallLinkStatus::computeFor(this, location);
                    if (!status)
                        dataLog("not set");
                    else {
                        dataLog("function = %p (executable = ", status.function());
                        JSCell* functionAsCell = getJSFunction(status.function());
                        if (functionAsCell)
                            dataLog("%p", jsCast<JSFunction*>(functionAsCell)->executable());
                        else
                            dataLog("N/A");
                        dataLog("), struct = %p", status.structure());
                        if (status.needsPrototypeCheck())
                            dataLog(", prototype = %p, struct = %p", status.prototype(), status.prototypeStructure());
                    }
                }
                dataLog(")");
            }
#endif
            dumpBytecodeCommentAndNewLine(location);
            ++it;
            printGetByIdOp(exec, location, it);
            printGetByIdCacheStatus(exec, location);
            dataLog("\n");
            break;
        }
        case op_del_by_id: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            dataLog("[%4d] del_by_id\t %s, %s, %s", location, registerName(exec, r0).data(), registerName(exec, r1).data(), idName(id0, m_identifiers[id0]).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_get_by_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            dataLog("[%4d] get_by_val\t %s, %s, %s", location, registerName(exec, r0).data(), registerName(exec, r1).data(), registerName(exec, r2).data());
            dumpBytecodeCommentAndNewLine(location);
            it++;
            it++;
            break;
        }
        case op_get_argument_by_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            dataLog("[%4d] get_argument_by_val\t %s, %s, %s", location, registerName(exec, r0).data(), registerName(exec, r1).data(), registerName(exec, r2).data());
            dumpBytecodeCommentAndNewLine(location);
            ++it;
            ++it;
            break;
        }
        case op_get_by_pname: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            int r3 = (++it)->u.operand;
            int r4 = (++it)->u.operand;
            int r5 = (++it)->u.operand;
            dataLog("[%4d] get_by_pname\t %s, %s, %s, %s, %s, %s", location, registerName(exec, r0).data(), registerName(exec, r1).data(), registerName(exec, r2).data(), registerName(exec, r3).data(), registerName(exec, r4).data(), registerName(exec, r5).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_put_by_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            dataLog("[%4d] put_by_val\t %s, %s, %s", location, registerName(exec, r0).data(), registerName(exec, r1).data(), registerName(exec, r2).data());
            dumpBytecodeCommentAndNewLine(location);
            ++it;
            break;
        }
        case op_del_by_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            dataLog("[%4d] del_by_val\t %s, %s, %s", location, registerName(exec, r0).data(), registerName(exec, r1).data(), registerName(exec, r2).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_put_by_index: {
            int r0 = (++it)->u.operand;
            unsigned n0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            dataLog("[%4d] put_by_index\t %s, %u, %s", location, registerName(exec, r0).data(), n0, registerName(exec, r1).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_jmp: {
            int offset = (++it)->u.operand;
            dataLog("[%4d] jmp\t\t %d(->%d)", location, offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_loop: {
            int offset = (++it)->u.operand;
            dataLog("[%4d] loop\t\t %d(->%d)", location, offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_jtrue: {
            printConditionalJump(exec, begin, it, location, "jtrue");
            break;
        }
        case op_loop_if_true: {
            printConditionalJump(exec, begin, it, location, "loop_if_true");
            break;
        }
        case op_loop_if_false: {
            printConditionalJump(exec, begin, it, location, "loop_if_false");
            break;
        }
        case op_jfalse: {
            printConditionalJump(exec, begin, it, location, "jfalse");
            break;
        }
        case op_jeq_null: {
            printConditionalJump(exec, begin, it, location, "jeq_null");
            break;
        }
        case op_jneq_null: {
            printConditionalJump(exec, begin, it, location, "jneq_null");
            break;
        }
        case op_jneq_ptr: {
            int r0 = (++it)->u.operand;
            void* pointer = (++it)->u.pointer;
            int offset = (++it)->u.operand;
            dataLog("[%4d] jneq_ptr\t\t %s, %p, %d(->%d)", location, registerName(exec, r0).data(), pointer, offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_jless: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            dataLog("[%4d] jless\t\t %s, %s, %d(->%d)", location, registerName(exec, r0).data(), registerName(exec, r1).data(), offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_jlesseq: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            dataLog("[%4d] jlesseq\t\t %s, %s, %d(->%d)", location, registerName(exec, r0).data(), registerName(exec, r1).data(), offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_jgreater: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            dataLog("[%4d] jgreater\t\t %s, %s, %d(->%d)", location, registerName(exec, r0).data(), registerName(exec, r1).data(), offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_jgreatereq: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            dataLog("[%4d] jgreatereq\t\t %s, %s, %d(->%d)", location, registerName(exec, r0).data(), registerName(exec, r1).data(), offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_jnless: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            dataLog("[%4d] jnless\t\t %s, %s, %d(->%d)", location, registerName(exec, r0).data(), registerName(exec, r1).data(), offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_jnlesseq: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            dataLog("[%4d] jnlesseq\t\t %s, %s, %d(->%d)", location, registerName(exec, r0).data(), registerName(exec, r1).data(), offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_jngreater: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            dataLog("[%4d] jngreater\t\t %s, %s, %d(->%d)", location, registerName(exec, r0).data(), registerName(exec, r1).data(), offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_jngreatereq: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            dataLog("[%4d] jngreatereq\t\t %s, %s, %d(->%d)", location, registerName(exec, r0).data(), registerName(exec, r1).data(), offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_loop_if_less: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            dataLog("[%4d] loop_if_less\t %s, %s, %d(->%d)", location, registerName(exec, r0).data(), registerName(exec, r1).data(), offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_loop_if_lesseq: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            dataLog("[%4d] loop_if_lesseq\t %s, %s, %d(->%d)", location, registerName(exec, r0).data(), registerName(exec, r1).data(), offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_loop_if_greater: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            dataLog("[%4d] loop_if_greater\t %s, %s, %d(->%d)", location, registerName(exec, r0).data(), registerName(exec, r1).data(), offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_loop_if_greatereq: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            dataLog("[%4d] loop_if_greatereq\t %s, %s, %d(->%d)", location, registerName(exec, r0).data(), registerName(exec, r1).data(), offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_loop_hint: {
            dataLog("[%4d] loop_hint", location);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_switch_imm: {
            int tableIndex = (++it)->u.operand;
            int defaultTarget = (++it)->u.operand;
            int scrutineeRegister = (++it)->u.operand;
            dataLog("[%4d] switch_imm\t %d, %d(->%d), %s", location, tableIndex, defaultTarget, location + defaultTarget, registerName(exec, scrutineeRegister).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_switch_char: {
            int tableIndex = (++it)->u.operand;
            int defaultTarget = (++it)->u.operand;
            int scrutineeRegister = (++it)->u.operand;
            dataLog("[%4d] switch_char\t %d, %d(->%d), %s", location, tableIndex, defaultTarget, location + defaultTarget, registerName(exec, scrutineeRegister).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_switch_string: {
            int tableIndex = (++it)->u.operand;
            int defaultTarget = (++it)->u.operand;
            int scrutineeRegister = (++it)->u.operand;
            dataLog("[%4d] switch_string\t %d, %d(->%d), %s", location, tableIndex, defaultTarget, location + defaultTarget, registerName(exec, scrutineeRegister).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_new_func: {
            int r0 = (++it)->u.operand;
            int f0 = (++it)->u.operand;
            int shouldCheck = (++it)->u.operand;
            dataLog("[%4d] new_func\t\t %s, f%d, %s", location, registerName(exec, r0).data(), f0, shouldCheck ? "<Checked>" : "<Unchecked>");
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_new_func_exp: {
            int r0 = (++it)->u.operand;
            int f0 = (++it)->u.operand;
            dataLog("[%4d] new_func_exp\t %s, f%d", location, registerName(exec, r0).data(), f0);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_call: {
            printCallOp(exec, location, it, "call", DumpCaches);
            break;
        }
        case op_call_eval: {
            printCallOp(exec, location, it, "call_eval", DontDumpCaches);
            break;
        }
        case op_call_varargs: {
            int callee = (++it)->u.operand;
            int thisValue = (++it)->u.operand;
            int arguments = (++it)->u.operand;
            int firstFreeRegister = (++it)->u.operand;
            dataLog("[%4d] call_varargs\t %s, %s, %s, %d", location, registerName(exec, callee).data(), registerName(exec, thisValue).data(), registerName(exec, arguments).data(), firstFreeRegister);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_tear_off_activation: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            dataLog("[%4d] tear_off_activation\t %s, %s", location, registerName(exec, r0).data(), registerName(exec, r1).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_tear_off_arguments: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] tear_off_arguments %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_ret: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] ret\t\t %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_call_put_result: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] call_put_result\t\t %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            it++;
            break;
        }
        case op_ret_object_or_this: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            dataLog("[%4d] constructor_ret\t\t %s %s", location, registerName(exec, r0).data(), registerName(exec, r1).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_construct: {
            printCallOp(exec, location, it, "construct", DumpCaches);
            break;
        }
        case op_strcat: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int count = (++it)->u.operand;
            dataLog("[%4d] strcat\t\t %s, %s, %d", location, registerName(exec, r0).data(), registerName(exec, r1).data(), count);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_to_primitive: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            dataLog("[%4d] to_primitive\t %s, %s", location, registerName(exec, r0).data(), registerName(exec, r1).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_get_pnames: {
            int r0 = it[1].u.operand;
            int r1 = it[2].u.operand;
            int r2 = it[3].u.operand;
            int r3 = it[4].u.operand;
            int offset = it[5].u.operand;
            dataLog("[%4d] get_pnames\t %s, %s, %s, %s, %d(->%d)", location, registerName(exec, r0).data(), registerName(exec, r1).data(), registerName(exec, r2).data(), registerName(exec, r3).data(), offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
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
            dataLog("[%4d] next_pname\t %s, %s, %s, %s, %s, %d(->%d)", location, registerName(exec, dest).data(), registerName(exec, base).data(), registerName(exec, i).data(), registerName(exec, size).data(), registerName(exec, iter).data(), offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            it += OPCODE_LENGTH(op_next_pname) - 1;
            break;
        }
        case op_push_scope: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] push_scope\t %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_pop_scope: {
            dataLog("[%4d] pop_scope", location);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_push_new_scope: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            dataLog("[%4d] push_new_scope \t%s, %s, %s", location, registerName(exec, r0).data(), idName(id0, m_identifiers[id0]).data(), registerName(exec, r1).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_jmp_scopes: {
            int scopeDelta = (++it)->u.operand;
            int offset = (++it)->u.operand;
            dataLog("[%4d] jmp_scopes\t^%d, %d(->%d)", location, scopeDelta, offset, location + offset);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_catch: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] catch\t\t %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_throw: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] throw\t\t %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_throw_reference_error: {
            int k0 = (++it)->u.operand;
            dataLog("[%4d] throw_reference_error\t %s", location, constantName(exec, k0, getConstant(k0)).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_debug: {
            int debugHookID = (++it)->u.operand;
            int firstLine = (++it)->u.operand;
            int lastLine = (++it)->u.operand;
            int column = (++it)->u.operand;
            dataLog("[%4d] debug\t\t %s, %d, %d, %d", location, debugHookName(debugHookID), firstLine, lastLine, column);
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_profile_will_call: {
            int function = (++it)->u.operand;
            dataLog("[%4d] profile_will_call %s", location, registerName(exec, function).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_profile_did_call: {
            int function = (++it)->u.operand;
            dataLog("[%4d] profile_did_call\t %s", location, registerName(exec, function).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
        case op_end: {
            int r0 = (++it)->u.operand;
            dataLog("[%4d] end\t\t %s", location, registerName(exec, r0).data());
            dumpBytecodeCommentAndNewLine(location);
            break;
        }
    }
}

#if DUMP_CODE_BLOCK_STATISTICS
static HashSet<CodeBlock*> liveCodeBlockSet;
#endif

#define FOR_EACH_MEMBER_VECTOR(macro) \
    macro(instructions) \
    macro(globalResolveInfos) \
    macro(structureStubInfos) \
    macro(callLinkInfos) \
    macro(linkedCallerList) \
    macro(identifiers) \
    macro(functionExpressions) \
    macro(constantRegisters)

#define FOR_EACH_MEMBER_VECTOR_RARE_DATA(macro) \
    macro(regexps) \
    macro(functions) \
    macro(exceptionHandlers) \
    macro(immediateSwitchJumpTables) \
    macro(characterSwitchJumpTables) \
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

void CodeBlock::dumpStatistics()
{
#if DUMP_CODE_BLOCK_STATISTICS
    #define DEFINE_VARS(name) size_t name##IsNotEmpty = 0; size_t name##TotalSize = 0;
        FOR_EACH_MEMBER_VECTOR(DEFINE_VARS)
        FOR_EACH_MEMBER_VECTOR_RARE_DATA(DEFINE_VARS)
    #undef DEFINE_VARS

    // Non-vector data members
    size_t evalCodeCacheIsNotEmpty = 0;

    size_t symbolTableIsNotEmpty = 0;
    size_t symbolTableTotalSize = 0;

    size_t hasRareData = 0;

    size_t isFunctionCode = 0;
    size_t isGlobalCode = 0;
    size_t isEvalCode = 0;

    HashSet<CodeBlock*>::const_iterator end = liveCodeBlockSet.end();
    for (HashSet<CodeBlock*>::const_iterator it = liveCodeBlockSet.begin(); it != end; ++it) {
        CodeBlock* codeBlock = *it;

        #define GET_STATS(name) if (!codeBlock->m_##name.isEmpty()) { name##IsNotEmpty++; name##TotalSize += sizeInBytes(codeBlock->m_##name); }
            FOR_EACH_MEMBER_VECTOR(GET_STATS)
        #undef GET_STATS

        if (!codeBlock->m_symbolTable.isEmpty()) {
            symbolTableIsNotEmpty++;
            symbolTableTotalSize += (codeBlock->m_symbolTable.capacity() * (sizeof(SymbolTable::KeyType) + sizeof(SymbolTable::MappedType)));
        }

        if (codeBlock->m_rareData) {
            hasRareData++;
            #define GET_STATS(name) if (!codeBlock->m_rareData->m_##name.isEmpty()) { name##IsNotEmpty++; name##TotalSize += sizeInBytes(codeBlock->m_rareData->m_##name); }
                FOR_EACH_MEMBER_VECTOR_RARE_DATA(GET_STATS)
            #undef GET_STATS

            if (!codeBlock->m_rareData->m_evalCodeCache.isEmpty())
                evalCodeCacheIsNotEmpty++;
        }

        switch (codeBlock->codeType()) {
            case FunctionCode:
                ++isFunctionCode;
                break;
            case GlobalCode:
                ++isGlobalCode;
                break;
            case EvalCode:
                ++isEvalCode;
                break;
        }
    }

    size_t totalSize = 0;

    #define GET_TOTAL_SIZE(name) totalSize += name##TotalSize;
        FOR_EACH_MEMBER_VECTOR(GET_TOTAL_SIZE)
        FOR_EACH_MEMBER_VECTOR_RARE_DATA(GET_TOTAL_SIZE)
    #undef GET_TOTAL_SIZE

    totalSize += symbolTableTotalSize;
    totalSize += (liveCodeBlockSet.size() * sizeof(CodeBlock));

    dataLog("Number of live CodeBlocks: %d\n", liveCodeBlockSet.size());
    dataLog("Size of a single CodeBlock [sizeof(CodeBlock)]: %zu\n", sizeof(CodeBlock));
    dataLog("Size of all CodeBlocks: %zu\n", totalSize);
    dataLog("Average size of a CodeBlock: %zu\n", totalSize / liveCodeBlockSet.size());

    dataLog("Number of FunctionCode CodeBlocks: %zu (%.3f%%)\n", isFunctionCode, static_cast<double>(isFunctionCode) * 100.0 / liveCodeBlockSet.size());
    dataLog("Number of GlobalCode CodeBlocks: %zu (%.3f%%)\n", isGlobalCode, static_cast<double>(isGlobalCode) * 100.0 / liveCodeBlockSet.size());
    dataLog("Number of EvalCode CodeBlocks: %zu (%.3f%%)\n", isEvalCode, static_cast<double>(isEvalCode) * 100.0 / liveCodeBlockSet.size());

    dataLog("Number of CodeBlocks with rare data: %zu (%.3f%%)\n", hasRareData, static_cast<double>(hasRareData) * 100.0 / liveCodeBlockSet.size());

    #define PRINT_STATS(name) dataLog("Number of CodeBlocks with " #name ": %zu\n", name##IsNotEmpty); dataLog("Size of all " #name ": %zu\n", name##TotalSize); 
        FOR_EACH_MEMBER_VECTOR(PRINT_STATS)
        FOR_EACH_MEMBER_VECTOR_RARE_DATA(PRINT_STATS)
    #undef PRINT_STATS

    dataLog("Number of CodeBlocks with evalCodeCache: %zu\n", evalCodeCacheIsNotEmpty);
    dataLog("Number of CodeBlocks with symbolTable: %zu\n", symbolTableIsNotEmpty);

    dataLog("Size of all symbolTables: %zu\n", symbolTableTotalSize);

#else
    dataLog("Dumping CodeBlock statistics is not enabled.\n");
#endif
}

CodeBlock::CodeBlock(CopyParsedBlockTag, CodeBlock& other, SymbolTable* symTab)
    : m_globalObject(other.m_globalObject)
    , m_heap(other.m_heap)
    , m_numCalleeRegisters(other.m_numCalleeRegisters)
    , m_numVars(other.m_numVars)
    , m_numCapturedVars(other.m_numCapturedVars)
    , m_isConstructor(other.m_isConstructor)
    , m_ownerExecutable(*other.m_globalData, other.m_ownerExecutable.get(), other.m_ownerExecutable.get())
    , m_globalData(other.m_globalData)
    , m_instructions(other.m_instructions)
    , m_thisRegister(other.m_thisRegister)
    , m_argumentsRegister(other.m_argumentsRegister)
    , m_activationRegister(other.m_activationRegister)
    , m_needsFullScopeChain(other.m_needsFullScopeChain)
    , m_usesEval(other.m_usesEval)
    , m_isNumericCompareFunction(other.m_isNumericCompareFunction)
    , m_isStrictMode(other.m_isStrictMode)
    , m_codeType(other.m_codeType)
    , m_source(other.m_source)
    , m_sourceOffset(other.m_sourceOffset)
#if ENABLE(JIT)
    , m_globalResolveInfos(other.m_globalResolveInfos.size())
#endif
#if ENABLE(VALUE_PROFILER)
    , m_executionEntryCount(0)
#endif
    , m_jumpTargets(other.m_jumpTargets)
    , m_loopTargets(other.m_loopTargets)
    , m_identifiers(other.m_identifiers)
    , m_constantRegisters(other.m_constantRegisters)
    , m_functionDecls(other.m_functionDecls)
    , m_functionExprs(other.m_functionExprs)
    , m_symbolTable(symTab)
    , m_osrExitCounter(0)
    , m_optimizationDelayCounter(0)
    , m_reoptimizationRetryCounter(0)
    , m_lineInfo(other.m_lineInfo)
#if ENABLE(BYTECODE_COMMENTS)
    , m_bytecodeCommentIterator(0)
#endif
#if ENABLE(JIT)
    , m_canCompileWithDFGState(DFG::CapabilityLevelNotSet)
#endif
{
    setNumParameters(other.numParameters());
    optimizeAfterWarmUp();
    jitAfterWarmUp();

#if ENABLE(JIT)
    for (unsigned i = m_globalResolveInfos.size(); i--;)
        m_globalResolveInfos[i] = GlobalResolveInfo(other.m_globalResolveInfos[i].bytecodeOffset);
#endif

    if (other.m_rareData) {
        createRareDataIfNecessary();
        
        m_rareData->m_exceptionHandlers = other.m_rareData->m_exceptionHandlers;
        m_rareData->m_regexps = other.m_rareData->m_regexps;
        m_rareData->m_constantBuffers = other.m_rareData->m_constantBuffers;
        m_rareData->m_immediateSwitchJumpTables = other.m_rareData->m_immediateSwitchJumpTables;
        m_rareData->m_characterSwitchJumpTables = other.m_rareData->m_characterSwitchJumpTables;
        m_rareData->m_stringSwitchJumpTables = other.m_rareData->m_stringSwitchJumpTables;
        m_rareData->m_expressionInfo = other.m_rareData->m_expressionInfo;
    }
}

CodeBlock::CodeBlock(ScriptExecutable* ownerExecutable, CodeType codeType, JSGlobalObject *globalObject, PassRefPtr<SourceProvider> sourceProvider, unsigned sourceOffset, SymbolTable* symTab, bool isConstructor, PassOwnPtr<CodeBlock> alternative)
    : m_globalObject(globalObject->globalData(), ownerExecutable, globalObject)
    , m_heap(&m_globalObject->globalData().heap)
    , m_numCalleeRegisters(0)
    , m_numVars(0)
    , m_isConstructor(isConstructor)
    , m_numParameters(0)
    , m_ownerExecutable(globalObject->globalData(), ownerExecutable, ownerExecutable)
    , m_globalData(0)
    , m_argumentsRegister(-1)
    , m_needsFullScopeChain(ownerExecutable->needsActivation())
    , m_usesEval(ownerExecutable->usesEval())
    , m_isNumericCompareFunction(false)
    , m_isStrictMode(ownerExecutable->isStrictMode())
    , m_codeType(codeType)
    , m_source(sourceProvider)
    , m_sourceOffset(sourceOffset)
#if ENABLE(VALUE_PROFILER)
    , m_executionEntryCount(0)
#endif
    , m_symbolTable(symTab)
    , m_alternative(alternative)
    , m_osrExitCounter(0)
    , m_optimizationDelayCounter(0)
    , m_reoptimizationRetryCounter(0)
#if ENABLE(BYTECODE_COMMENTS)
    , m_bytecodeCommentIterator(0)
#endif
{
    ASSERT(m_source);
    
    optimizeAfterWarmUp();
    jitAfterWarmUp();

#if DUMP_CODE_BLOCK_STATISTICS
    liveCodeBlockSet.add(this);
#endif
}

CodeBlock::~CodeBlock()
{
#if ENABLE(DFG_JIT)
    // Remove myself from the set of DFG code blocks. Note that I may not be in this set
    // (because I'm not a DFG code block), in which case this is a no-op anyway.
    m_globalData->heap.m_dfgCodeBlocks.m_set.remove(this);
#endif
    
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

    for (size_t size = m_structureStubInfos.size(), i = 0; i < size; ++i)
        m_structureStubInfos[i].deref();
#endif // ENABLE(JIT)

#if DUMP_CODE_BLOCK_STATISTICS
    liveCodeBlockSet.remove(this);
#endif
}

void CodeBlock::setNumParameters(int newValue)
{
    m_numParameters = newValue;

#if ENABLE(VALUE_PROFILER)
    m_argumentValueProfiles.resize(newValue);
#endif
}

void CodeBlock::addParameter()
{
    m_numParameters++;

#if ENABLE(VALUE_PROFILER)
    m_argumentValueProfiles.append(ValueProfile());
#endif
}

void CodeBlock::visitStructures(SlotVisitor& visitor, Instruction* vPC)
{
    Interpreter* interpreter = m_globalData->interpreter;

    if (vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id) && vPC[4].u.structure) {
        visitor.append(&vPC[4].u.structure);
        return;
    }

    if (vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id_self) || vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id_getter_self) || vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id_custom_self)) {
        visitor.append(&vPC[4].u.structure);
        return;
    }
    if (vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id_proto) || vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id_getter_proto) || vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id_custom_proto)) {
        visitor.append(&vPC[4].u.structure);
        visitor.append(&vPC[5].u.structure);
        return;
    }
    if (vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id_chain) || vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id_getter_chain) || vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id_custom_chain)) {
        visitor.append(&vPC[4].u.structure);
        if (vPC[5].u.structureChain)
            visitor.append(&vPC[5].u.structureChain);
        return;
    }
    if (vPC[0].u.opcode == interpreter->getOpcode(op_put_by_id_transition)) {
        visitor.append(&vPC[4].u.structure);
        visitor.append(&vPC[5].u.structure);
        if (vPC[6].u.structureChain)
            visitor.append(&vPC[6].u.structureChain);
        return;
    }
    if (vPC[0].u.opcode == interpreter->getOpcode(op_put_by_id) && vPC[4].u.structure) {
        visitor.append(&vPC[4].u.structure);
        return;
    }
    if (vPC[0].u.opcode == interpreter->getOpcode(op_put_by_id_replace)) {
        visitor.append(&vPC[4].u.structure);
        return;
    }
    if (vPC[0].u.opcode == interpreter->getOpcode(op_resolve_global) || vPC[0].u.opcode == interpreter->getOpcode(op_resolve_global_dynamic)) {
        if (vPC[3].u.structure)
            visitor.append(&vPC[3].u.structure);
        return;
    }

    // These instructions don't ref their Structures.
    ASSERT(vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id) || vPC[0].u.opcode == interpreter->getOpcode(op_put_by_id) || vPC[0].u.opcode == interpreter->getOpcode(op_get_by_id_generic) || vPC[0].u.opcode == interpreter->getOpcode(op_put_by_id_generic) || vPC[0].u.opcode == interpreter->getOpcode(op_get_array_length) || vPC[0].u.opcode == interpreter->getOpcode(op_get_string_length));
}

void EvalCodeCache::visitAggregate(SlotVisitor& visitor)
{
    EvalCacheMap::iterator end = m_cacheMap.end();
    for (EvalCacheMap::iterator ptr = m_cacheMap.begin(); ptr != end; ++ptr)
        visitor.append(&ptr->second);
}

void CodeBlock::visitAggregate(SlotVisitor& visitor)
{
#if ENABLE(PARALLEL_GC) && ENABLE(DFG_JIT)
    if (!!m_dfgData) {
        // I may be asked to scan myself more than once, and it may even happen concurrently.
        // To this end, use a CAS loop to check if I've been called already. Only one thread
        // may proceed past this point - whichever one wins the CAS race.
        unsigned oldValue;
        do {
            oldValue = m_dfgData->visitAggregateHasBeenCalled;
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
        } while (!WTF::weakCompareAndSwap(&m_dfgData->visitAggregateHasBeenCalled, 0, 1));
    }
#endif // ENABLE(PARALLEL_GC) && ENABLE(DFG_JIT)
    
    if (!!m_alternative)
        m_alternative->visitAggregate(visitor);

    // There are three things that may use unconditional finalizers: lazy bytecode freeing,
    // inline cache clearing, and jettisoning. The probability of us wanting to do at
    // least one of those things is probably quite close to 1. So we add one no matter what
    // and when it runs, it figures out whether it has any work to do.
    visitor.addUnconditionalFinalizer(this);
    
    if (shouldImmediatelyAssumeLivenessDuringScan()) {
        // This code block is live, so scan all references strongly and return.
        stronglyVisitStrongReferences(visitor);
        stronglyVisitWeakReferences(visitor);
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
    
    m_dfgData->livenessHasBeenProved = false;
    m_dfgData->allTransitionsHaveBeenMarked = false;
    
    performTracingFixpointIteration(visitor);

    // GC doesn't have enough information yet for us to decide whether to keep our DFG
    // data, so we need to register a handler to run again at the end of GC, when more
    // information is available.
    if (!(m_dfgData->livenessHasBeenProved && m_dfgData->allTransitionsHaveBeenMarked))
        visitor.addWeakReferenceHarvester(this);
    
#else // ENABLE(DFG_JIT)
    ASSERT_NOT_REACHED();
#endif // ENABLE(DFG_JIT)
}

void CodeBlock::performTracingFixpointIteration(SlotVisitor& visitor)
{
    UNUSED_PARAM(visitor);
    
#if ENABLE(DFG_JIT)
    // Evaluate our weak reference transitions, if there are still some to evaluate.
    if (!m_dfgData->allTransitionsHaveBeenMarked) {
        bool allAreMarkedSoFar = true;
        for (unsigned i = 0; i < m_dfgData->transitions.size(); ++i) {
            if ((!m_dfgData->transitions[i].m_codeOrigin
                 || Heap::isMarked(m_dfgData->transitions[i].m_codeOrigin.get()))
                && Heap::isMarked(m_dfgData->transitions[i].m_from.get())) {
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
                visitor.append(&m_dfgData->transitions[i].m_to);
            } else
                allAreMarkedSoFar = false;
        }
        
        if (allAreMarkedSoFar)
            m_dfgData->allTransitionsHaveBeenMarked = true;
    }
    
    // Check if we have any remaining work to do.
    if (m_dfgData->livenessHasBeenProved)
        return;
    
    // Now check all of our weak references. If all of them are live, then we
    // have proved liveness and so we scan our strong references. If at end of
    // GC we still have not proved liveness, then this code block is toast.
    bool allAreLiveSoFar = true;
    for (unsigned i = 0; i < m_dfgData->weakReferences.size(); ++i) {
        if (!Heap::isMarked(m_dfgData->weakReferences[i].get())) {
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
    m_dfgData->livenessHasBeenProved = true;
    stronglyVisitStrongReferences(visitor);
#endif // ENABLE(DFG_JIT)
}

void CodeBlock::visitWeakReferences(SlotVisitor& visitor)
{
    performTracingFixpointIteration(visitor);
}

#if ENABLE(JIT)
#if ENABLE(JIT_VERBOSE_OSR)
static const bool verboseUnlinking = true;
#else
static const bool verboseUnlinking = false;
#endif
#endif // ENABLE(JIT)
    
void CodeBlock::finalizeUnconditionally()
{
#if ENABLE(LLINT)
    Interpreter* interpreter = m_globalData->interpreter;
    // interpreter->classicEnabled() returns true if the old C++ interpreter is enabled. If that's enabled
    // then we're not using LLInt.
    if (!interpreter->classicEnabled() && !!numberOfInstructions()) {
        for (size_t size = m_propertyAccessInstructions.size(), i = 0; i < size; ++i) {
            Instruction* curInstruction = &instructions()[m_propertyAccessInstructions[i]];
            switch (interpreter->getOpcodeID(curInstruction[0].u.opcode)) {
            case op_get_by_id:
            case op_get_by_id_out_of_line:
            case op_put_by_id:
            case op_put_by_id_out_of_line:
                if (!curInstruction[4].u.structure || Heap::isMarked(curInstruction[4].u.structure.get()))
                    break;
                if (verboseUnlinking)
                    dataLog("Clearing LLInt property access with structure %p.\n", curInstruction[4].u.structure.get());
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
                if (verboseUnlinking) {
                    dataLog("Clearing LLInt put transition with structures %p -> %p, chain %p.\n",
                            curInstruction[4].u.structure.get(),
                            curInstruction[6].u.structure.get(),
                            curInstruction[7].u.structureChain.get());
                }
                curInstruction[4].u.structure.clear();
                curInstruction[6].u.structure.clear();
                curInstruction[7].u.structureChain.clear();
                curInstruction[0].u.opcode = interpreter->getOpcode(op_put_by_id);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }
        for (size_t size = m_globalResolveInstructions.size(), i = 0; i < size; ++i) {
            Instruction* curInstruction = &instructions()[m_globalResolveInstructions[i]];
            ASSERT(interpreter->getOpcodeID(curInstruction[0].u.opcode) == op_resolve_global
                   || interpreter->getOpcodeID(curInstruction[0].u.opcode) == op_resolve_global_dynamic);
            if (!curInstruction[3].u.structure || Heap::isMarked(curInstruction[3].u.structure.get()))
                continue;
            if (verboseUnlinking)
                dataLog("Clearing LLInt global resolve cache with structure %p.\n", curInstruction[3].u.structure.get());
            curInstruction[3].u.structure.clear();
            curInstruction[4].u.operand = 0;
        }
        for (unsigned i = 0; i < m_llintCallLinkInfos.size(); ++i) {
            if (m_llintCallLinkInfos[i].isLinked() && !Heap::isMarked(m_llintCallLinkInfos[i].callee.get())) {
                if (verboseUnlinking)
                    dataLog("Clearing LLInt call from %p.\n", this);
                m_llintCallLinkInfos[i].unlink();
            }
            if (!!m_llintCallLinkInfos[i].lastSeenCallee && !Heap::isMarked(m_llintCallLinkInfos[i].lastSeenCallee.get()))
                m_llintCallLinkInfos[i].lastSeenCallee.clear();
        }
    }
#endif // ENABLE(LLINT)

#if ENABLE(DFG_JIT)
    // Check if we're not live. If we are, then jettison.
    if (!(shouldImmediatelyAssumeLivenessDuringScan() || m_dfgData->livenessHasBeenProved)) {
        if (verboseUnlinking)
            dataLog("Code block %p has dead weak references, jettisoning during GC.\n", this);

        // Make sure that the baseline JIT knows that it should re-warm-up before
        // optimizing.
        alternative()->optimizeAfterWarmUp();
        
        jettison();
        return;
    }
#endif // ENABLE(DFG_JIT)
    
#if ENABLE(JIT)
    // Handle inline caches.
    if (!!getJITCode()) {
        RepatchBuffer repatchBuffer(this);
        for (unsigned i = 0; i < numberOfCallLinkInfos(); ++i) {
            if (callLinkInfo(i).isLinked() && !Heap::isMarked(callLinkInfo(i).callee.get())) {
                if (verboseUnlinking)
                    dataLog("Clearing call from %p to %p.\n", this, callLinkInfo(i).callee.get());
                callLinkInfo(i).unlink(*m_globalData, repatchBuffer);
            }
            if (!!callLinkInfo(i).lastSeenCallee
                && !Heap::isMarked(callLinkInfo(i).lastSeenCallee.get()))
                callLinkInfo(i).lastSeenCallee.clear();
        }
        for (size_t size = m_globalResolveInfos.size(), i = 0; i < size; ++i) {
            if (m_globalResolveInfos[i].structure && !Heap::isMarked(m_globalResolveInfos[i].structure.get())) {
                if (verboseUnlinking)
                    dataLog("Clearing resolve info in %p.\n", this);
                m_globalResolveInfos[i].structure.clear();
            }
        }

        for (size_t size = m_structureStubInfos.size(), i = 0; i < size; ++i) {
            StructureStubInfo& stubInfo = m_structureStubInfos[i];
            
            if (stubInfo.visitWeakReferences())
                continue;
            
            resetStubInternal(repatchBuffer, stubInfo);
        }

        for (size_t size = m_methodCallLinkInfos.size(), i = 0; i < size; ++i) {
            if (!m_methodCallLinkInfos[i].cachedStructure)
                continue;
            
            ASSERT(m_methodCallLinkInfos[i].seenOnce());
            ASSERT(!!m_methodCallLinkInfos[i].cachedPrototypeStructure);

            if (!Heap::isMarked(m_methodCallLinkInfos[i].cachedStructure.get())
                || !Heap::isMarked(m_methodCallLinkInfos[i].cachedPrototypeStructure.get())
                || !Heap::isMarked(m_methodCallLinkInfos[i].cachedFunction.get())
                || !Heap::isMarked(m_methodCallLinkInfos[i].cachedPrototype.get())) {
                if (verboseUnlinking)
                    dataLog("Clearing method call in %p.\n", this);
                m_methodCallLinkInfos[i].reset(repatchBuffer, getJITType());

                StructureStubInfo& stubInfo = getStubInfo(m_methodCallLinkInfos[i].bytecodeIndex);

                AccessType accessType = static_cast<AccessType>(stubInfo.accessType);

                if (accessType != access_unset) {
                    ASSERT(isGetByIdAccess(accessType));
                    if (getJITCode().jitType() == JITCode::DFGJIT)
                        DFG::dfgResetGetByID(repatchBuffer, stubInfo);
                    else
                        JIT::resetPatchGetById(repatchBuffer, &stubInfo);
                    stubInfo.reset();
                }
            }
        }
    }
#endif
}

#if ENABLE(JIT)
void CodeBlock::resetStub(StructureStubInfo& stubInfo)
{
    if (stubInfo.accessType == access_unset)
        return;
    
    RepatchBuffer repatchBuffer(this);
    resetStubInternal(repatchBuffer, stubInfo);
}

void CodeBlock::resetStubInternal(RepatchBuffer& repatchBuffer, StructureStubInfo& stubInfo)
{
    AccessType accessType = static_cast<AccessType>(stubInfo.accessType);
    
    if (verboseUnlinking)
        dataLog("Clearing structure cache (kind %d) in %p.\n", stubInfo.accessType, this);
    
    if (isGetByIdAccess(accessType)) {
        if (getJITCode().jitType() == JITCode::DFGJIT)
            DFG::dfgResetGetByID(repatchBuffer, stubInfo);
        else
            JIT::resetPatchGetById(repatchBuffer, &stubInfo);
    } else {
        ASSERT(isPutByIdAccess(accessType));
        if (getJITCode().jitType() == JITCode::DFGJIT)
            DFG::dfgResetPutByID(repatchBuffer, stubInfo);
        else 
            JIT::resetPatchPutById(repatchBuffer, &stubInfo);
    }
    
    stubInfo.reset();
}
#endif

void CodeBlock::stronglyVisitStrongReferences(SlotVisitor& visitor)
{
    visitor.append(&m_globalObject);
    visitor.append(&m_ownerExecutable);
    if (m_rareData) {
        m_rareData->m_evalCodeCache.visitAggregate(visitor);
        size_t regExpCount = m_rareData->m_regexps.size();
        WriteBarrier<RegExp>* regexps = m_rareData->m_regexps.data();
        for (size_t i = 0; i < regExpCount; i++)
            visitor.append(regexps + i);
    }
    visitor.appendValues(m_constantRegisters.data(), m_constantRegisters.size());
    for (size_t i = 0; i < m_functionExprs.size(); ++i)
        visitor.append(&m_functionExprs[i]);
    for (size_t i = 0; i < m_functionDecls.size(); ++i)
        visitor.append(&m_functionDecls[i]);
#if ENABLE(CLASSIC_INTERPRETER)
    if (m_globalData->interpreter->classicEnabled() && !!numberOfInstructions()) {
        for (size_t size = m_propertyAccessInstructions.size(), i = 0; i < size; ++i)
            visitStructures(visitor, &instructions()[m_propertyAccessInstructions[i]]);
        for (size_t size = m_globalResolveInstructions.size(), i = 0; i < size; ++i)
            visitStructures(visitor, &instructions()[m_globalResolveInstructions[i]]);
    }
#endif

    updateAllPredictions(Collection);
}

void CodeBlock::stronglyVisitWeakReferences(SlotVisitor& visitor)
{
    UNUSED_PARAM(visitor);

#if ENABLE(DFG_JIT)
    if (!m_dfgData)
        return;

    for (unsigned i = 0; i < m_dfgData->transitions.size(); ++i) {
        if (!!m_dfgData->transitions[i].m_codeOrigin)
            visitor.append(&m_dfgData->transitions[i].m_codeOrigin); // Almost certainly not necessary, since the code origin should also be a weak reference. Better to be safe, though.
        visitor.append(&m_dfgData->transitions[i].m_from);
        visitor.append(&m_dfgData->transitions[i].m_to);
    }
    
    for (unsigned i = 0; i < m_dfgData->weakReferences.size(); ++i)
        visitor.append(&m_dfgData->weakReferences[i]);
#endif    
}

#if ENABLE(BYTECODE_COMMENTS)
// Finds the comment string for the specified bytecode offset/PC is available. 
const char* CodeBlock::commentForBytecodeOffset(unsigned bytecodeOffset)
{
    ASSERT(bytecodeOffset < instructions().size());

    Vector<Comment>& comments = m_bytecodeComments;
    size_t numberOfComments = comments.size();
    const char* result = 0;

    if (!numberOfComments)
        return 0; // No comments to match with.

    // The next match is most likely the next comment in the list.
    // Do a quick check to see if that is a match first.
    // m_bytecodeCommentIterator should already be pointing to the
    // next comment we should check.

    ASSERT(m_bytecodeCommentIterator < comments.size());

    size_t i = m_bytecodeCommentIterator;
    size_t commentPC = comments[i].pc;
    if (commentPC == bytecodeOffset) {
        // We've got a match. All done!
        m_bytecodeCommentIterator = i;
        result = comments[i].string;
    } else if (commentPC > bytecodeOffset) {
        // The current comment is already greater than the requested PC.
        // Start searching from the first comment.
        i = 0;
    } else {
        // Otherwise, the current comment's PC is less than the requested PC.
        // Hence, we can just start searching from the next comment in the
        // list.
        i++;
    }

    // If the result is still not found, do a linear search in the range
    // that we've determined above.
    if (!result) {
        for (; i < comments.size(); ++i) {
            commentPC = comments[i].pc;
            if (commentPC == bytecodeOffset) {
                result = comments[i].string;
                break;
            }
            if (comments[i].pc > bytecodeOffset) {
                // The current comment PC is already past the requested
                // bytecodeOffset. Hence, there are no more possible
                // matches. Just fail.
                break;
            }
        }
    }

    // Update the iterator to point to the next comment.
    if (++i >= numberOfComments) {
        // At most point to the last comment entry. This ensures that the
        // next time we call this function, the quick checks will at least
        // have one entry to check and can fail fast if appropriate.
        i = numberOfComments - 1;
    }
    m_bytecodeCommentIterator = i;
    return result;
}

void CodeBlock::dumpBytecodeComments()
{
    Vector<Comment>& comments = m_bytecodeComments;
    printf("Comments for codeblock %p: size %lu\n", this, comments.size());
    for (size_t i = 0; i < comments.size(); ++i)
        printf("     pc %lu : '%s'\n", comments[i].pc, comments[i].string);
    printf("End of comments for codeblock %p\n", this);
}
#endif // ENABLE_BYTECODE_COMMENTS

HandlerInfo* CodeBlock::handlerForBytecodeOffset(unsigned bytecodeOffset)
{
    ASSERT(bytecodeOffset < instructions().size());

    if (!m_rareData)
        return 0;
    
    Vector<HandlerInfo>& exceptionHandlers = m_rareData->m_exceptionHandlers;
    for (size_t i = 0; i < exceptionHandlers.size(); ++i) {
        // Handlers are ordered innermost first, so the first handler we encounter
        // that contains the source address is the correct handler to use.
        if (exceptionHandlers[i].start <= bytecodeOffset && exceptionHandlers[i].end >= bytecodeOffset)
            return &exceptionHandlers[i];
    }

    return 0;
}

int CodeBlock::lineNumberForBytecodeOffset(unsigned bytecodeOffset)
{
    ASSERT(bytecodeOffset < instructions().size());

    Vector<LineInfo>& lineInfo = m_lineInfo;

    int low = 0;
    int high = lineInfo.size();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (lineInfo[mid].instructionOffset <= bytecodeOffset)
            low = mid + 1;
        else
            high = mid;
    }

    if (!low)
        return m_ownerExecutable->source().firstLine();
    return lineInfo[low - 1].lineNumber;
}

void CodeBlock::expressionRangeForBytecodeOffset(unsigned bytecodeOffset, int& divot, int& startOffset, int& endOffset)
{
    ASSERT(bytecodeOffset < instructions().size());

    if (!m_rareData) {
        startOffset = 0;
        endOffset = 0;
        divot = 0;
        return;
    }

    Vector<ExpressionRangeInfo>& expressionInfo = m_rareData->m_expressionInfo;

    int low = 0;
    int high = expressionInfo.size();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (expressionInfo[mid].instructionOffset <= bytecodeOffset)
            low = mid + 1;
        else
            high = mid;
    }

    ASSERT(low);
    if (!low) {
        startOffset = 0;
        endOffset = 0;
        divot = 0;
        return;
    }

    startOffset = expressionInfo[low - 1].startOffset;
    endOffset = expressionInfo[low - 1].endOffset;
    divot = expressionInfo[low - 1].divotPoint + m_sourceOffset;
    return;
}

#if ENABLE(CLASSIC_INTERPRETER)
bool CodeBlock::hasGlobalResolveInstructionAtBytecodeOffset(unsigned bytecodeOffset)
{
    if (m_globalResolveInstructions.isEmpty())
        return false;

    int low = 0;
    int high = m_globalResolveInstructions.size();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (m_globalResolveInstructions[mid] <= bytecodeOffset)
            low = mid + 1;
        else
            high = mid;
    }

    if (!low || m_globalResolveInstructions[low - 1] != bytecodeOffset)
        return false;
    return true;
}
#endif
#if ENABLE(JIT)
bool CodeBlock::hasGlobalResolveInfoAtBytecodeOffset(unsigned bytecodeOffset)
{
    if (m_globalResolveInfos.isEmpty())
        return false;

    int low = 0;
    int high = m_globalResolveInfos.size();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (m_globalResolveInfos[mid].bytecodeOffset <= bytecodeOffset)
            low = mid + 1;
        else
            high = mid;
    }

    if (!low || m_globalResolveInfos[low - 1].bytecodeOffset != bytecodeOffset)
        return false;
    return true;
}
GlobalResolveInfo& CodeBlock::globalResolveInfoForBytecodeOffset(unsigned bytecodeOffset)
{
    return *(binarySearch<GlobalResolveInfo, unsigned, getGlobalResolveInfoBytecodeOffset>(m_globalResolveInfos.begin(), m_globalResolveInfos.size(), bytecodeOffset));
}
#endif

void CodeBlock::shrinkToFit(ShrinkMode shrinkMode)
{
    m_propertyAccessInstructions.shrinkToFit();
    m_globalResolveInstructions.shrinkToFit();
#if ENABLE(LLINT)
    m_llintCallLinkInfos.shrinkToFit();
#endif
#if ENABLE(JIT)
    m_structureStubInfos.shrinkToFit();
    if (shrinkMode == EarlyShrink)
        m_globalResolveInfos.shrinkToFit();
    m_callLinkInfos.shrinkToFit();
    m_methodCallLinkInfos.shrinkToFit();
#endif
#if ENABLE(VALUE_PROFILER)
    if (shrinkMode == EarlyShrink)
        m_argumentValueProfiles.shrinkToFit();
    m_valueProfiles.shrinkToFit();
    m_rareCaseProfiles.shrinkToFit();
    m_specialFastCaseProfiles.shrinkToFit();
#endif
    
    if (shrinkMode == EarlyShrink) {
        m_identifiers.shrinkToFit();
        m_functionDecls.shrinkToFit();
        m_functionExprs.shrinkToFit();
        m_constantRegisters.shrinkToFit();
    } // else don't shrink these, because we would have already pointed pointers into these tables.

    m_lineInfo.shrinkToFit();
    if (m_rareData) {
        m_rareData->m_exceptionHandlers.shrinkToFit();
        m_rareData->m_regexps.shrinkToFit();
        m_rareData->m_immediateSwitchJumpTables.shrinkToFit();
        m_rareData->m_characterSwitchJumpTables.shrinkToFit();
        m_rareData->m_stringSwitchJumpTables.shrinkToFit();
        m_rareData->m_expressionInfo.shrinkToFit();
#if ENABLE(JIT)
        m_rareData->m_callReturnIndexVector.shrinkToFit();
#endif
#if ENABLE(DFG_JIT)
        m_rareData->m_inlineCallFrames.shrinkToFit();
        m_rareData->m_codeOrigins.shrinkToFit();
#endif
    }
    
#if ENABLE(DFG_JIT)
    if (m_dfgData) {
        m_dfgData->osrEntry.shrinkToFit();
        m_dfgData->osrExit.shrinkToFit();
        m_dfgData->speculationRecovery.shrinkToFit();
        m_dfgData->weakReferences.shrinkToFit();
        m_dfgData->transitions.shrinkToFit();
        m_dfgData->minifiedDFG.prepareAndShrink();
        m_dfgData->variableEventStream.shrinkToFit();
    }
#endif
}

void CodeBlock::createActivation(CallFrame* callFrame)
{
    ASSERT(codeType() == FunctionCode);
    ASSERT(needsFullScopeChain());
    ASSERT(!callFrame->uncheckedR(activationRegister()).jsValue());
    JSActivation* activation = JSActivation::create(callFrame->globalData(), callFrame, static_cast<FunctionExecutable*>(ownerExecutable()));
    callFrame->uncheckedR(activationRegister()) = JSValue(activation);
    callFrame->setScopeChain(callFrame->scopeChain()->push(activation));
}

unsigned CodeBlock::addOrFindConstant(JSValue v)
{
    unsigned numberOfConstants = numberOfConstantRegisters();
    for (unsigned i = 0; i < numberOfConstants; ++i) {
        if (getConstant(FirstConstantRegisterIndex + i) == v)
            return i;
    }
    return addConstant(v);
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
    if (!(m_callLinkInfos.size() || m_methodCallLinkInfos.size()))
        return;
    if (!m_globalData->canUseJIT())
        return;
    RepatchBuffer repatchBuffer(this);
    for (size_t i = 0; i < m_callLinkInfos.size(); i++) {
        if (!m_callLinkInfos[i].isLinked())
            continue;
        m_callLinkInfos[i].unlink(*m_globalData, repatchBuffer);
    }
}

void CodeBlock::unlinkIncomingCalls()
{
#if ENABLE(LLINT)
    while (m_incomingLLIntCalls.begin() != m_incomingLLIntCalls.end())
        m_incomingLLIntCalls.begin()->unlink();
#endif
    if (m_incomingCalls.isEmpty())
        return;
    RepatchBuffer repatchBuffer(this);
    while (m_incomingCalls.begin() != m_incomingCalls.end())
        m_incomingCalls.begin()->unlink(*m_globalData, repatchBuffer);
}

unsigned CodeBlock::bytecodeOffset(ExecState* exec, ReturnAddressPtr returnAddress)
{
#if ENABLE(LLINT)
    if (returnAddress.value() >= bitwise_cast<void*>(&llint_begin)
        && returnAddress.value() <= bitwise_cast<void*>(&llint_end)) {
        ASSERT(exec->codeBlock());
        ASSERT(exec->codeBlock() == this);
        ASSERT(JITCode::isBaselineCode(getJITType()));
        Instruction* instruction = exec->currentVPC();
        ASSERT(instruction);
        
        // The LLInt stores the PC after the call instruction rather than the PC of
        // the call instruction. This requires some correcting. We rely on the fact
        // that the preceding instruction must be one of the call instructions, so
        // either it's a call_varargs or it's a call, construct, or eval.
        ASSERT(OPCODE_LENGTH(op_call_varargs) <= OPCODE_LENGTH(op_call));
        ASSERT(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_construct));
        ASSERT(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_call_eval));
        if (instruction[-OPCODE_LENGTH(op_call_varargs)].u.pointer == bitwise_cast<void*>(llint_op_call_varargs)) {
            // We know that the preceding instruction must be op_call_varargs because there is no way that
            // the pointer to the call_varargs could be an operand to the call.
            instruction -= OPCODE_LENGTH(op_call_varargs);
            ASSERT(instruction[-OPCODE_LENGTH(op_call)].u.pointer != bitwise_cast<void*>(llint_op_call)
                   && instruction[-OPCODE_LENGTH(op_call)].u.pointer != bitwise_cast<void*>(llint_op_construct)
                   && instruction[-OPCODE_LENGTH(op_call)].u.pointer != bitwise_cast<void*>(llint_op_call_eval));
        } else {
            // Must be that the last instruction was some op_call.
            ASSERT(instruction[-OPCODE_LENGTH(op_call)].u.pointer == bitwise_cast<void*>(llint_op_call)
                   || instruction[-OPCODE_LENGTH(op_call)].u.pointer == bitwise_cast<void*>(llint_op_construct)
                   || instruction[-OPCODE_LENGTH(op_call)].u.pointer == bitwise_cast<void*>(llint_op_call_eval));
            instruction -= OPCODE_LENGTH(op_call);
        }
        
        return bytecodeOffset(instruction);
    }
#else
    UNUSED_PARAM(exec);
#endif
    if (!m_rareData)
        return 1;
    Vector<CallReturnOffsetToBytecodeOffset>& callIndices = m_rareData->m_callReturnIndexVector;
    if (!callIndices.size())
        return 1;
    return binarySearch<CallReturnOffsetToBytecodeOffset, unsigned, getCallReturnOffset>(callIndices.begin(), callIndices.size(), getJITCode().offsetOf(returnAddress.value()))->bytecodeOffset;
}
#endif

void CodeBlock::clearEvalCache()
{
    if (!!m_alternative)
        m_alternative->clearEvalCache();
    if (!m_rareData)
        return;
    m_rareData->m_evalCodeCache.clear();
}

template<typename T>
inline void replaceExistingEntries(Vector<T>& target, Vector<T>& source)
{
    ASSERT(target.size() <= source.size());
    for (size_t i = 0; i < target.size(); ++i)
        target[i] = source[i];
}

void CodeBlock::copyPostParseDataFrom(CodeBlock* alternative)
{
    if (!alternative)
        return;
    
    replaceExistingEntries(m_constantRegisters, alternative->m_constantRegisters);
    replaceExistingEntries(m_functionDecls, alternative->m_functionDecls);
    replaceExistingEntries(m_functionExprs, alternative->m_functionExprs);
    if (!!m_rareData && !!alternative->m_rareData)
        replaceExistingEntries(m_rareData->m_constantBuffers, alternative->m_rareData->m_constantBuffers);
}

void CodeBlock::copyPostParseDataFromAlternative()
{
    copyPostParseDataFrom(m_alternative.get());
}

#if ENABLE(JIT)
void CodeBlock::reoptimize()
{
    ASSERT(replacement() != this);
    ASSERT(replacement()->alternative() == this);
    replacement()->tallyFrequentExitSites();
    replacement()->jettison();
    countReoptimization();
    optimizeAfterWarmUp();
}

CodeBlock* ProgramCodeBlock::replacement()
{
    return &static_cast<ProgramExecutable*>(ownerExecutable())->generatedBytecode();
}

CodeBlock* EvalCodeBlock::replacement()
{
    return &static_cast<EvalExecutable*>(ownerExecutable())->generatedBytecode();
}

CodeBlock* FunctionCodeBlock::replacement()
{
    return &static_cast<FunctionExecutable*>(ownerExecutable())->generatedBytecodeFor(m_isConstructor ? CodeForConstruct : CodeForCall);
}

JSObject* ProgramCodeBlock::compileOptimized(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    if (replacement()->getJITType() == JITCode::nextTierJIT(getJITType()))
        return 0;
    JSObject* error = static_cast<ProgramExecutable*>(ownerExecutable())->compileOptimized(exec, scopeChainNode);
    return error;
}

JSObject* EvalCodeBlock::compileOptimized(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    if (replacement()->getJITType() == JITCode::nextTierJIT(getJITType()))
        return 0;
    JSObject* error = static_cast<EvalExecutable*>(ownerExecutable())->compileOptimized(exec, scopeChainNode);
    return error;
}

JSObject* FunctionCodeBlock::compileOptimized(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    if (replacement()->getJITType() == JITCode::nextTierJIT(getJITType()))
        return 0;
    JSObject* error = static_cast<FunctionExecutable*>(ownerExecutable())->compileOptimizedFor(exec, scopeChainNode, m_isConstructor ? CodeForConstruct : CodeForCall);
    return error;
}

DFG::CapabilityLevel ProgramCodeBlock::canCompileWithDFGInternal()
{
    return DFG::canCompileProgram(this);
}

DFG::CapabilityLevel EvalCodeBlock::canCompileWithDFGInternal()
{
    return DFG::canCompileEval(this);
}

DFG::CapabilityLevel FunctionCodeBlock::canCompileWithDFGInternal()
{
    if (m_isConstructor)
        return DFG::canCompileFunctionForConstruct(this);
    return DFG::canCompileFunctionForCall(this);
}

void ProgramCodeBlock::jettison()
{
    ASSERT(JITCode::isOptimizingJIT(getJITType()));
    ASSERT(this == replacement());
    static_cast<ProgramExecutable*>(ownerExecutable())->jettisonOptimizedCode(*globalData());
}

void EvalCodeBlock::jettison()
{
    ASSERT(JITCode::isOptimizingJIT(getJITType()));
    ASSERT(this == replacement());
    static_cast<EvalExecutable*>(ownerExecutable())->jettisonOptimizedCode(*globalData());
}

void FunctionCodeBlock::jettison()
{
    ASSERT(JITCode::isOptimizingJIT(getJITType()));
    ASSERT(this == replacement());
    static_cast<FunctionExecutable*>(ownerExecutable())->jettisonOptimizedCodeFor(*globalData(), m_isConstructor ? CodeForConstruct : CodeForCall);
}

bool ProgramCodeBlock::jitCompileImpl(ExecState* exec)
{
    ASSERT(getJITType() == JITCode::InterpreterThunk);
    ASSERT(this == replacement());
    return static_cast<ProgramExecutable*>(ownerExecutable())->jitCompile(exec);
}

bool EvalCodeBlock::jitCompileImpl(ExecState* exec)
{
    ASSERT(getJITType() == JITCode::InterpreterThunk);
    ASSERT(this == replacement());
    return static_cast<EvalExecutable*>(ownerExecutable())->jitCompile(exec);
}

bool FunctionCodeBlock::jitCompileImpl(ExecState* exec)
{
    ASSERT(getJITType() == JITCode::InterpreterThunk);
    ASSERT(this == replacement());
    return static_cast<FunctionExecutable*>(ownerExecutable())->jitCompileFor(exec, m_isConstructor ? CodeForConstruct : CodeForCall);
}
#endif

#if ENABLE(VALUE_PROFILER)
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

void CodeBlock::updateAllPredictionsAndCountLiveness(
    OperationInProgress operation, unsigned& numberOfLiveNonArgumentValueProfiles, unsigned& numberOfSamplesInProfiles)
{
    numberOfLiveNonArgumentValueProfiles = 0;
    numberOfSamplesInProfiles = 0; // If this divided by ValueProfile::numberOfBuckets equals numberOfValueProfiles() then value profiles are full.
    for (unsigned i = 0; i < totalNumberOfValueProfiles(); ++i) {
        ValueProfile* profile = getFromAllValueProfiles(i);
        unsigned numSamples = profile->totalNumberOfSamples();
        if (numSamples > ValueProfile::numberOfBuckets)
            numSamples = ValueProfile::numberOfBuckets; // We don't want profiles that are extremely hot to be given more weight.
        numberOfSamplesInProfiles += numSamples;
        if (profile->m_bytecodeOffset < 0) {
            profile->computeUpdatedPrediction(operation);
            continue;
        }
        if (profile->numberOfSamples() || profile->m_prediction != SpecNone)
            numberOfLiveNonArgumentValueProfiles++;
        profile->computeUpdatedPrediction(operation);
    }
    
#if ENABLE(DFG_JIT)
    m_lazyOperandValueProfiles.computeUpdatedPredictions(operation);
#endif
    
    // Don't count the array profiles towards statistics, since each array profile
    // site also has a value profile site - so we already know whether or not it's
    // live.
    for (unsigned i = m_arrayProfiles.size(); i--;)
        m_arrayProfiles[i].computeUpdatedPrediction(operation);
}

void CodeBlock::updateAllPredictions(OperationInProgress operation)
{
    unsigned ignoredValue1, ignoredValue2;
    updateAllPredictionsAndCountLiveness(operation, ignoredValue1, ignoredValue2);
}

bool CodeBlock::shouldOptimizeNow()
{
#if ENABLE(JIT_VERBOSE_OSR)
    dataLog("Considering optimizing %p...\n", this);
#endif

#if ENABLE(VERBOSE_VALUE_PROFILE)
    dumpValueProfiles();
#endif

    if (m_optimizationDelayCounter >= Options::maximumOptimizationDelay())
        return true;
    
    unsigned numberOfLiveNonArgumentValueProfiles;
    unsigned numberOfSamplesInProfiles;
    updateAllPredictionsAndCountLiveness(NoOperation, numberOfLiveNonArgumentValueProfiles, numberOfSamplesInProfiles);

#if ENABLE(JIT_VERBOSE_OSR)
    dataLog("Profile hotness: %lf, %lf\n", (double)numberOfLiveNonArgumentValueProfiles / numberOfValueProfiles(), (double)numberOfSamplesInProfiles / ValueProfile::numberOfBuckets / numberOfValueProfiles());
#endif

    if ((!numberOfValueProfiles() || (double)numberOfLiveNonArgumentValueProfiles / numberOfValueProfiles() >= Options::desiredProfileLivenessRate())
        && (!totalNumberOfValueProfiles() || (double)numberOfSamplesInProfiles / ValueProfile::numberOfBuckets / totalNumberOfValueProfiles() >= Options::desiredProfileFullnessRate())
        && static_cast<unsigned>(m_optimizationDelayCounter) + 1 >= Options::minimumOptimizationDelay())
        return true;
    
    ASSERT(m_optimizationDelayCounter < std::numeric_limits<uint8_t>::max());
    m_optimizationDelayCounter++;
    optimizeAfterWarmUp();
    return false;
}
#endif

#if ENABLE(DFG_JIT)
void CodeBlock::tallyFrequentExitSites()
{
    ASSERT(getJITType() == JITCode::DFGJIT);
    ASSERT(alternative()->getJITType() == JITCode::BaselineJIT);
    ASSERT(!!m_dfgData);
    
    CodeBlock* profiledBlock = alternative();
    
    for (unsigned i = 0; i < m_dfgData->osrExit.size(); ++i) {
        DFG::OSRExit& exit = m_dfgData->osrExit[i];
        
        if (!exit.considerAddingAsFrequentExitSite(this, profiledBlock))
            continue;
        
#if DFG_ENABLE(DEBUG_VERBOSE)
        dataLog("OSR exit #%u (bc#%u, @%u, %s) for code block %p occurred frequently; counting as frequent exit site.\n", i, exit.m_codeOrigin.bytecodeIndex, exit.m_nodeIndex, DFG::exitKindToString(exit.m_kind), this);
#endif
    }
}
#endif // ENABLE(DFG_JIT)

#if ENABLE(VERBOSE_VALUE_PROFILE)
void CodeBlock::dumpValueProfiles()
{
    dataLog("ValueProfile for %p:\n", this);
    for (unsigned i = 0; i < totalNumberOfValueProfiles(); ++i) {
        ValueProfile* profile = getFromAllValueProfiles(i);
        if (profile->m_bytecodeOffset < 0) {
            ASSERT(profile->m_bytecodeOffset == -1);
            dataLog("   arg = %u: ", i);
        } else
            dataLog("   bc = %d: ", profile->m_bytecodeOffset);
        if (!profile->numberOfSamples() && profile->m_prediction == SpecNone) {
            dataLog("<empty>\n");
            continue;
        }
        profile->dump(WTF::dataFile());
        dataLog("\n");
    }
    dataLog("RareCaseProfile for %p:\n", this);
    for (unsigned i = 0; i < numberOfRareCaseProfiles(); ++i) {
        RareCaseProfile* profile = rareCaseProfile(i);
        dataLog("   bc = %d: %u\n", profile->m_bytecodeOffset, profile->m_counter);
    }
    dataLog("SpecialFastCaseProfile for %p:\n", this);
    for (unsigned i = 0; i < numberOfSpecialFastCaseProfiles(); ++i) {
        RareCaseProfile* profile = specialFastCaseProfile(i);
        dataLog("   bc = %d: %u\n", profile->m_bytecodeOffset, profile->m_counter);
    }
}
#endif // ENABLE(VERBOSE_VALUE_PROFILE)

size_t CodeBlock::predictedMachineCodeSize()
{
    // This will be called from CodeBlock::CodeBlock before either m_globalData or the
    // instructions have been initialized. It's OK to return 0 because what will really
    // matter is the recomputation of this value when the slow path is triggered.
    if (!m_globalData)
        return 0;
    
    if (!m_globalData->machineCodeBytesPerBytecodeWordForBaselineJIT)
        return 0; // It's as good of a prediction as we'll get.
    
    // Be conservative: return a size that will be an overestimation 84% of the time.
    double multiplier = m_globalData->machineCodeBytesPerBytecodeWordForBaselineJIT.mean() +
        m_globalData->machineCodeBytesPerBytecodeWordForBaselineJIT.standardDeviation();
    
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
    Interpreter* interpreter = globalData()->interpreter;
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
            ASSERT_NOT_REACHED();
            break;
        }
    }
    
    return false;
}

UString CodeBlock::nameForRegister(int registerNumber)
{
    SymbolTable::iterator end = m_symbolTable->end();
    for (SymbolTable::iterator ptr = m_symbolTable->begin(); ptr != end; ++ptr) {
        if (ptr->second.getIndex() == registerNumber)
            return UString(ptr->first);
    }
    if (needsActivation() && registerNumber == activationRegister())
        return "activation";
    if (registerNumber == thisRegister())
        return "this";
    if (usesArguments()) {
        if (registerNumber == argumentsRegister())
            return "arguments";
        if (unmodifiedArgumentsRegister(argumentsRegister()) == registerNumber)
            return "real arguments";
    }
    if (registerNumber < 0) {
        int argumentPosition = -registerNumber;
        argumentPosition -= RegisterFile::CallFrameHeaderSize + 1;
        return String::format("arguments[%3d]", argumentPosition - 1).impl();
    }
    return "";
}

} // namespace JSC
