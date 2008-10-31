/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "CTI.h"
#include "JSValue.h"
#include "Machine.h"
#include "Debugger.h"
#include <stdio.h>
#include <wtf/StringExtras.h>

namespace JSC {

#if !defined(NDEBUG) || ENABLE(OPCODE_SAMPLING)

static UString escapeQuotes(const UString& str)
{
    UString result = str;
    int pos = 0;
    while ((pos = result.find('\"', pos)) >= 0) {
        result = result.substr(0, pos) + "\"\\\"\"" + result.substr(pos + 1);
        pos += 4;
    }
    return result;
}

static UString valueToSourceString(ExecState* exec, JSValue* val)
{
    if (val->isString()) {
        UString result("\"");
        result += escapeQuotes(val->toString(exec)) + "\"";
        return result;
    } 

    return val->toString(exec);
}

static CString registerName(int r)
{
    if (r == missingThisObjectMarker())
        return "<null>";

    return (UString("r") + UString::from(r)).UTF8String();
}

static CString constantName(ExecState* exec, int k, JSValue* value)
{
    return (valueToSourceString(exec, value) + "(@k" + UString::from(k) + ")").UTF8String();
}

static CString idName(int id0, const Identifier& ident)
{
    return (ident.ustring() + "(@id" + UString::from(id0) +")").UTF8String();
}

static UString regexpToSourceString(RegExp* regExp)
{
    UString pattern = UString("/") + regExp->pattern() + "/";
    if (regExp->global())
        pattern += "g";
    if (regExp->ignoreCase())
        pattern += "i";
    if (regExp->multiline())
        pattern += "m";

    return pattern;
}

static CString regexpName(int re, RegExp* regexp)
{
    return (regexpToSourceString(regexp) + "(@re" + UString::from(re) + ")").UTF8String();
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

static int jumpTarget(const Vector<Instruction>::const_iterator& begin, Vector<Instruction>::const_iterator& it, int offset)
{
    return it - begin + offset;
}

static void printUnaryOp(int location, Vector<Instruction>::const_iterator& it, const char* op)
{
    int r0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;

    printf("[%4d] %s\t\t %s, %s\n", location, op, registerName(r0).c_str(), registerName(r1).c_str());
}

static void printBinaryOp(int location, Vector<Instruction>::const_iterator& it, const char* op)
{
    int r0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;
    int r2 = (++it)->u.operand;
    printf("[%4d] %s\t\t %s, %s, %s\n", location, op, registerName(r0).c_str(), registerName(r1).c_str(), registerName(r2).c_str());
}

static void printConditionalJump(const Vector<Instruction>::const_iterator& begin, Vector<Instruction>::const_iterator& it, int location, const char* op)
{
    int r0 = (++it)->u.operand;
    int offset = (++it)->u.operand;
    printf("[%4d] %s\t\t %s, %d(->%d)\n", location, op, registerName(r0).c_str(), offset, jumpTarget(begin, it, offset));
}

static void printGetByIdOp(int location, Vector<Instruction>::const_iterator& it, const Vector<Identifier>& identifiers, const char* op)
{
    int r0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;
    int id0 = (++it)->u.operand;
    printf("[%4d] %s\t %s, %s, %s\n", location, op, registerName(r0).c_str(), registerName(r1).c_str(), idName(id0, identifiers[id0]).c_str());
    it += 4;
}

static void printPutByIdOp(int location, Vector<Instruction>::const_iterator& it, const Vector<Identifier>& identifiers, const char* op)
{
    int r0 = (++it)->u.operand;
    int id0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;
    printf("[%4d] %s\t %s, %s, %s\n", location, op, registerName(r0).c_str(), idName(id0, identifiers[id0]).c_str(), registerName(r1).c_str());
    it += 4;
}

void CodeBlock::printStructureID(const char* name, const Instruction* vPC, int operand) const
{
    unsigned instructionOffset = vPC - instructions.begin();
    printf("  [%4d] %s: %s\n", instructionOffset, name, pointerToSourceString(vPC[operand].u.structureID).UTF8String().c_str());
}

void CodeBlock::printStructureIDs(const Instruction* vPC) const
{
    Machine* machine = globalData->machine;
    unsigned instructionOffset = vPC - instructions.begin();

    if (vPC[0].u.opcode == machine->getOpcode(op_get_by_id)) {
        printStructureID("get_by_id", vPC, 4);
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_get_by_id_self)) {
        printStructureID("get_by_id_self", vPC, 4);
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_get_by_id_proto)) {
        printf("  [%4d] %s: %s, %s\n", instructionOffset, "get_by_id_proto", pointerToSourceString(vPC[4].u.structureID).UTF8String().c_str(), pointerToSourceString(vPC[5].u.structureID).UTF8String().c_str());
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_put_by_id_transition)) {
        printf("  [%4d] %s: %s, %s, %s\n", instructionOffset, "put_by_id_new", pointerToSourceString(vPC[4].u.structureID).UTF8String().c_str(), pointerToSourceString(vPC[5].u.structureID).UTF8String().c_str(), pointerToSourceString(vPC[6].u.structureIDChain).UTF8String().c_str());
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_get_by_id_chain)) {
        printf("  [%4d] %s: %s, %s\n", instructionOffset, "get_by_id_chain", pointerToSourceString(vPC[4].u.structureID).UTF8String().c_str(), pointerToSourceString(vPC[5].u.structureIDChain).UTF8String().c_str());
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_put_by_id)) {
        printStructureID("put_by_id", vPC, 4);
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_put_by_id_replace)) {
        printStructureID("put_by_id_replace", vPC, 4);
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_resolve_global)) {
        printStructureID("resolve_global", vPC, 4);
        return;
    }

    // These instructions doesn't ref StructureIDs.
    ASSERT(vPC[0].u.opcode == machine->getOpcode(op_get_by_id_generic) || vPC[0].u.opcode == machine->getOpcode(op_put_by_id_generic) || vPC[0].u.opcode == machine->getOpcode(op_call) || vPC[0].u.opcode == machine->getOpcode(op_call_eval) || vPC[0].u.opcode == machine->getOpcode(op_construct));
}

void CodeBlock::dump(ExecState* exec) const
{
    Vector<Instruction>::const_iterator begin = instructions.begin();
    Vector<Instruction>::const_iterator end = instructions.end();

    size_t instructionCount = 0;
    for (Vector<Instruction>::const_iterator it = begin; it != end; ++it)
        if (exec->machine()->isOpcode(it->u.opcode))
            ++instructionCount;

    printf("%lu instructions; %lu bytes at %p; %d parameter(s); %d callee register(s)\n\n",
        static_cast<unsigned long>(instructionCount),
        static_cast<unsigned long>(instructions.size() * sizeof(Instruction)),
        this, numParameters, numCalleeRegisters);
    
    for (Vector<Instruction>::const_iterator it = begin; it != end; ++it)
        dump(exec, begin, it);

    if (identifiers.size()) {
        printf("\nIdentifiers:\n");
        size_t i = 0;
        do {
            printf("  id%u = %s\n", static_cast<unsigned>(i), identifiers[i].ascii());
            ++i;
        } while (i != identifiers.size());
    }

    if (constantRegisters.size()) {
        printf("\nConstants:\n");
        unsigned registerIndex = numVars;
        size_t i = 0;
        do {
            printf("   r%u = %s\n", registerIndex, valueToSourceString(exec, constantRegisters[i].jsValue(exec)).ascii());
            ++i;
            ++registerIndex;
        } while (i < constantRegisters.size());
    }

    if (unexpectedConstants.size()) {
        printf("\nUnexpected Constants:\n");
        size_t i = 0;
        do {
            printf("  k%u = %s\n", static_cast<unsigned>(i), valueToSourceString(exec, unexpectedConstants[i]).ascii());
            ++i;
        } while (i < unexpectedConstants.size());
    }
    
    if (regexps.size()) {
        printf("\nRegExps:\n");
        size_t i = 0;
        do {
            printf("  re%u = %s\n", static_cast<unsigned>(i), regexpToSourceString(regexps[i].get()).ascii());
            ++i;
        } while (i < regexps.size());
    }

    if (globalResolveInstructions.size() || propertyAccessInstructions.size())
        printf("\nStructureIDs:\n");

    if (globalResolveInstructions.size()) {
        size_t i = 0;
        do {
             printStructureIDs(&instructions[globalResolveInstructions[i]]);
             ++i;
        } while (i < globalResolveInstructions.size());
    }
    if (propertyAccessInstructions.size()) {
        size_t i = 0;
        do {
             printStructureIDs(&instructions[propertyAccessInstructions[i].opcodeIndex]);
             ++i;
        } while (i < propertyAccessInstructions.size());
    }
 
    if (exceptionHandlers.size()) {
        printf("\nException Handlers:\n");
        unsigned i = 0;
        do {
            printf("\t %d: { start: [%4d] end: [%4d] target: [%4d] }\n", i + 1, exceptionHandlers[i].start, exceptionHandlers[i].end, exceptionHandlers[i].target);
            ++i;
        } while (i < exceptionHandlers.size());
    }
    
    if (immediateSwitchJumpTables.size()) {
        printf("Immediate Switch Jump Tables:\n");
        unsigned i = 0;
        do {
            printf("  %1d = {\n", i);
            int entry = 0;
            Vector<int32_t>::const_iterator end = immediateSwitchJumpTables[i].branchOffsets.end();
            for (Vector<int32_t>::const_iterator iter = immediateSwitchJumpTables[i].branchOffsets.begin(); iter != end; ++iter, ++entry) {
                if (!*iter)
                    continue;
                printf("\t\t%4d => %04d\n", entry + immediateSwitchJumpTables[i].min, *iter);
            }
            printf("      }\n");
            ++i;
        } while (i < immediateSwitchJumpTables.size());
    }
    
    if (characterSwitchJumpTables.size()) {
        printf("\nCharacter Switch Jump Tables:\n");
        unsigned i = 0;
        do {
            printf("  %1d = {\n", i);
            int entry = 0;
            Vector<int32_t>::const_iterator end = characterSwitchJumpTables[i].branchOffsets.end();
            for (Vector<int32_t>::const_iterator iter = characterSwitchJumpTables[i].branchOffsets.begin(); iter != end; ++iter, ++entry) {
                if (!*iter)
                    continue;
                ASSERT(!((i + characterSwitchJumpTables[i].min) & ~0xFFFF));
                UChar ch = static_cast<UChar>(entry + characterSwitchJumpTables[i].min);
                printf("\t\t\"%s\" => %04d\n", UString(&ch, 1).ascii(), *iter);
        }
            printf("      }\n");
            ++i;
        } while (i < characterSwitchJumpTables.size());
    }
    
    if (stringSwitchJumpTables.size()) {
        printf("\nString Switch Jump Tables:\n");
        unsigned i = 0;
        do {
            printf("  %1d = {\n", i);
            StringJumpTable::StringOffsetTable::const_iterator end = stringSwitchJumpTables[i].offsetTable.end();
            for (StringJumpTable::StringOffsetTable::const_iterator iter = stringSwitchJumpTables[i].offsetTable.begin(); iter != end; ++iter)
                printf("\t\t\"%s\" => %04d\n", UString(iter->first).ascii(), iter->second.branchOffset);
            printf("      }\n");
            ++i;
        } while (i < stringSwitchJumpTables.size());
    }

    printf("\n");
}

void CodeBlock::dump(ExecState* exec, const Vector<Instruction>::const_iterator& begin, Vector<Instruction>::const_iterator& it) const
{
    int location = it - begin;
    switch (exec->machine()->getOpcodeID(it->u.opcode)) {
        case op_enter: {
            printf("[%4d] enter\n", location);
            break;
        }
        case op_enter_with_activation: {
            int r0 = (++it)->u.operand;
            printf("[%4d] enter_with_activation %s\n", location, registerName(r0).c_str());
            break;
        }
        case op_create_arguments: {
            printf("[%4d] create_arguments\n", location);
            break;
        }
        case op_convert_this: {
            int r0 = (++it)->u.operand;
            printf("[%4d] convert_this %s\n", location, registerName(r0).c_str());
            break;
        }
        case op_unexpected_load: {
            int r0 = (++it)->u.operand;
            int k0 = (++it)->u.operand;
            printf("[%4d] unexpected_load\t %s, %s\n", location, registerName(r0).c_str(), constantName(exec, k0, unexpectedConstants[k0]).c_str());
            break;
        }
        case op_new_object: {
            int r0 = (++it)->u.operand;
            printf("[%4d] new_object\t %s\n", location, registerName(r0).c_str());
            break;
        }
        case op_new_array: {
            int dst = (++it)->u.operand;
            int argv = (++it)->u.operand;
            int argc = (++it)->u.operand;
            printf("[%4d] new_array\t %s, %s, %d\n", location, registerName(dst).c_str(), registerName(argv).c_str(), argc);
            break;
        }
        case op_new_regexp: {
            int r0 = (++it)->u.operand;
            int re0 = (++it)->u.operand;
            printf("[%4d] new_regexp\t %s, %s\n", location, registerName(r0).c_str(), regexpName(re0, regexps[re0].get()).c_str());
            break;
        }
        case op_mov: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printf("[%4d] mov\t\t %s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str());
            break;
        }
        case op_not: {
            printUnaryOp(location, it, "not");
            break;
        }
        case op_eq: {
            printBinaryOp(location, it, "eq");
            break;
        }
        case op_eq_null: {
            printUnaryOp(location, it, "eq_null");
            break;
        }
        case op_neq: {
            printBinaryOp(location, it, "neq");
            break;
        }
        case op_neq_null: {
            printUnaryOp(location, it, "neq_null");
            break;
        }
        case op_stricteq: {
            printBinaryOp(location, it, "stricteq");
            break;
        }
        case op_nstricteq: {
            printBinaryOp(location, it, "nstricteq");
            break;
        }
        case op_less: {
            printBinaryOp(location, it, "less");
            break;
        }
        case op_lesseq: {
            printBinaryOp(location, it, "lesseq");
            break;
        }
        case op_pre_inc: {
            int r0 = (++it)->u.operand;
            printf("[%4d] pre_inc\t\t %s\n", location, registerName(r0).c_str());
            break;
        }
        case op_pre_dec: {
            int r0 = (++it)->u.operand;
            printf("[%4d] pre_dec\t\t %s\n", location, registerName(r0).c_str());
            break;
        }
        case op_post_inc: {
            printUnaryOp(location, it, "post_inc");
            break;
        }
        case op_post_dec: {
            printUnaryOp(location, it, "post_dec");
            break;
        }
        case op_to_jsnumber: {
            printUnaryOp(location, it, "to_jsnumber");
            break;
        }
        case op_negate: {
            printUnaryOp(location, it, "negate");
            ++it;
            break;
        }
        case op_add: {
            printBinaryOp(location, it, "add");
            ++it;
            break;
        }
        case op_mul: {
            printBinaryOp(location, it, "mul");
            ++it;
            break;
        }
        case op_div: {
            printBinaryOp(location, it, "div");
            break;
        }
        case op_mod: {
            printBinaryOp(location, it, "mod");
            break;
        }
        case op_sub: {
            printBinaryOp(location, it, "sub");
            ++it;
            break;
        }
        case op_lshift: {
            printBinaryOp(location, it, "lshift");
            break;            
        }
        case op_rshift: {
            printBinaryOp(location, it, "rshift");
            break;
        }
        case op_urshift: {
            printBinaryOp(location, it, "urshift");
            break;
        }
        case op_bitand: {
            printBinaryOp(location, it, "bitand");
            ++it;
            break;
        }
        case op_bitxor: {
            printBinaryOp(location, it, "bitxor");
            ++it;
            break;
        }
        case op_bitor: {
            printBinaryOp(location, it, "bitor");
            ++it;
            break;
        }
        case op_bitnot: {
            printUnaryOp(location, it, "bitnot");
            break;
        }
        case op_instanceof: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            int r3 = (++it)->u.operand;
            printf("[%4d] instanceof\t\t %s, %s, %s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str(), registerName(r2).c_str(), registerName(r3).c_str());
            break;
        }
        case op_typeof: {
            printUnaryOp(location, it, "typeof");
            break;
        }
        case op_is_undefined: {
            printUnaryOp(location, it, "is_undefined");
            break;
        }
        case op_is_boolean: {
            printUnaryOp(location, it, "is_boolean");
            break;
        }
        case op_is_number: {
            printUnaryOp(location, it, "is_number");
            break;
        }
        case op_is_string: {
            printUnaryOp(location, it, "is_string");
            break;
        }
        case op_is_object: {
            printUnaryOp(location, it, "is_object");
            break;
        }
        case op_is_function: {
            printUnaryOp(location, it, "is_function");
            break;
        }
        case op_in: {
            printBinaryOp(location, it, "in");
            break;
        }
        case op_resolve: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            printf("[%4d] resolve\t\t %s, %s\n", location, registerName(r0).c_str(), idName(id0, identifiers[id0]).c_str());
            break;
        }
        case op_resolve_skip: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int skipLevels = (++it)->u.operand;
            printf("[%4d] resolve_skip\t %s, %s, %d\n", location, registerName(r0).c_str(), idName(id0, identifiers[id0]).c_str(), skipLevels);
            break;
        }
        case op_resolve_global: {
            int r0 = (++it)->u.operand;
            JSValue* scope = static_cast<JSValue*>((++it)->u.jsCell);
            int id0 = (++it)->u.operand;
            printf("[%4d] resolve_global\t %s, %s, %s\n", location, registerName(r0).c_str(), valueToSourceString(exec, scope).ascii(), idName(id0, identifiers[id0]).c_str());
            it += 2;
            break;
        }
        case op_get_scoped_var: {
            int r0 = (++it)->u.operand;
            int index = (++it)->u.operand;
            int skipLevels = (++it)->u.operand;
            printf("[%4d] get_scoped_var\t %s, %d, %d\n", location, registerName(r0).c_str(), index, skipLevels);
            break;
        }
        case op_put_scoped_var: {
            int index = (++it)->u.operand;
            int skipLevels = (++it)->u.operand;
            int r0 = (++it)->u.operand;
            printf("[%4d] put_scoped_var\t %d, %d, %s\n", location, index, skipLevels, registerName(r0).c_str());
            break;
        }
        case op_get_global_var: {
            int r0 = (++it)->u.operand;
            JSValue* scope = static_cast<JSValue*>((++it)->u.jsCell);
            int index = (++it)->u.operand;
            printf("[%4d] get_global_var\t %s, %s, %d\n", location, registerName(r0).c_str(), valueToSourceString(exec, scope).ascii(), index);
            break;
        }
        case op_put_global_var: {
            JSValue* scope = static_cast<JSValue*>((++it)->u.jsCell);
            int index = (++it)->u.operand;
            int r0 = (++it)->u.operand;
            printf("[%4d] put_global_var\t %s, %d, %s\n", location, valueToSourceString(exec, scope).ascii(), index, registerName(r0).c_str());
            break;
        }
        case op_resolve_base: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            printf("[%4d] resolve_base\t %s, %s\n", location, registerName(r0).c_str(), idName(id0, identifiers[id0]).c_str());
            break;
        }
        case op_resolve_with_base: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            printf("[%4d] resolve_with_base %s, %s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str(), idName(id0, identifiers[id0]).c_str());
            break;
        }
        case op_resolve_func: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            printf("[%4d] resolve_func\t %s, %s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str(), idName(id0, identifiers[id0]).c_str());
            break;
        }
        case op_get_by_id: {
            printGetByIdOp(location, it, identifiers, "get_by_id");
            break;
        }
        case op_get_by_id_self: {
            printGetByIdOp(location, it, identifiers, "get_by_id_self");
            break;
        }
        case op_get_by_id_proto: {
            printGetByIdOp(location, it, identifiers, "get_by_id_proto");
            break;
        }
        case op_get_by_id_chain: {
            printGetByIdOp(location, it, identifiers, "get_by_id_chain");
            break;
        }
        case op_get_by_id_generic: {
            printGetByIdOp(location, it, identifiers, "get_by_id_generic");
            break;
        }
        case op_get_array_length: {
            printGetByIdOp(location, it, identifiers, "get_array_length");
            break;
        }
        case op_get_string_length: {
            printGetByIdOp(location, it, identifiers, "get_string_length");
            break;
        }
        case op_put_by_id: {
            printPutByIdOp(location, it, identifiers, "put_by_id");
            break;
        }
        case op_put_by_id_replace: {
            printPutByIdOp(location, it, identifiers, "put_by_id_replace");
            break;
        }
        case op_put_by_id_transition: {
            printPutByIdOp(location, it, identifiers, "put_by_id_transition");
            break;
        }
        case op_put_by_id_generic: {
            printPutByIdOp(location, it, identifiers, "put_by_id_generic");
            break;
        }
        case op_put_getter: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printf("[%4d] put_getter\t %s, %s, %s\n", location, registerName(r0).c_str(), idName(id0, identifiers[id0]).c_str(), registerName(r1).c_str());
            break;
        }
        case op_put_setter: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printf("[%4d] put_setter\t %s, %s, %s\n", location, registerName(r0).c_str(), idName(id0, identifiers[id0]).c_str(), registerName(r1).c_str());
            break;
        }
        case op_del_by_id: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            printf("[%4d] del_by_id\t %s, %s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str(), idName(id0, identifiers[id0]).c_str());
            break;
        }
        case op_get_by_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printf("[%4d] get_by_val\t %s, %s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str(), registerName(r2).c_str());
            break;
        }
        case op_put_by_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printf("[%4d] put_by_val\t %s, %s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str(), registerName(r2).c_str());
            break;
        }
        case op_del_by_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printf("[%4d] del_by_val\t %s, %s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str(), registerName(r2).c_str());
            break;
        }
        case op_put_by_index: {
            int r0 = (++it)->u.operand;
            unsigned n0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printf("[%4d] put_by_index\t %s, %u, %s\n", location, registerName(r0).c_str(), n0, registerName(r1).c_str());
            break;
        }
        case op_jmp: {
            int offset = (++it)->u.operand;
            printf("[%4d] jmp\t\t %d(->%d)\n", location, offset, jumpTarget(begin, it, offset));
            break;
        }
        case op_loop: {
            int offset = (++it)->u.operand;
            printf("[%4d] loop\t\t %d(->%d)\n", location, offset, jumpTarget(begin, it, offset));
            break;
        }
        case op_jtrue: {
            printConditionalJump(begin, it, location, "jtrue");
            break;
        }
        case op_loop_if_true: {
            printConditionalJump(begin, it, location, "loop_if_true");
            break;
        }
        case op_jfalse: {
            printConditionalJump(begin, it, location, "jfalse");
            break;
        }
        case op_jeq_null: {
            printConditionalJump(begin, it, location, "jeq_null");
            break;
        }
        case op_jneq_null: {
            printConditionalJump(begin, it, location, "jneq_null");
            break;
        }
        case op_jnless: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printf("[%4d] jnless\t\t %s, %s, %d(->%d)\n", location, registerName(r0).c_str(), registerName(r1).c_str(), offset, jumpTarget(begin, it, offset));
            break;
        }
        case op_loop_if_less: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printf("[%4d] loop_if_less\t %s, %s, %d(->%d)\n", location, registerName(r0).c_str(), registerName(r1).c_str(), offset, jumpTarget(begin, it, offset));
            break;
        }
        case op_loop_if_lesseq: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printf("[%4d] loop_if_lesseq\t %s, %s, %d(->%d)\n", location, registerName(r0).c_str(), registerName(r1).c_str(), offset, jumpTarget(begin, it, offset));
            break;
        }
        case op_switch_imm: {
            int tableIndex = (++it)->u.operand;
            int defaultTarget = (++it)->u.operand;
            int scrutineeRegister = (++it)->u.operand;
            printf("[%4d] switch_imm\t %d, %d(->%d), %s\n", location, tableIndex, defaultTarget, jumpTarget(begin, it, defaultTarget), registerName(scrutineeRegister).c_str());
            break;
        }
        case op_switch_char: {
            int tableIndex = (++it)->u.operand;
            int defaultTarget = (++it)->u.operand;
            int scrutineeRegister = (++it)->u.operand;
            printf("[%4d] switch_char\t %d, %d(->%d), %s\n", location, tableIndex, defaultTarget, jumpTarget(begin, it, defaultTarget), registerName(scrutineeRegister).c_str());
            break;
        }
        case op_switch_string: {
            int tableIndex = (++it)->u.operand;
            int defaultTarget = (++it)->u.operand;
            int scrutineeRegister = (++it)->u.operand;
            printf("[%4d] switch_string\t %d, %d(->%d), %s\n", location, tableIndex, defaultTarget, jumpTarget(begin, it, defaultTarget), registerName(scrutineeRegister).c_str());
            break;
        }
        case op_new_func: {
            int r0 = (++it)->u.operand;
            int f0 = (++it)->u.operand;
            printf("[%4d] new_func\t\t %s, f%d\n", location, registerName(r0).c_str(), f0);
            break;
        }
        case op_new_func_exp: {
            int r0 = (++it)->u.operand;
            int f0 = (++it)->u.operand;
            printf("[%4d] new_func_exp\t %s, f%d\n", location, registerName(r0).c_str(), f0);
            break;
        }
        case op_call: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            int tempCount = (++it)->u.operand;
            int argCount = (++it)->u.operand;
            int registerOffset = (++it)->u.operand;
            printf("[%4d] call\t\t %s, %s, %s, %d, %d, %d\n", location, registerName(r0).c_str(), registerName(r1).c_str(), registerName(r2).c_str(), tempCount, argCount, registerOffset);
            break;
        }
        case op_call_eval: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            int tempCount = (++it)->u.operand;
            int argCount = (++it)->u.operand;
            int registerOffset = (++it)->u.operand;
            printf("[%4d] call_eval\t\t %s, %s, %s, %d, %d, %d\n", location, registerName(r0).c_str(), registerName(r1).c_str(), registerName(r2).c_str(), tempCount, argCount, registerOffset);
            break;
        }
        case op_tear_off_activation: {
            int r0 = (++it)->u.operand;
            printf("[%4d] tear_off_activation\t %s\n", location, registerName(r0).c_str());
            break;
        }
        case op_tear_off_arguments: {
            printf("[%4d] tear_off_arguments\n", location);
            break;
        }
        case op_ret: {
            int r0 = (++it)->u.operand;
            printf("[%4d] ret\t\t %s\n", location, registerName(r0).c_str());
            break;
        }
        case op_construct: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            int tempCount = (++it)->u.operand;
            int argCount = (++it)->u.operand;
            int registerOffset = (++it)->u.operand;
            printf("[%4d] construct\t %s, %s, %s, %d, %d, %d\n", location, registerName(r0).c_str(), registerName(r1).c_str(), registerName(r2).c_str(), tempCount, argCount, registerOffset);
            break;
        }
        case op_construct_verify: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printf("[%4d] construct_verify\t %s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str());
            break;
        }
        case op_get_pnames: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printf("[%4d] get_pnames\t %s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str());
            break;
        }
        case op_next_pname: {
            int dest = (++it)->u.operand;
            int iter = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printf("[%4d] next_pname\t %s, %s, %d(->%d)\n", location, registerName(dest).c_str(), registerName(iter).c_str(), offset, jumpTarget(begin, it, offset));
            break;
        }
        case op_push_scope: {
            int r0 = (++it)->u.operand;
            printf("[%4d] push_scope\t %s\n", location, registerName(r0).c_str());
            break;
        }
        case op_pop_scope: {
            printf("[%4d] pop_scope\n", location);
            break;
        }
        case op_push_new_scope: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printf("[%4d] push_new_scope \t%s, %s, %s\n", location, registerName(r0).c_str(), idName(id0, identifiers[id0]).c_str(), registerName(r1).c_str());
            break;
        }
        case op_jmp_scopes: {
            int scopeDelta = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printf("[%4d] jmp_scopes\t^%d, %d(->%d)\n", location, scopeDelta, offset, jumpTarget(begin, it, offset));
            break;
        }
        case op_catch: {
            int r0 = (++it)->u.operand;
            printf("[%4d] catch\t\t %s\n", location, registerName(r0).c_str());
            break;
        }
        case op_throw: {
            int r0 = (++it)->u.operand;
            printf("[%4d] throw\t\t %s\n", location, registerName(r0).c_str());
            break;
        }
        case op_new_error: {
            int r0 = (++it)->u.operand;
            int errorType = (++it)->u.operand;
            int k0 = (++it)->u.operand;
            printf("[%4d] new_error\t %s, %d, %s\n", location, registerName(r0).c_str(), errorType, constantName(exec, k0, unexpectedConstants[k0]).c_str());
            break;
        }
        case op_jsr: {
            int retAddrDst = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printf("[%4d] jsr\t\t %s, %d(->%d)\n", location, registerName(retAddrDst).c_str(), offset, jumpTarget(begin, it, offset));
            break;
        }
        case op_sret: {
            int retAddrSrc = (++it)->u.operand;
            printf("[%4d] sret\t\t %s\n", location, registerName(retAddrSrc).c_str());
            break;
        }
        case op_debug: {
            int debugHookID = (++it)->u.operand;
            int firstLine = (++it)->u.operand;
            int lastLine = (++it)->u.operand;
            printf("[%4d] debug\t\t %s, %d, %d\n", location, debugHookName(debugHookID), firstLine, lastLine);
            break;
        }
        case op_profile_will_call: {
            int function = (++it)->u.operand;
            printf("[%4d] profile_will_call %s\n", location, registerName(function).c_str());
            break;
        }
        case op_profile_did_call: {
            int function = (++it)->u.operand;
            printf("[%4d] profile_did_call\t %s\n", location, registerName(function).c_str());
            break;
        }
        case op_end: {
            int r0 = (++it)->u.operand;
            printf("[%4d] end\t\t %s\n", location, registerName(r0).c_str());
            break;
        }
    }
}

#endif // !defined(NDEBUG) || ENABLE(OPCODE_SAMPLING)

CodeBlock::~CodeBlock()
{
    for (size_t size = globalResolveInstructions.size(), i = 0; i < size; ++i) {
        derefStructureIDs(&instructions[globalResolveInstructions[i]]);
    }

    for (size_t size = propertyAccessInstructions.size(), i = 0; i < size; ++i) {
        derefStructureIDs(&instructions[propertyAccessInstructions[i].opcodeIndex]);
        if (propertyAccessInstructions[i].stubRoutine)
            WTF::fastFreeExecutable(propertyAccessInstructions[i].stubRoutine);
    }

    for (size_t size = callLinkInfos.size(), i = 0; i < size; ++i) {
        CallLinkInfo* callLinkInfo = &callLinkInfos[i];
        if (callLinkInfo->isLinked())
            callLinkInfo->callee->removeCaller(callLinkInfo);
    }

#if ENABLE(CTI) 
    unlinkCallers();

    if (ctiCode)
        WTF::fastFreeExecutable(ctiCode);
#endif
}

#if ENABLE(CTI) 
void CodeBlock::unlinkCallers()
{
    size_t size = linkedCallerList.size();
    for (size_t i = 0; i < size; ++i) {
        CallLinkInfo* currentCaller = linkedCallerList[i];
        CTI::unlinkCall(currentCaller);
        currentCaller->setUnlinked();
    }
    linkedCallerList.clear();
}
#endif

void CodeBlock::derefStructureIDs(Instruction* vPC) const
{
    Machine* machine = globalData->machine;

    if (vPC[0].u.opcode == machine->getOpcode(op_get_by_id_self)) {
        vPC[4].u.structureID->deref();
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_get_by_id_proto)) {
        vPC[4].u.structureID->deref();
        vPC[5].u.structureID->deref();
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_get_by_id_chain)) {
        vPC[4].u.structureID->deref();
        vPC[5].u.structureIDChain->deref();
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_put_by_id_transition)) {
        vPC[4].u.structureID->deref();
        vPC[5].u.structureID->deref();
        vPC[6].u.structureIDChain->deref();
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_put_by_id_replace)) {
        vPC[4].u.structureID->deref();
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_resolve_global)) {
        if(vPC[4].u.structureID)
            vPC[4].u.structureID->deref();
        return;
    }
    
    // These instructions don't ref their StructureIDs.
    ASSERT(vPC[0].u.opcode == machine->getOpcode(op_get_by_id) || vPC[0].u.opcode == machine->getOpcode(op_put_by_id) || vPC[0].u.opcode == machine->getOpcode(op_get_by_id_generic) || vPC[0].u.opcode == machine->getOpcode(op_put_by_id_generic) || vPC[0].u.opcode == machine->getOpcode(op_get_array_length) || vPC[0].u.opcode == machine->getOpcode(op_get_string_length));
}

void CodeBlock::refStructureIDs(Instruction* vPC) const
{
    Machine* machine = globalData->machine;

    if (vPC[0].u.opcode == machine->getOpcode(op_get_by_id_self)) {
        vPC[4].u.structureID->ref();
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_get_by_id_proto)) {
        vPC[4].u.structureID->ref();
        vPC[5].u.structureID->ref();
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_get_by_id_chain)) {
        vPC[4].u.structureID->ref();
        vPC[5].u.structureIDChain->ref();
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_put_by_id_transition)) {
        vPC[4].u.structureID->ref();
        vPC[5].u.structureID->ref();
        vPC[6].u.structureIDChain->ref();
        return;
    }
    if (vPC[0].u.opcode == machine->getOpcode(op_put_by_id_replace)) {
        vPC[4].u.structureID->ref();
        return;
    }
    
    // These instructions don't ref their StructureIDs.
    ASSERT(vPC[0].u.opcode == machine->getOpcode(op_get_by_id) || vPC[0].u.opcode == machine->getOpcode(op_put_by_id) || vPC[0].u.opcode == machine->getOpcode(op_get_by_id_generic) || vPC[0].u.opcode == machine->getOpcode(op_put_by_id_generic));
}

void CodeBlock::mark()
{
    for (size_t i = 0; i < constantRegisters.size(); ++i)
        if (!constantRegisters[i].marked())
            constantRegisters[i].mark();

    for (size_t i = 0; i < unexpectedConstants.size(); ++i)
        if (!unexpectedConstants[i]->marked())
            unexpectedConstants[i]->mark();

    for (size_t i = 0; i < functions.size(); ++i)
        functions[i]->body()->mark();

    for (size_t i = 0; i < functionExpressions.size(); ++i)
        functionExpressions[i]->body()->mark();
}

bool CodeBlock::getHandlerForVPC(const Instruction* vPC, Instruction*& target, int& scopeDepth)
{
    Vector<HandlerInfo>::iterator ptr = exceptionHandlers.begin(); 
    Vector<HandlerInfo>::iterator end = exceptionHandlers.end();
    unsigned addressOffset = vPC - instructions.begin();
    ASSERT(addressOffset < instructions.size());
    
    for (; ptr != end; ++ptr) {
        // Handlers are ordered innermost first, so the first handler we encounter
        // that contains the source address is the correct handler to use.
        if (ptr->start <= addressOffset && ptr->end >= addressOffset) {
            scopeDepth = ptr->scopeDepth;
            target = instructions.begin() + ptr->target;
            return true;
        }
    }
    return false;
}

void* CodeBlock::nativeExceptionCodeForHandlerVPC(const Instruction* handlerVPC)
{
    Vector<HandlerInfo>::iterator ptr = exceptionHandlers.begin(); 
    Vector<HandlerInfo>::iterator end = exceptionHandlers.end();
    
    for (; ptr != end; ++ptr) {
        Instruction*target = instructions.begin() + ptr->target;
        if (handlerVPC == target)
            return ptr->nativeCode;
    }

    return 0;
}

int CodeBlock::lineNumberForVPC(const Instruction* vPC)
{
    unsigned instructionOffset = vPC - instructions.begin();
    ASSERT(instructionOffset < instructions.size());

    if (!lineInfo.size())
        return ownerNode->source().firstLine(); // Empty function

    int low = 0;
    int high = lineInfo.size();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (lineInfo[mid].instructionOffset <= instructionOffset)
            low = mid + 1;
        else
            high = mid;
    }
    
    if (!low)
        return ownerNode->source().firstLine();
    return lineInfo[low - 1].lineNumber;
}

int CodeBlock::expressionRangeForVPC(const Instruction* vPC, int& divot, int& startOffset, int& endOffset)
{
    unsigned instructionOffset = vPC - instructions.begin();
    ASSERT(instructionOffset < instructions.size());

    if (!expressionInfo.size()) {
        // We didn't think anything could throw.  Apparently we were wrong.
        startOffset = 0;
        endOffset = 0;
        divot = 0;
        return lineNumberForVPC(vPC);
    }

    int low = 0;
    int high = expressionInfo.size();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (expressionInfo[mid].instructionOffset <= instructionOffset)
            low = mid + 1;
        else
            high = mid;
    }
    
    ASSERT(low);
    if (!low) {
        startOffset = 0;
        endOffset = 0;
        divot = 0;
        return lineNumberForVPC(vPC);
    }

    startOffset = expressionInfo[low - 1].startOffset;
    endOffset = expressionInfo[low - 1].endOffset;
    divot = expressionInfo[low - 1].divotPoint + sourceOffset;
    return lineNumberForVPC(vPC);
}

int32_t SimpleJumpTable::offsetForValue(int32_t value, int32_t defaultOffset)
{
    if (value >= min && static_cast<uint32_t>(value - min) < branchOffsets.size()) {
        int32_t offset = branchOffsets[value - min];
        if (offset)
            return offset;
    }
    return defaultOffset;        
}

} // namespace JSC
