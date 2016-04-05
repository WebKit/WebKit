/*
 * Copyright (C) 2008-2010, 2012-2015 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#include "BasicBlockLocation.h"
#include "BytecodeGenerator.h"
#include "BytecodeUseDef.h"
#include "CallLinkStatus.h"
#include "DFGCapabilities.h"
#include "DFGCommon.h"
#include "DFGDriver.h"
#include "DFGJITCode.h"
#include "DFGWorklist.h"
#include "Debugger.h"
#include "FunctionExecutableDump.h"
#include "GetPutInfo.h"
#include "InlineCallFrame.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JSCJSValue.h"
#include "JSFunction.h"
#include "JSLexicalEnvironment.h"
#include "JSModuleEnvironment.h"
#include "LLIntEntrypoint.h"
#include "LowLevelInterpreter.h"
#include "JSCInlines.h"
#include "PCToCodeOriginMap.h"
#include "PolymorphicAccess.h"
#include "ProfilerDatabase.h"
#include "ReduceWhitespace.h"
#include "Repatch.h"
#include "SlotVisitorInlines.h"
#include "StackVisitor.h"
#include "TypeLocationCache.h"
#include "TypeProfiler.h"
#include "UnlinkedInstructionStream.h"
#include <wtf/BagToHashMap.h>
#include <wtf/CommaPrinter.h>
#include <wtf/StringExtras.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/UniquedStringImpl.h>

#if ENABLE(JIT)
#include "RegisterAtOffsetList.h"
#endif

#if ENABLE(DFG_JIT)
#include "DFGOperations.h"
#endif

#if ENABLE(FTL_JIT)
#include "FTLJITCode.h"
#endif

namespace JSC {

const ClassInfo CodeBlock::s_info = {
    "CodeBlock", 0, 0,
    CREATE_METHOD_TABLE(CodeBlock)
};

const ClassInfo FunctionCodeBlock::s_info = {
    "FunctionCodeBlock", &Base::s_info, 0,
    CREATE_METHOD_TABLE(FunctionCodeBlock)
};

#if ENABLE(WEBASSEMBLY)
const ClassInfo WebAssemblyCodeBlock::s_info = {
    "WebAssemblyCodeBlock", &Base::s_info, 0,
    CREATE_METHOD_TABLE(WebAssemblyCodeBlock)
};
#endif

const ClassInfo GlobalCodeBlock::s_info = {
    "GlobalCodeBlock", &Base::s_info, 0,
    CREATE_METHOD_TABLE(GlobalCodeBlock)
};

const ClassInfo ProgramCodeBlock::s_info = {
    "ProgramCodeBlock", &Base::s_info, 0,
    CREATE_METHOD_TABLE(ProgramCodeBlock)
};

const ClassInfo ModuleProgramCodeBlock::s_info = {
    "ModuleProgramCodeBlock", &Base::s_info, 0,
    CREATE_METHOD_TABLE(ModuleProgramCodeBlock)
};

const ClassInfo EvalCodeBlock::s_info = {
    "EvalCodeBlock", &Base::s_info, 0,
    CREATE_METHOD_TABLE(EvalCodeBlock)
};

void FunctionCodeBlock::destroy(JSCell* cell)
{
    jsCast<FunctionCodeBlock*>(cell)->~FunctionCodeBlock();
}

#if ENABLE(WEBASSEMBLY)
void WebAssemblyCodeBlock::destroy(JSCell* cell)
{
    jsCast<WebAssemblyCodeBlock*>(cell)->~WebAssemblyCodeBlock();
}
#endif

void ProgramCodeBlock::destroy(JSCell* cell)
{
    jsCast<ProgramCodeBlock*>(cell)->~ProgramCodeBlock();
}

void ModuleProgramCodeBlock::destroy(JSCell* cell)
{
    jsCast<ModuleProgramCodeBlock*>(cell)->~ModuleProgramCodeBlock();
}

void EvalCodeBlock::destroy(JSCell* cell)
{
    jsCast<EvalCodeBlock*>(cell)->~EvalCodeBlock();
}

CString CodeBlock::inferredName() const
{
    switch (codeType()) {
    case GlobalCode:
        return "<global>";
    case EvalCode:
        return "<eval>";
    case FunctionCode:
        return jsCast<FunctionExecutable*>(ownerExecutable())->inferredName().utf8();
    case ModuleCode:
        return "<module>";
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
        m_hash = CodeBlockHash(ownerScriptExecutable()->source(), specializationKind());
    }
    return m_hash;
}

CString CodeBlock::sourceCodeForTools() const
{
    if (codeType() != FunctionCode)
        return ownerScriptExecutable()->source().toUTF8();
    
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
        provider->source().substring(rangeStart, rangeEnd - rangeStart).utf8());
}

CString CodeBlock::sourceCodeOnOneLine() const
{
    return reduceWhitespace(sourceCodeForTools());
}

CString CodeBlock::hashAsStringIfPossible() const
{
    if (hasHash() || isSafeToComputeHash())
        return toCString(hash());
    return "<no-hash>";
}

void CodeBlock::dumpAssumingJITType(PrintStream& out, JITCode::JITType jitType) const
{
    out.print(inferredName(), "#", hashAsStringIfPossible());
    out.print(":[", RawPointer(this), "->");
    if (!!m_alternative)
        out.print(RawPointer(alternative()), "->");
    out.print(RawPointer(ownerExecutable()), ", ", jitType, codeType());

    if (codeType() == FunctionCode)
        out.print(specializationKind());
    out.print(", ", instructionCount());
    if (this->jitType() == JITCode::BaselineJIT && m_shouldAlwaysBeInlined)
        out.print(" (ShouldAlwaysBeInlined)");
    if (ownerScriptExecutable()->neverInline())
        out.print(" (NeverInline)");
    if (ownerScriptExecutable()->neverOptimize())
        out.print(" (NeverOptimize)");
    if (ownerScriptExecutable()->didTryToEnterInLoop())
        out.print(" (DidTryToEnterInLoop)");
    if (ownerScriptExecutable()->isStrictMode())
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

static CString idName(int id0, const Identifier& ident)
{
    return toCString(ident.impl(), "(@id", id0, ")");
}

CString CodeBlock::registerName(int r) const
{
    if (isConstantRegisterIndex(r))
        return constantName(r);

    return toCString(VirtualRegister(r));
}

CString CodeBlock::constantName(int index) const
{
    JSValue value = getConstant(index);
    return toCString(value, "(", VirtualRegister(index), ")");
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
    if (regExp->sticky())
        postfix[index++] = 'y';
    if (regExp->unicode())
        postfix[index++] = 'u';

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
    case op_get_array_length:
        op = "array_length";
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
#if COMPILER_QUIRK(CONSIDERS_UNREACHABLE_CODE)
        op = 0;
#endif
    }
    int r0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;
    int id0 = (++it)->u.operand;
    printLocationAndOp(out, exec, location, it, op);
    out.printf("%s, %s, %s", registerName(r0).data(), registerName(r1).data(), idName(id0, identifier(id0)).data());
    it += 4; // Increment up to the value profiler.
}

static void dumpStructure(PrintStream& out, const char* name, Structure* structure, const Identifier& ident)
{
    if (!structure)
        return;
    
    out.printf("%s = %p", name, structure);
    
    PropertyOffset offset = structure->getConcurrently(ident.impl());
    if (offset != invalidOffset)
        out.printf(" (offset = %d)", offset);
}

static void dumpChain(PrintStream& out, StructureChain* chain, const Identifier& ident)
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
        dumpStructure(out, "struct", currentStructure->get(), ident);
    }
    out.printf("]");
}

void CodeBlock::printGetByIdCacheStatus(PrintStream& out, ExecState* exec, int location, const StubInfoMap& map)
{
    Instruction* instruction = instructions().begin() + location;

    const Identifier& ident = identifier(instruction[3].u.operand);
    
    UNUSED_PARAM(ident); // tell the compiler to shut up in certain platform configurations.
    
    if (exec->interpreter()->getOpcodeID(instruction[0].u.opcode) == op_get_array_length)
        out.printf(" llint(array_length)");
    else if (StructureID structureID = instruction[4].u.structureID) {
        Structure* structure = m_vm->heap.structureIDTable().get(structureID);
        out.printf(" llint(");
        dumpStructure(out, "struct", structure, ident);
        out.printf(")");
    }

#if ENABLE(JIT)
    if (StructureStubInfo* stubPtr = map.get(CodeOrigin(location))) {
        StructureStubInfo& stubInfo = *stubPtr;
        if (stubInfo.resetByGC)
            out.print(" (Reset By GC)");
        
        out.printf(" jit(");
            
        Structure* baseStructure = nullptr;
        PolymorphicAccess* stub = nullptr;
            
        switch (stubInfo.cacheType) {
        case CacheType::GetByIdSelf:
            out.printf("self");
            baseStructure = stubInfo.u.byIdSelf.baseObjectStructure.get();
            break;
        case CacheType::Stub:
            out.printf("stub");
            stub = stubInfo.u.stub;
            break;
        case CacheType::Unset:
            out.printf("unset");
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
            
        if (baseStructure) {
            out.printf(", ");
            dumpStructure(out, "struct", baseStructure, ident);
        }

        if (stub)
            out.print(", ", *stub);

        out.printf(")");
    }
#else
    UNUSED_PARAM(map);
#endif
}

void CodeBlock::printPutByIdCacheStatus(PrintStream& out, int location, const StubInfoMap& map)
{
    Instruction* instruction = instructions().begin() + location;

    const Identifier& ident = identifier(instruction[2].u.operand);
    
    UNUSED_PARAM(ident); // tell the compiler to shut up in certain platform configurations.

    out.print(", ", instruction[8].u.putByIdFlags);
    
    if (StructureID structureID = instruction[4].u.structureID) {
        Structure* structure = m_vm->heap.structureIDTable().get(structureID);
        out.print(" llint(");
        if (StructureID newStructureID = instruction[6].u.structureID) {
            Structure* newStructure = m_vm->heap.structureIDTable().get(newStructureID);
            dumpStructure(out, "prev", structure, ident);
            out.print(", ");
            dumpStructure(out, "next", newStructure, ident);
            if (StructureChain* chain = instruction[7].u.structureChain.get()) {
                out.print(", ");
                dumpChain(out, chain, ident);
            }
        } else
            dumpStructure(out, "struct", structure, ident);
        out.print(")");
    }

#if ENABLE(JIT)
    if (StructureStubInfo* stubPtr = map.get(CodeOrigin(location))) {
        StructureStubInfo& stubInfo = *stubPtr;
        if (stubInfo.resetByGC)
            out.print(" (Reset By GC)");
        
        out.printf(" jit(");
        
        switch (stubInfo.cacheType) {
        case CacheType::PutByIdReplace:
            out.print("replace, ");
            dumpStructure(out, "struct", stubInfo.u.byIdSelf.baseObjectStructure.get(), ident);
            break;
        case CacheType::Stub: {
            out.print("stub, ", *stubInfo.u.stub);
            break;
        }
        case CacheType::Unset:
            out.printf("unset");
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        out.printf(")");
    }
#else
    UNUSED_PARAM(map);
#endif
}

void CodeBlock::printCallOp(PrintStream& out, ExecState* exec, int location, const Instruction*& it, const char* op, CacheDumpMode cacheDumpMode, bool& hasPrintedProfiling, const CallLinkInfoMap& map)
{
    int dst = (++it)->u.operand;
    int func = (++it)->u.operand;
    int argCount = (++it)->u.operand;
    int registerOffset = (++it)->u.operand;
    printLocationAndOp(out, exec, location, it, op);
    out.printf("%s, %s, %d, %d", registerName(dst).data(), registerName(func).data(), argCount, registerOffset);
    if (cacheDumpMode == DumpCaches) {
        LLIntCallLinkInfo* callLinkInfo = it[1].u.callLinkInfo;
        if (callLinkInfo->lastSeenCallee) {
            out.printf(
                " llint(%p, exec %p)",
                callLinkInfo->lastSeenCallee.get(),
                callLinkInfo->lastSeenCallee->executable());
        }
#if ENABLE(JIT)
        if (CallLinkInfo* info = map.get(CodeOrigin(location))) {
            JSFunction* target = info->lastSeenCallee();
            if (target)
                out.printf(" jit(%p, exec %p)", target, target->executable());
        }
        
        if (jitType() != JITCode::FTLJIT)
            out.print(" status(", CallLinkStatus::computeFor(this, location, map), ")");
#else
        UNUSED_PARAM(map);
#endif
    }
    ++it;
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

void CodeBlock::dumpSource()
{
    dumpSource(WTF::dataFile());
}

void CodeBlock::dumpSource(PrintStream& out)
{
    ScriptExecutable* executable = ownerScriptExecutable();
    if (executable->isFunctionExecutable()) {
        FunctionExecutable* functionExecutable = reinterpret_cast<FunctionExecutable*>(executable);
        StringView source = functionExecutable->source().provider()->getRange(
            functionExecutable->parametersStartOffset(),
            functionExecutable->typeProfilingEndOffset() + 1); // Type profiling end offset is the character before the '}'.
        
        out.print("function ", inferredName(), source);
        return;
    }
    out.print(executable->source().view());
}

void CodeBlock::dumpBytecode()
{
    dumpBytecode(WTF::dataFile());
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
        m_numParameters, m_numCalleeLocals, m_numVars);
    out.printf("\n");
    
    StubInfoMap stubInfos;
    CallLinkInfoMap callLinkInfos;
    getStubInfoMap(stubInfos);
    getCallLinkInfoMap(callLinkInfos);
    
    const Instruction* begin = instructions().begin();
    const Instruction* end = instructions().end();
    for (const Instruction* it = begin; it != end; ++it)
        dumpBytecode(out, exec, begin, it, stubInfos, callLinkInfos);
    
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
            const char* sourceCodeRepresentationDescription = nullptr;
            switch (m_constantsSourceCodeRepresentation[i]) {
            case SourceCodeRepresentation::Double:
                sourceCodeRepresentationDescription = ": in source as double";
                break;
            case SourceCodeRepresentation::Integer:
                sourceCodeRepresentationDescription = ": in source as integer";
                break;
            case SourceCodeRepresentation::Other:
                sourceCodeRepresentationDescription = "";
                break;
            }
            out.printf("   k%u = %s%s\n", static_cast<unsigned>(i), toCString(m_constantRegisters[i].get()).data(), sourceCodeRepresentationDescription);
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

    dumpExceptionHandlers(out);
    
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
                out.printf("\t\t\"%s\" => %04d\n", iter->key->utf8().data(), iter->value.branchOffset);
            out.printf("      }\n");
            ++i;
        } while (i < m_rareData->m_stringSwitchJumpTables.size());
    }

    if (m_rareData && !m_rareData->m_liveCalleeLocalsAtYield.isEmpty()) {
        out.printf("\nLive Callee Locals:\n");
        unsigned i = 0;
        do {
            const FastBitVector& liveness = m_rareData->m_liveCalleeLocalsAtYield[i];
            out.printf("  live%1u = ", i);
            liveness.dump(out);
            out.printf("\n");
            ++i;
        } while (i < m_rareData->m_liveCalleeLocalsAtYield.size());
    }

    out.printf("\n");
}

void CodeBlock::dumpExceptionHandlers(PrintStream& out)
{
    if (m_rareData && !m_rareData->m_exceptionHandlers.isEmpty()) {
        out.printf("\nException Handlers:\n");
        unsigned i = 0;
        do {
            HandlerInfo& handler = m_rareData->m_exceptionHandlers[i];
            out.printf("\t %d: { start: [%4d] end: [%4d] target: [%4d] } %s\n",
                i + 1, handler.start, handler.end, handler.target, handler.typeName());
            ++i;
        } while (i < m_rareData->m_exceptionHandlers.size());
    }
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

void CodeBlock::dumpResultProfile(PrintStream& out, ResultProfile* profile, bool& hasPrintedProfiling)
{
    if (!profile)
        return;
    
    beginDumpProfiling(out, hasPrintedProfiling);
    out.print("results: ", *profile);
}

void CodeBlock::printLocationAndOp(PrintStream& out, ExecState*, int location, const Instruction*&, const char* op)
{
    out.printf("[%4d] %-17s ", location, op);
}

void CodeBlock::printLocationOpAndRegisterOperand(PrintStream& out, ExecState* exec, int location, const Instruction*& it, const char* op, int operand)
{
    printLocationAndOp(out, exec, location, it, op);
    out.printf("%s", registerName(operand).data());
}

void CodeBlock::dumpBytecode(
    PrintStream& out, ExecState* exec, const Instruction* begin, const Instruction*& it,
    const StubInfoMap& stubInfos, const CallLinkInfoMap& callLinkInfos)
{
    int location = it - begin;
    bool hasPrintedProfiling = false;
    OpcodeID opcode = exec->interpreter()->getOpcodeID(it->u.opcode);
    switch (opcode) {
        case op_enter: {
            printLocationAndOp(out, exec, location, it, "enter");
            break;
        }
        case op_get_scope: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "get_scope", r0);
            break;
        }
        case op_create_direct_arguments: {
            int r0 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "create_direct_arguments");
            out.printf("%s", registerName(r0).data());
            break;
        }
        case op_create_scoped_arguments: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "create_scoped_arguments");
            out.printf("%s, %s", registerName(r0).data(), registerName(r1).data());
            break;
        }
        case op_create_cloned_arguments: {
            int r0 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "create_cloned_arguments");
            out.printf("%s", registerName(r0).data());
            break;
        }
        case op_copy_rest: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            unsigned argumentOffset = (++it)->u.unsignedValue;
            printLocationAndOp(out, exec, location, it, "copy_rest");
            out.printf("%s, %s, ", registerName(r0).data(), registerName(r1).data());
            out.printf("ArgumentsOffset: %u", argumentOffset);
            break;
        }
        case op_get_rest_length: {
            int r0 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "get_rest_length");
            out.printf("%s, ", registerName(r0).data());
            unsigned argumentOffset = (++it)->u.unsignedValue;
            out.printf("ArgumentsOffset: %u", argumentOffset);
            break;
        }
        case op_create_this: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            unsigned inferredInlineCapacity = (++it)->u.operand;
            unsigned cachedFunction = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "create_this");
            out.printf("%s, %s, %u, %u", registerName(r0).data(), registerName(r1).data(), inferredInlineCapacity, cachedFunction);
            break;
        }
        case op_to_this: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "to_this", r0);
            Structure* structure = (++it)->u.structure.get();
            if (structure)
                out.print(", cache(struct = ", RawPointer(structure), ")");
            out.print(", ", (++it)->u.toThisStatus);
            break;
        }
        case op_check_tdz: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "op_check_tdz", r0);
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
        case op_profile_type: {
            int r0 = (++it)->u.operand;
            ++it;
            ++it;
            ++it;
            ++it;
            printLocationAndOp(out, exec, location, it, "op_profile_type");
            out.printf("%s", registerName(r0).data());
            break;
        }
        case op_profile_control_flow: {
            BasicBlockLocation* basicBlockLocation = (++it)->u.basicBlockLocation;
            printLocationAndOp(out, exec, location, it, "profile_control_flow");
            out.printf("[%d, %d]", basicBlockLocation->startOffset(), basicBlockLocation->endOffset());
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
        case op_to_string: {
            printUnaryOp(out, exec, location, it, "to_string");
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
        case op_overrides_has_instance: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "overrides_has_instance");
            out.printf("%s, %s, %s", registerName(r0).data(), registerName(r1).data(), registerName(r2).data());
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
        case op_instanceof_custom: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            int r3 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "instanceof_custom");
            out.printf("%s, %s, %s, %s", registerName(r0).data(), registerName(r1).data(), registerName(r2).data(), registerName(r3).data());
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
        case op_is_object_or_null: {
            printUnaryOp(out, exec, location, it, "is_object_or_null");
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
        case op_try_get_by_id: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "try_get_by_id");
            out.printf("%s, %s, %s", registerName(r0).data(), registerName(r1).data(), idName(id0, identifier(id0)).data());
            break;
        }
        case op_get_by_id:
        case op_get_array_length: {
            printGetByIdOp(out, exec, location, it);
            printGetByIdCacheStatus(out, exec, location, stubInfos);
            dumpValueProfiling(out, it, hasPrintedProfiling);
            break;
        }
        case op_put_by_id: {
            printPutByIdOp(out, exec, location, it, "put_by_id");
            printPutByIdCacheStatus(out, location, stubInfos);
            break;
        }
        case op_put_getter_by_id: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int n0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "put_getter_by_id");
            out.printf("%s, %s, %d, %s", registerName(r0).data(), idName(id0, identifier(id0)).data(), n0, registerName(r1).data());
            break;
        }
        case op_put_setter_by_id: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int n0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "put_setter_by_id");
            out.printf("%s, %s, %d, %s", registerName(r0).data(), idName(id0, identifier(id0)).data(), n0, registerName(r1).data());
            break;
        }
        case op_put_getter_setter_by_id: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int n0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "put_getter_setter_by_id");
            out.printf("%s, %s, %d, %s, %s", registerName(r0).data(), idName(id0, identifier(id0)).data(), n0, registerName(r1).data(), registerName(r2).data());
            break;
        }
        case op_put_getter_by_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int n0 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "put_getter_by_val");
            out.printf("%s, %s, %d, %s", registerName(r0).data(), registerName(r1).data(), n0, registerName(r2).data());
            break;
        }
        case op_put_setter_by_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int n0 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "put_setter_by_val");
            out.printf("%s, %s, %d, %s", registerName(r0).data(), registerName(r1).data(), n0, registerName(r2).data());
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
        case op_watchdog: {
            printLocationAndOp(out, exec, location, it, "watchdog");
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
            int r1 = (++it)->u.operand;
            int f0 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "new_func");
            out.printf("%s, %s, f%d", registerName(r0).data(), registerName(r1).data(), f0);
            break;
        }
        case op_new_generator_func: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int f0 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "new_generator_func");
            out.printf("%s, %s, f%d", registerName(r0).data(), registerName(r1).data(), f0);
            break;
        }
        case op_new_arrow_func_exp: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int f0 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "op_new_arrow_func_exp");
            out.printf("%s, %s, f%d", registerName(r0).data(), registerName(r1).data(), f0);
            break;
        }
        case op_new_func_exp: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int f0 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "new_func_exp");
            out.printf("%s, %s, f%d", registerName(r0).data(), registerName(r1).data(), f0);
            break;
        }
        case op_new_generator_func_exp: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int f0 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "new_generator_func_exp");
            out.printf("%s, %s, f%d", registerName(r0).data(), registerName(r1).data(), f0);
            break;
        }
        case op_set_function_name: {
            int funcReg = (++it)->u.operand;
            int nameReg = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "set_function_name");
            out.printf("%s, %s", registerName(funcReg).data(), registerName(nameReg).data());
            break;
        }
        case op_call: {
            printCallOp(out, exec, location, it, "call", DumpCaches, hasPrintedProfiling, callLinkInfos);
            break;
        }
        case op_tail_call: {
            printCallOp(out, exec, location, it, "tail_call", DumpCaches, hasPrintedProfiling, callLinkInfos);
            break;
        }
        case op_call_eval: {
            printCallOp(out, exec, location, it, "call_eval", DontDumpCaches, hasPrintedProfiling, callLinkInfos);
            break;
        }
            
        case op_construct_varargs:
        case op_call_varargs:
        case op_tail_call_varargs: {
            int result = (++it)->u.operand;
            int callee = (++it)->u.operand;
            int thisValue = (++it)->u.operand;
            int arguments = (++it)->u.operand;
            int firstFreeRegister = (++it)->u.operand;
            int varArgOffset = (++it)->u.operand;
            ++it;
            printLocationAndOp(out, exec, location, it, opcode == op_call_varargs ? "call_varargs" : opcode == op_construct_varargs ? "construct_varargs" : "tail_call_varargs");
            out.printf("%s, %s, %s, %s, %d, %d", registerName(result).data(), registerName(callee).data(), registerName(thisValue).data(), registerName(arguments).data(), firstFreeRegister, varArgOffset);
            dumpValueProfiling(out, it, hasPrintedProfiling);
            break;
        }

        case op_ret: {
            int r0 = (++it)->u.operand;
            printLocationOpAndRegisterOperand(out, exec, location, it, "ret", r0);
            break;
        }
        case op_construct: {
            printCallOp(out, exec, location, it, "construct", DumpCaches, hasPrintedProfiling, callLinkInfos);
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
        case op_get_enumerable_length: {
            int dst = it[1].u.operand;
            int base = it[2].u.operand;
            printLocationAndOp(out, exec, location, it, "op_get_enumerable_length");
            out.printf("%s, %s", registerName(dst).data(), registerName(base).data());
            it += OPCODE_LENGTH(op_get_enumerable_length) - 1;
            break;
        }
        case op_has_indexed_property: {
            int dst = it[1].u.operand;
            int base = it[2].u.operand;
            int propertyName = it[3].u.operand;
            ArrayProfile* arrayProfile = it[4].u.arrayProfile;
            printLocationAndOp(out, exec, location, it, "op_has_indexed_property");
            out.printf("%s, %s, %s, %p", registerName(dst).data(), registerName(base).data(), registerName(propertyName).data(), arrayProfile);
            it += OPCODE_LENGTH(op_has_indexed_property) - 1;
            break;
        }
        case op_has_structure_property: {
            int dst = it[1].u.operand;
            int base = it[2].u.operand;
            int propertyName = it[3].u.operand;
            int enumerator = it[4].u.operand;
            printLocationAndOp(out, exec, location, it, "op_has_structure_property");
            out.printf("%s, %s, %s, %s", registerName(dst).data(), registerName(base).data(), registerName(propertyName).data(), registerName(enumerator).data());
            it += OPCODE_LENGTH(op_has_structure_property) - 1;
            break;
        }
        case op_has_generic_property: {
            int dst = it[1].u.operand;
            int base = it[2].u.operand;
            int propertyName = it[3].u.operand;
            printLocationAndOp(out, exec, location, it, "op_has_generic_property");
            out.printf("%s, %s, %s", registerName(dst).data(), registerName(base).data(), registerName(propertyName).data());
            it += OPCODE_LENGTH(op_has_generic_property) - 1;
            break;
        }
        case op_get_direct_pname: {
            int dst = it[1].u.operand;
            int base = it[2].u.operand;
            int propertyName = it[3].u.operand;
            int index = it[4].u.operand;
            int enumerator = it[5].u.operand;
            ValueProfile* profile = it[6].u.profile;
            printLocationAndOp(out, exec, location, it, "op_get_direct_pname");
            out.printf("%s, %s, %s, %s, %s, %p", registerName(dst).data(), registerName(base).data(), registerName(propertyName).data(), registerName(index).data(), registerName(enumerator).data(), profile);
            it += OPCODE_LENGTH(op_get_direct_pname) - 1;
            break;

        }
        case op_get_property_enumerator: {
            int dst = it[1].u.operand;
            int base = it[2].u.operand;
            printLocationAndOp(out, exec, location, it, "op_get_property_enumerator");
            out.printf("%s, %s", registerName(dst).data(), registerName(base).data());
            it += OPCODE_LENGTH(op_get_property_enumerator) - 1;
            break;
        }
        case op_enumerator_structure_pname: {
            int dst = it[1].u.operand;
            int enumerator = it[2].u.operand;
            int index = it[3].u.operand;
            printLocationAndOp(out, exec, location, it, "op_enumerator_structure_pname");
            out.printf("%s, %s, %s", registerName(dst).data(), registerName(enumerator).data(), registerName(index).data());
            it += OPCODE_LENGTH(op_enumerator_structure_pname) - 1;
            break;
        }
        case op_enumerator_generic_pname: {
            int dst = it[1].u.operand;
            int enumerator = it[2].u.operand;
            int index = it[3].u.operand;
            printLocationAndOp(out, exec, location, it, "op_enumerator_generic_pname");
            out.printf("%s, %s, %s", registerName(dst).data(), registerName(enumerator).data(), registerName(index).data());
            it += OPCODE_LENGTH(op_enumerator_generic_pname) - 1;
            break;
        }
        case op_to_index_string: {
            int dst = it[1].u.operand;
            int index = it[2].u.operand;
            printLocationAndOp(out, exec, location, it, "op_to_index_string");
            out.printf("%s, %s", registerName(dst).data(), registerName(index).data());
            it += OPCODE_LENGTH(op_to_index_string) - 1;
            break;
        }
        case op_push_with_scope: {
            int dst = (++it)->u.operand;
            int newScope = (++it)->u.operand;
            int currentScope = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "push_with_scope");
            out.printf("%s, %s, %s", registerName(dst).data(), registerName(newScope).data(), registerName(currentScope).data());
            break;
        }
        case op_get_parent_scope: {
            int dst = (++it)->u.operand;
            int parentScope = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "get_parent_scope");
            out.printf("%s, %s", registerName(dst).data(), registerName(parentScope).data());
            break;
        }
        case op_create_lexical_environment: {
            int dst = (++it)->u.operand;
            int scope = (++it)->u.operand;
            int symbolTable = (++it)->u.operand;
            int initialValue = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "create_lexical_environment");
            out.printf("%s, %s, %s, %s", 
                registerName(dst).data(), registerName(scope).data(), registerName(symbolTable).data(), registerName(initialValue).data());
            break;
        }
        case op_catch: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "catch");
            out.printf("%s, %s", registerName(r0).data(), registerName(r1).data());
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
            out.printf("%s, %s", constantName(k0).data(), k1 ? "true" : "false");
            break;
        }
        case op_debug: {
            int debugHookID = (++it)->u.operand;
            int hasBreakpointFlag = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "debug");
            out.printf("%s, %d", debugHookName(debugHookID), hasBreakpointFlag);
            break;
        }
        case op_save: {
            int generator = (++it)->u.operand;
            unsigned liveCalleeLocalsIndex = (++it)->u.unsignedValue;
            int offset = (++it)->u.operand;
            const FastBitVector& liveness = m_rareData->m_liveCalleeLocalsAtYield[liveCalleeLocalsIndex];
            printLocationAndOp(out, exec, location, it, "save");
            out.printf("%s, ", registerName(generator).data());
            liveness.dump(out);
            out.printf("(@live%1u), %d(->%d)", liveCalleeLocalsIndex, offset, location + offset);
            break;
        }
        case op_resume: {
            int generator = (++it)->u.operand;
            unsigned liveCalleeLocalsIndex = (++it)->u.unsignedValue;
            const FastBitVector& liveness = m_rareData->m_liveCalleeLocalsAtYield[liveCalleeLocalsIndex];
            printLocationAndOp(out, exec, location, it, "resume");
            out.printf("%s, ", registerName(generator).data());
            liveness.dump(out);
            out.printf("(@live%1u)", liveCalleeLocalsIndex);
            break;
        }
        case op_assert: {
            int condition = (++it)->u.operand;
            int line = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "assert");
            out.printf("%s, %d", registerName(condition).data(), line);
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
            int scope = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            ResolveType resolveType = static_cast<ResolveType>((++it)->u.operand);
            int depth = (++it)->u.operand;
            void* pointer = (++it)->u.pointer;
            printLocationAndOp(out, exec, location, it, "resolve_scope");
            out.printf("%s, %s, %s, <%s>, %d, %p", registerName(r0).data(), registerName(scope).data(), idName(id0, identifier(id0)).data(), resolveTypeName(resolveType), depth, pointer);
            break;
        }
        case op_get_from_scope: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            GetPutInfo getPutInfo = GetPutInfo((++it)->u.operand);
            ++it; // Structure
            int operand = (++it)->u.operand; // Operand
            printLocationAndOp(out, exec, location, it, "get_from_scope");
            out.print(registerName(r0), ", ", registerName(r1));
            if (static_cast<unsigned>(id0) == UINT_MAX)
                out.print(", anonymous");
            else
                out.print(", ", idName(id0, identifier(id0)));
            out.print(", ", getPutInfo.operand(), "<", resolveModeName(getPutInfo.resolveMode()), "|", resolveTypeName(getPutInfo.resolveType()), "|", initializationModeName(getPutInfo.initializationMode()), ">, ", operand);
            dumpValueProfiling(out, it, hasPrintedProfiling);
            break;
        }
        case op_put_to_scope: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            GetPutInfo getPutInfo = GetPutInfo((++it)->u.operand);
            ++it; // Structure
            int operand = (++it)->u.operand; // Operand
            printLocationAndOp(out, exec, location, it, "put_to_scope");
            out.print(registerName(r0));
            if (static_cast<unsigned>(id0) == UINT_MAX)
                out.print(", anonymous");
            else
                out.print(", ", idName(id0, identifier(id0)));
            out.print(", ", registerName(r1), ", ", getPutInfo.operand(), "<", resolveModeName(getPutInfo.resolveMode()), "|", resolveTypeName(getPutInfo.resolveType()), "|", initializationModeName(getPutInfo.initializationMode()), ">, <structure>, ", operand);
            break;
        }
        case op_get_from_arguments: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "get_from_arguments");
            out.printf("%s, %s, %d", registerName(r0).data(), registerName(r1).data(), offset);
            dumpValueProfiling(out, it, hasPrintedProfiling);
            break;
        }
        case op_put_to_arguments: {
            int r0 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printLocationAndOp(out, exec, location, it, "put_to_arguments");
            out.printf("%s, %d, %s", registerName(r0).data(), offset, registerName(r1).data());
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
    }

    dumpRareCaseProfile(out, "rare case: ", rareCaseProfileForBytecodeOffset(location), hasPrintedProfiling);
    dumpResultProfile(out, resultProfileForBytecodeOffset(location), hasPrintedProfiling);
    
#if ENABLE(DFG_JIT)
    Vector<DFG::FrequentExitSite> exitSites = exitProfile().exitSitesFor(location);
    if (!exitSites.isEmpty()) {
        out.print(" !! frequent exits: ");
        CommaPrinter comma;
        for (unsigned i = 0; i < exitSites.size(); ++i)
            out.print(comma, exitSites[i].kind(), " ", exitSites[i].jitType());
    }
#else // ENABLE(DFG_JIT)
    UNUSED_PARAM(location);
#endif // ENABLE(DFG_JIT)
    out.print("\n");
}

void CodeBlock::dumpBytecode(
    PrintStream& out, unsigned bytecodeOffset,
    const StubInfoMap& stubInfos, const CallLinkInfoMap& callLinkInfos)
{
    ExecState* exec = m_globalObject->globalExec();
    const Instruction* it = instructions().begin() + bytecodeOffset;
    dumpBytecode(out, exec, instructions().begin(), it, stubInfos, callLinkInfos);
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

namespace {

class PutToScopeFireDetail : public FireDetail {
public:
    PutToScopeFireDetail(CodeBlock* codeBlock, const Identifier& ident)
        : m_codeBlock(codeBlock)
        , m_ident(ident)
    {
    }
    
    void dump(PrintStream& out) const override
    {
        out.print("Linking put_to_scope in ", FunctionExecutableDump(jsCast<FunctionExecutable*>(m_codeBlock->ownerExecutable())), " for ", m_ident);
    }
    
private:
    CodeBlock* m_codeBlock;
    const Identifier& m_ident;
};

} // anonymous namespace

CodeBlock::CodeBlock(VM* vm, Structure* structure, CopyParsedBlockTag, CodeBlock& other)
    : JSCell(*vm, structure)
    , m_globalObject(other.m_globalObject)
    , m_numCalleeLocals(other.m_numCalleeLocals)
    , m_numVars(other.m_numVars)
    , m_shouldAlwaysBeInlined(true)
#if ENABLE(JIT)
    , m_capabilityLevelState(DFG::CapabilityLevelNotSet)
#endif
    , m_didFailFTLCompilation(false)
    , m_hasBeenCompiledWithFTL(false)
    , m_isConstructor(other.m_isConstructor)
    , m_isStrictMode(other.m_isStrictMode)
    , m_codeType(other.m_codeType)
    , m_unlinkedCode(*other.m_vm, this, other.m_unlinkedCode.get())
    , m_hasDebuggerStatement(false)
    , m_steppingMode(SteppingModeDisabled)
    , m_numBreakpoints(0)
    , m_ownerExecutable(*other.m_vm, this, other.m_ownerExecutable.get())
    , m_vm(other.m_vm)
    , m_instructions(other.m_instructions)
    , m_thisRegister(other.m_thisRegister)
    , m_scopeRegister(other.m_scopeRegister)
    , m_hash(other.m_hash)
    , m_source(other.m_source)
    , m_sourceOffset(other.m_sourceOffset)
    , m_firstLineColumnOffset(other.m_firstLineColumnOffset)
    , m_constantRegisters(other.m_constantRegisters)
    , m_constantsSourceCodeRepresentation(other.m_constantsSourceCodeRepresentation)
    , m_functionDecls(other.m_functionDecls)
    , m_functionExprs(other.m_functionExprs)
    , m_osrExitCounter(0)
    , m_optimizationDelayCounter(0)
    , m_reoptimizationRetryCounter(0)
    , m_creationTime(std::chrono::steady_clock::now())
{
    m_visitWeaklyHasBeenCalled.store(false, std::memory_order_relaxed);

    ASSERT(heap()->isDeferred());
    ASSERT(m_scopeRegister.isLocal());

    setNumParameters(other.numParameters());
}

void CodeBlock::finishCreation(VM& vm, CopyParsedBlockTag, CodeBlock& other)
{
    Base::finishCreation(vm);

    optimizeAfterWarmUp();
    jitAfterWarmUp();

    if (other.m_rareData) {
        createRareDataIfNecessary();
        
        m_rareData->m_exceptionHandlers = other.m_rareData->m_exceptionHandlers;
        m_rareData->m_constantBuffers = other.m_rareData->m_constantBuffers;
        m_rareData->m_switchJumpTables = other.m_rareData->m_switchJumpTables;
        m_rareData->m_stringSwitchJumpTables = other.m_rareData->m_stringSwitchJumpTables;
        m_rareData->m_liveCalleeLocalsAtYield = other.m_rareData->m_liveCalleeLocalsAtYield;
    }
    
    heap()->m_codeBlocks.add(this);
}

CodeBlock::CodeBlock(VM* vm, Structure* structure, ScriptExecutable* ownerExecutable, UnlinkedCodeBlock* unlinkedCodeBlock,
    JSScope* scope, PassRefPtr<SourceProvider> sourceProvider, unsigned sourceOffset, unsigned firstLineColumnOffset)
    : JSCell(*vm, structure)
    , m_globalObject(scope->globalObject()->vm(), this, scope->globalObject())
    , m_numCalleeLocals(unlinkedCodeBlock->m_numCalleeLocals)
    , m_numVars(unlinkedCodeBlock->m_numVars)
    , m_shouldAlwaysBeInlined(true)
#if ENABLE(JIT)
    , m_capabilityLevelState(DFG::CapabilityLevelNotSet)
#endif
    , m_didFailFTLCompilation(false)
    , m_hasBeenCompiledWithFTL(false)
    , m_isConstructor(unlinkedCodeBlock->isConstructor())
    , m_isStrictMode(unlinkedCodeBlock->isStrictMode())
    , m_codeType(unlinkedCodeBlock->codeType())
    , m_unlinkedCode(m_globalObject->vm(), this, unlinkedCodeBlock)
    , m_hasDebuggerStatement(false)
    , m_steppingMode(SteppingModeDisabled)
    , m_numBreakpoints(0)
    , m_ownerExecutable(m_globalObject->vm(), this, ownerExecutable)
    , m_vm(unlinkedCodeBlock->vm())
    , m_thisRegister(unlinkedCodeBlock->thisRegister())
    , m_scopeRegister(unlinkedCodeBlock->scopeRegister())
    , m_source(sourceProvider)
    , m_sourceOffset(sourceOffset)
    , m_firstLineColumnOffset(firstLineColumnOffset)
    , m_osrExitCounter(0)
    , m_optimizationDelayCounter(0)
    , m_reoptimizationRetryCounter(0)
    , m_creationTime(std::chrono::steady_clock::now())
{
    m_visitWeaklyHasBeenCalled.store(false, std::memory_order_relaxed);

    ASSERT(heap()->isDeferred());
    ASSERT(m_scopeRegister.isLocal());

    ASSERT(m_source);
    setNumParameters(unlinkedCodeBlock->numParameters());
}

void CodeBlock::finishCreation(VM& vm, ScriptExecutable* ownerExecutable, UnlinkedCodeBlock* unlinkedCodeBlock,
    JSScope* scope)
{
    Base::finishCreation(vm);

    if (vm.typeProfiler() || vm.controlFlowProfiler())
        vm.functionHasExecutedCache()->removeUnexecutedRange(ownerExecutable->sourceID(), ownerExecutable->typeProfilingStartOffset(), ownerExecutable->typeProfilingEndOffset());

    setConstantRegisters(unlinkedCodeBlock->constantRegisters(), unlinkedCodeBlock->constantsSourceCodeRepresentation());
    if (unlinkedCodeBlock->usesGlobalObject())
        m_constantRegisters[unlinkedCodeBlock->globalObjectRegister().toConstantIndex()].set(*m_vm, this, m_globalObject.get());

    for (unsigned i = 0; i < LinkTimeConstantCount; i++) {
        LinkTimeConstant type = static_cast<LinkTimeConstant>(i);
        if (unsigned registerIndex = unlinkedCodeBlock->registerIndexForLinkTimeConstant(type))
            m_constantRegisters[registerIndex].set(*m_vm, this, m_globalObject->jsCellForLinkTimeConstant(type));
    }

#if !ASSERT_DISABLED
    HashSet<int, WTF::IntHash<int>, WTF::UnsignedWithZeroKeyHashTraits<int>> clonedConstantSymbolTables;
#endif
    {
#if !ASSERT_DISABLED
        HashSet<SymbolTable*> clonedSymbolTables;
#endif
        bool hasTypeProfiler = !!vm.typeProfiler();
        for (unsigned i = 0; i < m_constantRegisters.size(); i++) {
            if (m_constantRegisters[i].get().isEmpty())
                continue;
            if (SymbolTable* symbolTable = jsDynamicCast<SymbolTable*>(m_constantRegisters[i].get())) {
                ASSERT(clonedSymbolTables.add(symbolTable).isNewEntry);
                if (hasTypeProfiler) {
                    ConcurrentJITLocker locker(symbolTable->m_lock);
                    symbolTable->prepareForTypeProfiling(locker);
                }
                m_constantRegisters[i].set(*m_vm, this, symbolTable->cloneScopePart(*m_vm));
#if !ASSERT_DISABLED
                clonedConstantSymbolTables.add(i + FirstConstantRegisterIndex);
#endif
            }
        }
    }

    // We already have the cloned symbol table for the module environment since we need to instantiate
    // the module environments before linking the code block. We replace the stored symbol table with the already cloned one.
    if (UnlinkedModuleProgramCodeBlock* unlinkedModuleProgramCodeBlock = jsDynamicCast<UnlinkedModuleProgramCodeBlock*>(unlinkedCodeBlock)) {
        SymbolTable* clonedSymbolTable = jsCast<ModuleProgramExecutable*>(ownerExecutable)->moduleEnvironmentSymbolTable();
        if (m_vm->typeProfiler()) {
            ConcurrentJITLocker locker(clonedSymbolTable->m_lock);
            clonedSymbolTable->prepareForTypeProfiling(locker);
        }
        replaceConstant(unlinkedModuleProgramCodeBlock->moduleEnvironmentSymbolTableConstantRegisterOffset(), clonedSymbolTable);
    }

    bool shouldUpdateFunctionHasExecutedCache = vm.typeProfiler() || vm.controlFlowProfiler();
    m_functionDecls = RefCountedArray<WriteBarrier<FunctionExecutable>>(unlinkedCodeBlock->numberOfFunctionDecls());
    for (size_t count = unlinkedCodeBlock->numberOfFunctionDecls(), i = 0; i < count; ++i) {
        UnlinkedFunctionExecutable* unlinkedExecutable = unlinkedCodeBlock->functionDecl(i);
        if (shouldUpdateFunctionHasExecutedCache)
            vm.functionHasExecutedCache()->insertUnexecutedRange(ownerExecutable->sourceID(), unlinkedExecutable->typeProfilingStartOffset(), unlinkedExecutable->typeProfilingEndOffset());
        m_functionDecls[i].set(*m_vm, this, unlinkedExecutable->link(*m_vm, ownerExecutable->source()));
    }

    m_functionExprs = RefCountedArray<WriteBarrier<FunctionExecutable>>(unlinkedCodeBlock->numberOfFunctionExprs());
    for (size_t count = unlinkedCodeBlock->numberOfFunctionExprs(), i = 0; i < count; ++i) {
        UnlinkedFunctionExecutable* unlinkedExecutable = unlinkedCodeBlock->functionExpr(i);
        if (shouldUpdateFunctionHasExecutedCache)
            vm.functionHasExecutedCache()->insertUnexecutedRange(ownerExecutable->sourceID(), unlinkedExecutable->typeProfilingStartOffset(), unlinkedExecutable->typeProfilingEndOffset());
        m_functionExprs[i].set(*m_vm, this, unlinkedExecutable->link(*m_vm, ownerExecutable->source()));
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
            for (size_t i = 0; i < count; i++) {
                const UnlinkedHandlerInfo& unlinkedHandler = unlinkedCodeBlock->exceptionHandler(i);
                HandlerInfo& handler = m_rareData->m_exceptionHandlers[i];
#if ENABLE(JIT)
                handler.initialize(unlinkedHandler, CodeLocationLabel(MacroAssemblerCodePtr::createFromExecutableAddress(LLInt::getCodePtr(op_catch))));
#else
                handler.initialize(unlinkedHandler);
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
    if (size_t size = unlinkedCodeBlock->numberOfLLintCallLinkInfos())
        m_llintCallLinkInfos = RefCountedArray<LLIntCallLinkInfo>(size);
    if (size_t size = unlinkedCodeBlock->numberOfArrayProfiles())
        m_arrayProfiles.grow(size);
    if (size_t size = unlinkedCodeBlock->numberOfArrayAllocationProfiles())
        m_arrayAllocationProfiles = RefCountedArray<ArrayAllocationProfile>(size);
    if (size_t size = unlinkedCodeBlock->numberOfValueProfiles())
        m_valueProfiles = RefCountedArray<ValueProfile>(size);
    if (size_t size = unlinkedCodeBlock->numberOfObjectAllocationProfiles())
        m_objectAllocationProfiles = RefCountedArray<ObjectAllocationProfile>(size);

#if ENABLE(JIT)
    setCalleeSaveRegisters(RegisterSet::llintBaselineCalleeSaveRegisters());
#endif

    // Copy and translate the UnlinkedInstructions
    unsigned instructionCount = unlinkedCodeBlock->instructions().count();
    UnlinkedInstructionStream::Reader instructionReader(unlinkedCodeBlock->instructions());

    // Bookkeep the strongly referenced module environments.
    HashSet<JSModuleEnvironment*> stronglyReferencedModuleEnvironments;

    // Bookkeep the merge point bytecode offsets.
    Vector<size_t> mergePointBytecodeOffsets;

    RefCountedArray<Instruction> instructions(instructionCount);

    for (unsigned i = 0; !instructionReader.atEnd(); ) {
        const UnlinkedInstruction* pc = instructionReader.next();

        unsigned opLength = opcodeLength(pc[0].u.opcode);

        instructions[i] = vm.interpreter->getOpcode(pc[0].u.opcode);
        for (size_t j = 1; j < opLength; ++j) {
            if (sizeof(int32_t) != sizeof(intptr_t))
                instructions[i + j].u.pointer = 0;
            instructions[i + j].u.operand = pc[j].u.operand;
        }
        switch (pc[0].u.opcode) {
        case op_has_indexed_property: {
            int arrayProfileIndex = pc[opLength - 1].u.operand;
            m_arrayProfiles[arrayProfileIndex] = ArrayProfile(i);

            instructions[i + opLength - 1] = &m_arrayProfiles[arrayProfileIndex];
            break;
        }
        case op_call_varargs:
        case op_tail_call_varargs:
        case op_construct_varargs:
        case op_get_by_val: {
            int arrayProfileIndex = pc[opLength - 2].u.operand;
            m_arrayProfiles[arrayProfileIndex] = ArrayProfile(i);

            instructions[i + opLength - 2] = &m_arrayProfiles[arrayProfileIndex];
            FALLTHROUGH;
        }
        case op_get_direct_pname:
        case op_get_by_id:
        case op_get_from_arguments: {
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
            objectAllocationProfile->initialize(vm,
                this, m_globalObject->objectPrototype(), inferredInlineCapacity);
            break;
        }

        case op_call:
        case op_tail_call:
        case op_call_eval: {
            ValueProfile* profile = &m_valueProfiles[pc[opLength - 1].u.operand];
            ASSERT(profile->m_bytecodeOffset == -1);
            profile->m_bytecodeOffset = i;
            instructions[i + opLength - 1] = profile;
            int arrayProfileIndex = pc[opLength - 2].u.operand;
            m_arrayProfiles[arrayProfileIndex] = ArrayProfile(i);
            instructions[i + opLength - 2] = &m_arrayProfiles[arrayProfileIndex];
            instructions[i + 5] = &m_llintCallLinkInfos[pc[5].u.operand];
            break;
        }
        case op_construct: {
            instructions[i + 5] = &m_llintCallLinkInfos[pc[5].u.operand];
            ValueProfile* profile = &m_valueProfiles[pc[opLength - 1].u.operand];
            ASSERT(profile->m_bytecodeOffset == -1);
            profile->m_bytecodeOffset = i;
            instructions[i + opLength - 1] = profile;
            break;
        }
        case op_get_array_length:
            CRASH();

#if !ASSERT_DISABLED
        case op_create_lexical_environment: {
            int symbolTableIndex = pc[3].u.operand;
            ASSERT(clonedConstantSymbolTables.contains(symbolTableIndex));
            break;
        }
#endif

        case op_resolve_scope: {
            const Identifier& ident = identifier(pc[3].u.operand);
            ResolveType type = static_cast<ResolveType>(pc[4].u.operand);
            RELEASE_ASSERT(type != LocalClosureVar);
            int localScopeDepth = pc[5].u.operand;

            ResolveOp op = JSScope::abstractResolve(m_globalObject->globalExec(), localScopeDepth, scope, ident, Get, type, NotInitialization);
            instructions[i + 4].u.operand = op.type;
            instructions[i + 5].u.operand = op.depth;
            if (op.lexicalEnvironment) {
                if (op.type == ModuleVar) {
                    // Keep the linked module environment strongly referenced.
                    if (stronglyReferencedModuleEnvironments.add(jsCast<JSModuleEnvironment*>(op.lexicalEnvironment)).isNewEntry)
                        addConstant(op.lexicalEnvironment);
                    instructions[i + 6].u.jsCell.set(vm, this, op.lexicalEnvironment);
                } else
                    instructions[i + 6].u.symbolTable.set(vm, this, op.lexicalEnvironment->symbolTable());
            } else if (JSScope* constantScope = JSScope::constantScopeForCodeBlock(op.type, this))
                instructions[i + 6].u.jsCell.set(vm, this, constantScope);
            else
                instructions[i + 6].u.pointer = nullptr;
            break;
        }

        case op_get_from_scope: {
            ValueProfile* profile = &m_valueProfiles[pc[opLength - 1].u.operand];
            ASSERT(profile->m_bytecodeOffset == -1);
            profile->m_bytecodeOffset = i;
            instructions[i + opLength - 1] = profile;

            // get_from_scope dst, scope, id, GetPutInfo, Structure, Operand

            int localScopeDepth = pc[5].u.operand;
            instructions[i + 5].u.pointer = nullptr;

            GetPutInfo getPutInfo = GetPutInfo(pc[4].u.operand);
            ASSERT(getPutInfo.initializationMode() == NotInitialization);
            if (getPutInfo.resolveType() == LocalClosureVar) {
                instructions[i + 4] = GetPutInfo(getPutInfo.resolveMode(), ClosureVar, getPutInfo.initializationMode()).operand();
                break;
            }

            const Identifier& ident = identifier(pc[3].u.operand);
            ResolveOp op = JSScope::abstractResolve(m_globalObject->globalExec(), localScopeDepth, scope, ident, Get, getPutInfo.resolveType(), NotInitialization);

            instructions[i + 4].u.operand = GetPutInfo(getPutInfo.resolveMode(), op.type, getPutInfo.initializationMode()).operand();
            if (op.type == ModuleVar)
                instructions[i + 4].u.operand = GetPutInfo(getPutInfo.resolveMode(), ClosureVar, getPutInfo.initializationMode()).operand();
            if (op.type == GlobalVar || op.type == GlobalVarWithVarInjectionChecks || op.type == GlobalLexicalVar || op.type == GlobalLexicalVarWithVarInjectionChecks)
                instructions[i + 5].u.watchpointSet = op.watchpointSet;
            else if (op.structure)
                instructions[i + 5].u.structure.set(vm, this, op.structure);
            instructions[i + 6].u.pointer = reinterpret_cast<void*>(op.operand);
            break;
        }

        case op_put_to_scope: {
            // put_to_scope scope, id, value, GetPutInfo, Structure, Operand
            GetPutInfo getPutInfo = GetPutInfo(pc[4].u.operand);
            if (getPutInfo.resolveType() == LocalClosureVar) {
                // Only do watching if the property we're putting to is not anonymous.
                if (static_cast<unsigned>(pc[2].u.operand) != UINT_MAX) {
                    int symbolTableIndex = pc[5].u.operand;
                    ASSERT(clonedConstantSymbolTables.contains(symbolTableIndex));
                    SymbolTable* symbolTable = jsCast<SymbolTable*>(getConstant(symbolTableIndex));
                    const Identifier& ident = identifier(pc[2].u.operand);
                    ConcurrentJITLocker locker(symbolTable->m_lock);
                    auto iter = symbolTable->find(locker, ident.impl());
                    ASSERT(iter != symbolTable->end(locker));
                    iter->value.prepareToWatch();
                    instructions[i + 5].u.watchpointSet = iter->value.watchpointSet();
                } else
                    instructions[i + 5].u.watchpointSet = nullptr;
                break;
            }

            const Identifier& ident = identifier(pc[2].u.operand);
            int localScopeDepth = pc[5].u.operand;
            instructions[i + 5].u.pointer = nullptr;
            ResolveOp op = JSScope::abstractResolve(m_globalObject->globalExec(), localScopeDepth, scope, ident, Put, getPutInfo.resolveType(), getPutInfo.initializationMode());

            instructions[i + 4].u.operand = GetPutInfo(getPutInfo.resolveMode(), op.type, getPutInfo.initializationMode()).operand();
            if (op.type == GlobalVar || op.type == GlobalVarWithVarInjectionChecks || op.type == GlobalLexicalVar || op.type == GlobalLexicalVarWithVarInjectionChecks)
                instructions[i + 5].u.watchpointSet = op.watchpointSet;
            else if (op.type == ClosureVar || op.type == ClosureVarWithVarInjectionChecks) {
                if (op.watchpointSet)
                    op.watchpointSet->invalidate(PutToScopeFireDetail(this, ident));
            } else if (op.structure)
                instructions[i + 5].u.structure.set(vm, this, op.structure);
            instructions[i + 6].u.pointer = reinterpret_cast<void*>(op.operand);

            break;
        }

        case op_profile_type: {
            RELEASE_ASSERT(vm.typeProfiler());
            // The format of this instruction is: op_profile_type regToProfile, TypeLocation*, flag, identifier?, resolveType?
            size_t instructionOffset = i + opLength - 1;
            unsigned divotStart, divotEnd;
            GlobalVariableID globalVariableID = 0;
            RefPtr<TypeSet> globalTypeSet;
            bool shouldAnalyze = m_unlinkedCode->typeProfilerExpressionInfoForBytecodeOffset(instructionOffset, divotStart, divotEnd);
            VirtualRegister profileRegister(pc[1].u.operand);
            ProfileTypeBytecodeFlag flag = static_cast<ProfileTypeBytecodeFlag>(pc[3].u.operand);
            SymbolTable* symbolTable = nullptr;

            switch (flag) {
            case ProfileTypeBytecodeClosureVar: {
                const Identifier& ident = identifier(pc[4].u.operand);
                int localScopeDepth = pc[2].u.operand;
                ResolveType type = static_cast<ResolveType>(pc[5].u.operand);
                // Even though type profiling may be profiling either a Get or a Put, we can always claim a Get because
                // we're abstractly "read"ing from a JSScope.
                ResolveOp op = JSScope::abstractResolve(m_globalObject->globalExec(), localScopeDepth, scope, ident, Get, type, NotInitialization);

                if (op.type == ClosureVar || op.type == ModuleVar)
                    symbolTable = op.lexicalEnvironment->symbolTable();
                else if (op.type == GlobalVar)
                    symbolTable = m_globalObject.get()->symbolTable();

                UniquedStringImpl* impl = (op.type == ModuleVar) ? op.importedName.get() : ident.impl();
                if (symbolTable) {
                    ConcurrentJITLocker locker(symbolTable->m_lock);
                    // If our parent scope was created while profiling was disabled, it will not have prepared for profiling yet.
                    symbolTable->prepareForTypeProfiling(locker);
                    globalVariableID = symbolTable->uniqueIDForVariable(locker, impl, vm);
                    globalTypeSet = symbolTable->globalTypeSetForVariable(locker, impl, vm);
                } else
                    globalVariableID = TypeProfilerNoGlobalIDExists;

                break;
            }
            case ProfileTypeBytecodeLocallyResolved: {
                int symbolTableIndex = pc[2].u.operand;
                ASSERT(clonedConstantSymbolTables.contains(symbolTableIndex));
                SymbolTable* symbolTable = jsCast<SymbolTable*>(getConstant(symbolTableIndex));
                const Identifier& ident = identifier(pc[4].u.operand);
                ConcurrentJITLocker locker(symbolTable->m_lock);
                // If our parent scope was created while profiling was disabled, it will not have prepared for profiling yet.
                globalVariableID = symbolTable->uniqueIDForVariable(locker, ident.impl(), vm);
                globalTypeSet = symbolTable->globalTypeSetForVariable(locker, ident.impl(), vm);

                break;
            }
            case ProfileTypeBytecodeDoesNotHaveGlobalID: 
            case ProfileTypeBytecodeFunctionArgument: {
                globalVariableID = TypeProfilerNoGlobalIDExists;
                break;
            }
            case ProfileTypeBytecodeFunctionReturnStatement: {
                RELEASE_ASSERT(ownerExecutable->isFunctionExecutable());
                globalTypeSet = jsCast<FunctionExecutable*>(ownerExecutable)->returnStatementTypeSet();
                globalVariableID = TypeProfilerReturnStatement;
                if (!shouldAnalyze) {
                    // Because a return statement can be added implicitly to return undefined at the end of a function,
                    // and these nodes don't emit expression ranges because they aren't in the actual source text of
                    // the user's program, give the type profiler some range to identify these return statements.
                    // Currently, the text offset that is used as identification is "f" in the function keyword
                    // and is stored on TypeLocation's m_divotForFunctionOffsetIfReturnStatement member variable.
                    divotStart = divotEnd = ownerExecutable->typeProfilingStartOffset();
                    shouldAnalyze = true;
                }
                break;
            }
            }

            std::pair<TypeLocation*, bool> locationPair = vm.typeProfiler()->typeLocationCache()->getTypeLocation(globalVariableID,
                ownerExecutable->sourceID(), divotStart, divotEnd, globalTypeSet, &vm);
            TypeLocation* location = locationPair.first;
            bool isNewLocation = locationPair.second;

            if (flag == ProfileTypeBytecodeFunctionReturnStatement)
                location->m_divotForFunctionOffsetIfReturnStatement = ownerExecutable->typeProfilingStartOffset();

            if (shouldAnalyze && isNewLocation)
                vm.typeProfiler()->insertNewLocation(location);

            instructions[i + 2].u.location = location;
            break;
        }

        case op_debug: {
            if (pc[1].u.index == DidReachBreakpoint)
                m_hasDebuggerStatement = true;
            break;
        }

        case op_save: {
            unsigned liveCalleeLocalsIndex = pc[2].u.index;
            int offset = pc[3].u.operand;
            if (liveCalleeLocalsIndex >= mergePointBytecodeOffsets.size())
                mergePointBytecodeOffsets.resize(liveCalleeLocalsIndex + 1);
            mergePointBytecodeOffsets[liveCalleeLocalsIndex] = i + offset;
            break;
        }

        default:
            break;
        }
        i += opLength;
    }

    if (vm.controlFlowProfiler())
        insertBasicBlockBoundariesForControlFlowProfiler(instructions);

    m_instructions = WTFMove(instructions);

    // Perform bytecode liveness analysis to determine which locals are live and should be resumed when executing op_resume.
    if (unlinkedCodeBlock->parseMode() == SourceParseMode::GeneratorBodyMode) {
        if (size_t count = mergePointBytecodeOffsets.size()) {
            createRareDataIfNecessary();
            BytecodeLivenessAnalysis liveness(this);
            m_rareData->m_liveCalleeLocalsAtYield.grow(count);
            size_t liveCalleeLocalsIndex = 0;
            for (size_t bytecodeOffset : mergePointBytecodeOffsets) {
                m_rareData->m_liveCalleeLocalsAtYield[liveCalleeLocalsIndex] = liveness.getLivenessInfoAtBytecodeOffset(bytecodeOffset);
                ++liveCalleeLocalsIndex;
            }
        }
    }

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
    
    heap()->m_codeBlocks.add(this);
    heap()->reportExtraMemoryAllocated(m_instructions.size() * sizeof(Instruction));
}

#if ENABLE(WEBASSEMBLY)
CodeBlock::CodeBlock(VM* vm, Structure* structure, WebAssemblyExecutable* ownerExecutable, JSGlobalObject* globalObject)
    : JSCell(*vm, structure)
    , m_globalObject(globalObject->vm(), this, globalObject)
    , m_numCalleeLocals(0)
    , m_numVars(0)
    , m_shouldAlwaysBeInlined(false)
#if ENABLE(JIT)
    , m_capabilityLevelState(DFG::CannotCompile)
#endif
    , m_didFailFTLCompilation(false)
    , m_hasBeenCompiledWithFTL(false)
    , m_isConstructor(false)
    , m_isStrictMode(false)
    , m_codeType(FunctionCode)
    , m_hasDebuggerStatement(false)
    , m_steppingMode(SteppingModeDisabled)
    , m_numBreakpoints(0)
    , m_ownerExecutable(m_globalObject->vm(), this, ownerExecutable)
    , m_vm(vm)
    , m_osrExitCounter(0)
    , m_optimizationDelayCounter(0)
    , m_reoptimizationRetryCounter(0)
    , m_creationTime(std::chrono::steady_clock::now())
{
    ASSERT(heap()->isDeferred());
}

void CodeBlock::finishCreation(VM& vm, WebAssemblyExecutable*, JSGlobalObject*)
{
    Base::finishCreation(vm);

    heap()->m_codeBlocks.add(this);
}
#endif

CodeBlock::~CodeBlock()
{
    if (m_vm->m_perBytecodeProfiler)
        m_vm->m_perBytecodeProfiler->notifyDestruction(this);
    
#if ENABLE(VERBOSE_VALUE_PROFILE)
    dumpValueProfiles();
#endif

    // We may be destroyed before any CodeBlocks that refer to us are destroyed.
    // Consider that two CodeBlocks become unreachable at the same time. There
    // is no guarantee about the order in which the CodeBlocks are destroyed.
    // So, if we don't remove incoming calls, and get destroyed before the
    // CodeBlock(s) that have calls into us, then the CallLinkInfo vector's
    // destructor will try to remove nodes from our (no longer valid) linked list.
    unlinkIncomingCalls();
    
    // Note that our outgoing calls will be removed from other CodeBlocks'
    // m_incomingCalls linked lists through the execution of the ~CallLinkInfo
    // destructors.

#if ENABLE(JIT)
    for (Bag<StructureStubInfo>::iterator iter = m_stubInfos.begin(); !!iter; ++iter) {
        StructureStubInfo* stub = *iter;
        stub->aboutToDie();
        stub->deref();
    }
#endif // ENABLE(JIT)
}

void CodeBlock::setAlternative(VM& vm, CodeBlock* alternative)
{
    m_alternative.set(vm, this, alternative);
}

void CodeBlock::setNumParameters(int newValue)
{
    m_numParameters = newValue;

    m_argumentValueProfiles = RefCountedArray<ValueProfile>(newValue);
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
    return jitCode->osrEntryBlock();
#else // ENABLE(FTL_JIT)
    return 0;
#endif // ENABLE(FTL_JIT)
}

void CodeBlock::visitWeakly(SlotVisitor& visitor)
{
    bool setByMe = m_visitWeaklyHasBeenCalled.compareExchangeStrong(false, true);
    if (!setByMe)
        return;

    if (Heap::isMarked(this))
        return;

    if (shouldVisitStrongly()) {
        visitor.appendUnbarrieredReadOnlyPointer(this);
        return;
    }

    // There are two things that may use unconditional finalizers: inline cache clearing
    // and jettisoning. The probability of us wanting to do at least one of those things
    // is probably quite close to 1. So we add one no matter what and when it runs, it
    // figures out whether it has any work to do.
    visitor.addUnconditionalFinalizer(&m_unconditionalFinalizer);

    if (!JITCode::isOptimizingJIT(jitType()))
        return;

    // If we jettison ourselves we'll install our alternative, so make sure that it
    // survives GC even if we don't.
    visitor.append(&m_alternative);
    
    // There are two things that we use weak reference harvesters for: DFG fixpoint for
    // jettisoning, and trying to find structures that would be live based on some
    // inline cache. So it makes sense to register them regardless.
    visitor.addWeakReferenceHarvester(&m_weakReferenceHarvester);

#if ENABLE(DFG_JIT)
    // We get here if we're live in the sense that our owner executable is live,
    // but we're not yet live for sure in another sense: we may yet decide that this
    // code block should be jettisoned based on its outgoing weak references being
    // stale. Set a flag to indicate that we're still assuming that we're dead, and
    // perform one round of determining if we're live. The GC may determine, based on
    // either us marking additional objects, or by other objects being marked for
    // other reasons, that this iteration should run again; it will notify us of this
    // decision by calling harvestWeakReferences().

    m_allTransitionsHaveBeenMarked = false;
    propagateTransitions(visitor);

    m_jitCode->dfgCommon()->livenessHasBeenProved = false;
    determineLiveness(visitor);
#endif // ENABLE(DFG_JIT)
}

size_t CodeBlock::estimatedSize(JSCell* cell)
{
    CodeBlock* thisObject = jsCast<CodeBlock*>(cell);
    size_t extraMemoryAllocated = thisObject->m_instructions.size() * sizeof(Instruction);
    if (thisObject->m_jitCode)
        extraMemoryAllocated += thisObject->m_jitCode->size();
    return Base::estimatedSize(cell) + extraMemoryAllocated;
}

void CodeBlock::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    CodeBlock* thisObject = jsCast<CodeBlock*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    JSCell::visitChildren(thisObject, visitor);
    thisObject->visitChildren(visitor);
}

void CodeBlock::visitChildren(SlotVisitor& visitor)
{
    // There are two things that may use unconditional finalizers: inline cache clearing
    // and jettisoning. The probability of us wanting to do at least one of those things
    // is probably quite close to 1. So we add one no matter what and when it runs, it
    // figures out whether it has any work to do.
    visitor.addUnconditionalFinalizer(&m_unconditionalFinalizer);

    if (CodeBlock* otherBlock = specialOSREntryBlockOrNull())
        visitor.appendUnbarrieredReadOnlyPointer(otherBlock);

    if (m_jitCode)
        visitor.reportExtraMemoryVisited(m_jitCode->size());
    if (m_instructions.size())
        visitor.reportExtraMemoryVisited(m_instructions.size() * sizeof(Instruction) / m_instructions.refCount());

    stronglyVisitStrongReferences(visitor);
    stronglyVisitWeakReferences(visitor);

    m_allTransitionsHaveBeenMarked = false;
    propagateTransitions(visitor);
}

bool CodeBlock::shouldVisitStrongly()
{
    if (Options::forceCodeBlockLiveness())
        return true;

    if (shouldJettisonDueToOldAge())
        return false;

    // Interpreter and Baseline JIT CodeBlocks don't need to be jettisoned when
    // their weak references go stale. So if a basline JIT CodeBlock gets
    // scanned, we can assume that this means that it's live.
    if (!JITCode::isOptimizingJIT(jitType()))
        return true;

    return false;
}

bool CodeBlock::shouldJettisonDueToWeakReference()
{
    if (!JITCode::isOptimizingJIT(jitType()))
        return false;
    return !Heap::isMarked(this);
}

bool CodeBlock::shouldJettisonDueToOldAge()
{
    return false;
}

#if ENABLE(DFG_JIT)
static bool shouldMarkTransition(DFG::WeakReferenceTransition& transition)
{
    if (transition.m_codeOrigin && !Heap::isMarked(transition.m_codeOrigin.get()))
        return false;
    
    if (!Heap::isMarked(transition.m_from.get()))
        return false;
    
    return true;
}
#endif // ENABLE(DFG_JIT)

void CodeBlock::propagateTransitions(SlotVisitor& visitor)
{
    UNUSED_PARAM(visitor);

    if (m_allTransitionsHaveBeenMarked)
        return;

    bool allAreMarkedSoFar = true;
        
    Interpreter* interpreter = m_vm->interpreter;
    if (jitType() == JITCode::InterpreterThunk) {
        const Vector<unsigned>& propertyAccessInstructions = m_unlinkedCode->propertyAccessInstructions();
        for (size_t i = 0; i < propertyAccessInstructions.size(); ++i) {
            Instruction* instruction = &instructions()[propertyAccessInstructions[i]];
            switch (interpreter->getOpcodeID(instruction[0].u.opcode)) {
            case op_put_by_id: {
                StructureID oldStructureID = instruction[4].u.structureID;
                StructureID newStructureID = instruction[6].u.structureID;
                if (!oldStructureID || !newStructureID)
                    break;
                Structure* oldStructure =
                    m_vm->heap.structureIDTable().get(oldStructureID);
                Structure* newStructure =
                    m_vm->heap.structureIDTable().get(newStructureID);
                if (Heap::isMarked(oldStructure))
                    visitor.appendUnbarrieredReadOnlyPointer(newStructure);
                else
                    allAreMarkedSoFar = false;
                break;
            }
            default:
                break;
            }
        }
    }

#if ENABLE(JIT)
    if (JITCode::isJIT(jitType())) {
        for (Bag<StructureStubInfo>::iterator iter = m_stubInfos.begin(); !!iter; ++iter) {
            StructureStubInfo& stubInfo = **iter;
            if (stubInfo.cacheType != CacheType::Stub)
                continue;
            PolymorphicAccess* list = stubInfo.u.stub;
            JSCell* origin = stubInfo.codeOrigin.codeOriginOwner();
            if (origin && !Heap::isMarked(origin)) {
                allAreMarkedSoFar = false;
                continue;
            }
            for (unsigned j = list->size(); j--;) {
                const AccessCase& access = list->at(j);
                if (access.type() != AccessCase::Transition)
                    continue;
                if (Heap::isMarked(access.structure()))
                    visitor.appendUnbarrieredReadOnlyPointer(access.newStructure());
                else
                    allAreMarkedSoFar = false;
            }
        }
    }
#endif // ENABLE(JIT)
    
#if ENABLE(DFG_JIT)
    if (JITCode::isOptimizingJIT(jitType())) {
        DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
        
        for (unsigned i = 0; i < dfgCommon->transitions.size(); ++i) {
            if (shouldMarkTransition(dfgCommon->transitions[i])) {
                // If the following three things are live, then the target of the
                // transition is also live:
                //
                // - This code block. We know it's live already because otherwise
                //   we wouldn't be scanning ourselves.
                //
                // - The code origin of the transition. Transitions may arise from
                //   code that was inlined. They are not relevant if the user's
                //   object that is required for the inlinee to run is no longer
                //   live.
                //
                // - The source of the transition. The transition checks if some
                //   heap location holds the source, and if so, stores the target.
                //   Hence the source must be live for the transition to be live.
                //
                // We also short-circuit the liveness if the structure is harmless
                // to mark (i.e. its global object and prototype are both already
                // live).
                
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
    if (allAreLiveSoFar) {
        for (unsigned i = 0; i < dfgCommon->weakStructureReferences.size(); ++i) {
            if (!Heap::isMarked(dfgCommon->weakStructureReferences[i].get())) {
                allAreLiveSoFar = false;
                break;
            }
        }
    }
    
    // If some weak references are dead, then this fixpoint iteration was
    // unsuccessful.
    if (!allAreLiveSoFar)
        return;
    
    // All weak references are live. Record this information so we don't
    // come back here again, and scan the strong references.
    dfgCommon->livenessHasBeenProved = true;
    visitor.appendUnbarrieredReadOnlyPointer(this);
#endif // ENABLE(DFG_JIT)
}

void CodeBlock::WeakReferenceHarvester::visitWeakReferences(SlotVisitor& visitor)
{
    CodeBlock* codeBlock =
        bitwise_cast<CodeBlock*>(
            bitwise_cast<char*>(this) - OBJECT_OFFSETOF(CodeBlock, m_weakReferenceHarvester));

    codeBlock->propagateTransitions(visitor);
    codeBlock->determineLiveness(visitor);
}

void CodeBlock::finalizeLLIntInlineCaches()
{
#if ENABLE(WEBASSEMBLY)
    if (m_ownerExecutable->isWebAssemblyExecutable())
        return;
#endif

    Interpreter* interpreter = m_vm->interpreter;
    const Vector<unsigned>& propertyAccessInstructions = m_unlinkedCode->propertyAccessInstructions();
    for (size_t size = propertyAccessInstructions.size(), i = 0; i < size; ++i) {
        Instruction* curInstruction = &instructions()[propertyAccessInstructions[i]];
        switch (interpreter->getOpcodeID(curInstruction[0].u.opcode)) {
        case op_get_by_id: {
            StructureID oldStructureID = curInstruction[4].u.structureID;
            if (!oldStructureID || Heap::isMarked(m_vm->heap.structureIDTable().get(oldStructureID)))
                break;
            if (Options::verboseOSR())
                dataLogF("Clearing LLInt property access.\n");
            curInstruction[4].u.structureID = 0;
            curInstruction[5].u.operand = 0;
            break;
        }
        case op_put_by_id: {
            StructureID oldStructureID = curInstruction[4].u.structureID;
            StructureID newStructureID = curInstruction[6].u.structureID;
            StructureChain* chain = curInstruction[7].u.structureChain.get();
            if ((!oldStructureID || Heap::isMarked(m_vm->heap.structureIDTable().get(oldStructureID))) &&
                (!newStructureID || Heap::isMarked(m_vm->heap.structureIDTable().get(newStructureID))) &&
                (!chain || Heap::isMarked(chain)))
                break;
            if (Options::verboseOSR())
                dataLogF("Clearing LLInt put transition.\n");
            curInstruction[4].u.structureID = 0;
            curInstruction[5].u.operand = 0;
            curInstruction[6].u.structureID = 0;
            curInstruction[7].u.structureChain.clear();
            break;
        }
        case op_get_array_length:
            break;
        case op_to_this:
            if (!curInstruction[2].u.structure || Heap::isMarked(curInstruction[2].u.structure.get()))
                break;
            if (Options::verboseOSR())
                dataLogF("Clearing LLInt to_this with structure %p.\n", curInstruction[2].u.structure.get());
            curInstruction[2].u.structure.clear();
            curInstruction[3].u.toThisStatus = merge(
                curInstruction[3].u.toThisStatus, ToThisClearedByGC);
            break;
        case op_create_this: {
            auto& cacheWriteBarrier = curInstruction[4].u.jsCell;
            if (!cacheWriteBarrier || cacheWriteBarrier.unvalidatedGet() == JSCell::seenMultipleCalleeObjects())
                break;
            JSCell* cachedFunction = cacheWriteBarrier.get();
            if (Heap::isMarked(cachedFunction))
                break;
            if (Options::verboseOSR())
                dataLogF("Clearing LLInt create_this with cached callee %p.\n", cachedFunction);
            cacheWriteBarrier.clear();
            break;
        }
        case op_resolve_scope: {
            // Right now this isn't strictly necessary. Any symbol tables that this will refer to
            // are for outer functions, and we refer to those functions strongly, and they refer
            // to the symbol table strongly. But it's nice to be on the safe side.
            WriteBarrierBase<SymbolTable>& symbolTable = curInstruction[6].u.symbolTable;
            if (!symbolTable || Heap::isMarked(symbolTable.get()))
                break;
            if (Options::verboseOSR())
                dataLogF("Clearing dead symbolTable %p.\n", symbolTable.get());
            symbolTable.clear();
            break;
        }
        case op_get_from_scope:
        case op_put_to_scope: {
            GetPutInfo getPutInfo = GetPutInfo(curInstruction[4].u.operand);
            if (getPutInfo.resolveType() == GlobalVar || getPutInfo.resolveType() == GlobalVarWithVarInjectionChecks 
                || getPutInfo.resolveType() == LocalClosureVar || getPutInfo.resolveType() == GlobalLexicalVar || getPutInfo.resolveType() == GlobalLexicalVarWithVarInjectionChecks)
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
            OpcodeID opcodeID = interpreter->getOpcodeID(curInstruction[0].u.opcode);
            ASSERT_WITH_MESSAGE_UNUSED(opcodeID, false, "Unhandled opcode in CodeBlock::finalizeUnconditionally, %s(%d) at bc %u", opcodeNames[opcodeID], opcodeID, propertyAccessInstructions[i]);
        }
    }

    for (unsigned i = 0; i < m_llintCallLinkInfos.size(); ++i) {
        if (m_llintCallLinkInfos[i].isLinked() && !Heap::isMarked(m_llintCallLinkInfos[i].callee.get())) {
            if (Options::verboseOSR())
                dataLog("Clearing LLInt call from ", *this, "\n");
            m_llintCallLinkInfos[i].unlink();
        }
        if (!!m_llintCallLinkInfos[i].lastSeenCallee && !Heap::isMarked(m_llintCallLinkInfos[i].lastSeenCallee.get()))
            m_llintCallLinkInfos[i].lastSeenCallee.clear();
    }
}

void CodeBlock::finalizeBaselineJITInlineCaches()
{
#if ENABLE(JIT)
    for (auto iter = callLinkInfosBegin(); !!iter; ++iter)
        (*iter)->visitWeak(*vm());

    for (Bag<StructureStubInfo>::iterator iter = m_stubInfos.begin(); !!iter; ++iter) {
        StructureStubInfo& stubInfo = **iter;
        stubInfo.visitWeakReferences(this);
    }
#endif
}

void CodeBlock::UnconditionalFinalizer::finalizeUnconditionally()
{
    CodeBlock* codeBlock = bitwise_cast<CodeBlock*>(
        bitwise_cast<char*>(this) - OBJECT_OFFSETOF(CodeBlock, m_unconditionalFinalizer));

#if ENABLE(DFG_JIT)
    if (codeBlock->shouldJettisonDueToWeakReference()) {
        codeBlock->jettison(Profiler::JettisonDueToWeakReference);
        return;
    }
#endif // ENABLE(DFG_JIT)

    if (codeBlock->shouldJettisonDueToOldAge()) {
        codeBlock->jettison(Profiler::JettisonDueToOldAge);
        return;
    }

    if (JITCode::couldBeInterpreted(codeBlock->jitType()))
        codeBlock->finalizeLLIntInlineCaches();

#if ENABLE(JIT)
    if (!!codeBlock->jitCode())
        codeBlock->finalizeBaselineJITInlineCaches();
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

void CodeBlock::getCallLinkInfoMap(const ConcurrentJITLocker&, CallLinkInfoMap& result)
{
#if ENABLE(JIT)
    toHashMap(m_callLinkInfos, getCallLinkInfoCodeOrigin, result);
#else
    UNUSED_PARAM(result);
#endif
}

void CodeBlock::getCallLinkInfoMap(CallLinkInfoMap& result)
{
    ConcurrentJITLocker locker(m_lock);
    getCallLinkInfoMap(locker, result);
}

void CodeBlock::getByValInfoMap(const ConcurrentJITLocker&, ByValInfoMap& result)
{
#if ENABLE(JIT)
    for (auto* byValInfo : m_byValInfos)
        result.add(CodeOrigin(byValInfo->bytecodeIndex), byValInfo);
#else
    UNUSED_PARAM(result);
#endif
}

void CodeBlock::getByValInfoMap(ByValInfoMap& result)
{
    ConcurrentJITLocker locker(m_lock);
    getByValInfoMap(locker, result);
}

#if ENABLE(JIT)
StructureStubInfo* CodeBlock::addStubInfo(AccessType accessType)
{
    ConcurrentJITLocker locker(m_lock);
    return m_stubInfos.add(accessType);
}

StructureStubInfo* CodeBlock::findStubInfo(CodeOrigin codeOrigin)
{
    for (StructureStubInfo* stubInfo : m_stubInfos) {
        if (stubInfo->codeOrigin == codeOrigin)
            return stubInfo;
    }
    return nullptr;
}

ByValInfo* CodeBlock::addByValInfo()
{
    ConcurrentJITLocker locker(m_lock);
    return m_byValInfos.add();
}

CallLinkInfo* CodeBlock::addCallLinkInfo()
{
    ConcurrentJITLocker locker(m_lock);
    return m_callLinkInfos.add();
}

CallLinkInfo* CodeBlock::getCallLinkInfoForBytecodeIndex(unsigned index)
{
    for (auto iter = m_callLinkInfos.begin(); !!iter; ++iter) {
        if ((*iter)->codeOrigin() == CodeOrigin(index))
            return *iter;
    }
    return nullptr;
}
#endif

void CodeBlock::visitOSRExitTargets(SlotVisitor& visitor)
{
    // We strongly visit OSR exits targets because we don't want to deal with
    // the complexity of generating an exit target CodeBlock on demand and
    // guaranteeing that it matches the details of the CodeBlock we compiled
    // the OSR exit against.

    visitor.append(&m_alternative);

#if ENABLE(DFG_JIT)
    DFG::CommonData* dfgCommon = m_jitCode->dfgCommon();
    if (dfgCommon->inlineCallFrames) {
        for (auto* inlineCallFrame : *dfgCommon->inlineCallFrames) {
            ASSERT(inlineCallFrame->baselineCodeBlock);
            visitor.append(&inlineCallFrame->baselineCodeBlock);
        }
    }
#endif
}

void CodeBlock::stronglyVisitStrongReferences(SlotVisitor& visitor)
{
    visitor.append(&m_globalObject);
    visitor.append(&m_ownerExecutable);
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

#if ENABLE(DFG_JIT)
    if (JITCode::isOptimizingJIT(jitType()))
        visitOSRExitTargets(visitor);
#endif

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

    for (unsigned i = 0; i < dfgCommon->weakStructureReferences.size(); ++i)
        visitor.append(&dfgCommon->weakStructureReferences[i]);

    dfgCommon->livenessHasBeenProved = true;
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

HandlerInfo* CodeBlock::handlerForBytecodeOffset(unsigned bytecodeOffset, RequiredHandler requiredHandler)
{
    RELEASE_ASSERT(bytecodeOffset < instructions().size());
    return handlerForIndex(bytecodeOffset, requiredHandler);
}

HandlerInfo* CodeBlock::handlerForIndex(unsigned index, RequiredHandler requiredHandler)
{
    if (!m_rareData)
        return 0;
    
    Vector<HandlerInfo>& exceptionHandlers = m_rareData->m_exceptionHandlers;
    for (size_t i = 0; i < exceptionHandlers.size(); ++i) {
        HandlerInfo& handler = exceptionHandlers[i];
        if ((requiredHandler == RequiredHandler::CatchHandler) && !handler.isCatchHandler())
            continue;

        // Handlers are ordered innermost first, so the first handler we encounter
        // that contains the source address is the correct handler to use.
        // This index used is either the BytecodeOffset or a CallSiteIndex.
        if (handler.start <= index && handler.end > index)
            return &handler;
    }

    return 0;
}

CallSiteIndex CodeBlock::newExceptionHandlingCallSiteIndex(CallSiteIndex originalCallSite)
{
#if ENABLE(DFG_JIT)
    RELEASE_ASSERT(JITCode::isOptimizingJIT(jitType()));
    RELEASE_ASSERT(canGetCodeOrigin(originalCallSite));
    ASSERT(!!handlerForIndex(originalCallSite.bits()));
    CodeOrigin originalOrigin = codeOrigin(originalCallSite);
    return m_jitCode->dfgCommon()->addUniqueCallSiteIndex(originalOrigin);
#else
    // We never create new on-the-fly exception handling
    // call sites outside the DFG/FTL inline caches.
    UNUSED_PARAM(originalCallSite);
    RELEASE_ASSERT_NOT_REACHED();
    return CallSiteIndex(0u);
#endif
}

void CodeBlock::removeExceptionHandlerForCallSite(CallSiteIndex callSiteIndex)
{
    RELEASE_ASSERT(m_rareData);
    Vector<HandlerInfo>& exceptionHandlers = m_rareData->m_exceptionHandlers;
    unsigned index = callSiteIndex.bits();
    for (size_t i = 0; i < exceptionHandlers.size(); ++i) {
        HandlerInfo& handler = exceptionHandlers[i];
        if (handler.start <= index && handler.end > index) {
            exceptionHandlers.remove(i);
            return;
        }
    }

    RELEASE_ASSERT_NOT_REACHED();
}

unsigned CodeBlock::lineNumberForBytecodeOffset(unsigned bytecodeOffset)
{
    RELEASE_ASSERT(bytecodeOffset < instructions().size());
    return ownerScriptExecutable()->firstLine() + m_unlinkedCode->lineNumberForBytecodeOffset(bytecodeOffset);
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
    line += ownerScriptExecutable()->firstLine();
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
    m_resultProfiles.shrinkToFit();
    
    if (shrinkMode == EarlyShrink) {
        m_constantRegisters.shrinkToFit();
        m_constantsSourceCodeRepresentation.shrinkToFit();
        
        if (m_rareData) {
            m_rareData->m_switchJumpTables.shrinkToFit();
            m_rareData->m_stringSwitchJumpTables.shrinkToFit();
            m_rareData->m_liveCalleeLocalsAtYield.shrinkToFit();
        }
    } // else don't shrink these, because we would have already pointed pointers into these tables.
}

#if ENABLE(JIT)
void CodeBlock::linkIncomingCall(ExecState* callerFrame, CallLinkInfo* incoming)
{
    noticeIncomingCall(callerFrame);
    m_incomingCalls.push(incoming);
}

void CodeBlock::linkIncomingPolymorphicCall(ExecState* callerFrame, PolymorphicCallNode* incoming)
{
    noticeIncomingCall(callerFrame);
    m_incomingPolymorphicCalls.push(incoming);
}
#endif // ENABLE(JIT)

void CodeBlock::unlinkIncomingCalls()
{
    while (m_incomingLLIntCalls.begin() != m_incomingLLIntCalls.end())
        m_incomingLLIntCalls.begin()->unlink();
#if ENABLE(JIT)
    while (m_incomingCalls.begin() != m_incomingCalls.end())
        m_incomingCalls.begin()->unlink(*vm());
    while (m_incomingPolymorphicCalls.begin() != m_incomingPolymorphicCalls.end())
        m_incomingPolymorphicCalls.begin()->unlink(*vm());
#endif // ENABLE(JIT)
}

void CodeBlock::linkIncomingCall(ExecState* callerFrame, LLIntCallLinkInfo* incoming)
{
    noticeIncomingCall(callerFrame);
    m_incomingLLIntCalls.push(incoming);
}

CodeBlock* CodeBlock::newReplacement()
{
    return ownerScriptExecutable()->newReplacementCodeBlockFor(specializationKind());
}

#if ENABLE(JIT)
CodeBlock* CodeBlock::replacement()
{
    const ClassInfo* classInfo = this->classInfo();

    if (classInfo == FunctionCodeBlock::info())
        return jsCast<FunctionExecutable*>(ownerExecutable())->codeBlockFor(m_isConstructor ? CodeForConstruct : CodeForCall);

    if (classInfo == EvalCodeBlock::info())
        return jsCast<EvalExecutable*>(ownerExecutable())->codeBlock();

    if (classInfo == ProgramCodeBlock::info())
        return jsCast<ProgramExecutable*>(ownerExecutable())->codeBlock();

    if (classInfo == ModuleProgramCodeBlock::info())
        return jsCast<ModuleProgramExecutable*>(ownerExecutable())->codeBlock();

#if ENABLE(WEBASSEMBLY)
    if (classInfo == WebAssemblyCodeBlock::info())
        return nullptr;
#endif

    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

DFG::CapabilityLevel CodeBlock::computeCapabilityLevel()
{
    const ClassInfo* classInfo = this->classInfo();

    if (classInfo == FunctionCodeBlock::info()) {
        if (m_isConstructor)
            return DFG::functionForConstructCapabilityLevel(this);
        return DFG::functionForCallCapabilityLevel(this);
    }

    if (classInfo == EvalCodeBlock::info())
        return DFG::evalCapabilityLevel(this);

    if (classInfo == ProgramCodeBlock::info())
        return DFG::programCapabilityLevel(this);

    if (classInfo == ModuleProgramCodeBlock::info())
        return DFG::programCapabilityLevel(this);

#if ENABLE(WEBASSEMBLY)
    if (classInfo == WebAssemblyCodeBlock::info())
        return DFG::CannotCompile;
#endif

    RELEASE_ASSERT_NOT_REACHED();
    return DFG::CannotCompile;
}

#endif // ENABLE(JIT)

void CodeBlock::jettison(Profiler::JettisonReason reason, ReoptimizationMode mode, const FireDetail* detail)
{
#if !ENABLE(DFG_JIT)
    UNUSED_PARAM(mode);
    UNUSED_PARAM(detail);
#endif

    RELEASE_ASSERT(reason != Profiler::NotJettisoned);
    
#if ENABLE(DFG_JIT)
    if (DFG::shouldDumpDisassembly()) {
        dataLog("Jettisoning ", *this);
        if (mode == CountReoptimization)
            dataLog(" and counting reoptimization");
        dataLog(" due to ", reason);
        if (detail)
            dataLog(", ", *detail);
        dataLog(".\n");
    }
    
    if (reason == Profiler::JettisonDueToWeakReference) {
        if (DFG::shouldDumpDisassembly()) {
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
    }
#endif // ENABLE(DFG_JIT)

    DeferGCForAWhile deferGC(*heap());
    
    // We want to accomplish two things here:
    // 1) Make sure that if this CodeBlock is on the stack right now, then if we return to it
    //    we should OSR exit at the top of the next bytecode instruction after the return.
    // 2) Make sure that if we call the owner executable, then we shouldn't call this CodeBlock.

#if ENABLE(DFG_JIT)
    if (reason != Profiler::JettisonDueToOldAge) {
        if (Profiler::Compilation* compilation = jitCode()->dfgCommon()->compilation.get())
            compilation->setJettisonReason(reason, detail);
        
        // This accomplishes (1), and does its own book-keeping about whether it has already happened.
        if (!jitCode()->dfgCommon()->invalidate()) {
            // We've already been invalidated.
            RELEASE_ASSERT(this != replacement());
            return;
        }
    }
    
    if (DFG::shouldDumpDisassembly())
        dataLog("    Did invalidate ", *this, "\n");
    
    // Count the reoptimization if that's what the user wanted.
    if (mode == CountReoptimization) {
        // FIXME: Maybe this should call alternative().
        // https://bugs.webkit.org/show_bug.cgi?id=123677
        baselineAlternative()->countReoptimization();
        if (DFG::shouldDumpDisassembly())
            dataLog("    Did count reoptimization for ", *this, "\n");
    }
    
    if (this != replacement()) {
        // This means that we were never the entrypoint. This can happen for OSR entry code
        // blocks.
        return;
    }

    if (alternative())
        alternative()->optimizeAfterWarmUp();

    if (reason != Profiler::JettisonDueToOldAge)
        tallyFrequentExitSites();
#endif // ENABLE(DFG_JIT)

    // This accomplishes (2).
    ownerScriptExecutable()->installCode(
        m_globalObject->vm(), alternative(), codeType(), specializationKind());

#if ENABLE(DFG_JIT)
    if (DFG::shouldDumpDisassembly())
        dataLog("    Did install baseline version of ", *this, "\n");
#endif // ENABLE(DFG_JIT)
}

JSGlobalObject* CodeBlock::globalObjectFor(CodeOrigin codeOrigin)
{
    if (!codeOrigin.inlineCallFrame)
        return globalObject();
    return codeOrigin.inlineCallFrame->baselineCodeBlock->globalObject();
}

class RecursionCheckFunctor {
public:
    RecursionCheckFunctor(CallFrame* startCallFrame, CodeBlock* codeBlock, unsigned depthToCheck)
        : m_startCallFrame(startCallFrame)
        , m_codeBlock(codeBlock)
        , m_depthToCheck(depthToCheck)
        , m_foundStartCallFrame(false)
        , m_didRecurse(false)
    { }

    StackVisitor::Status operator()(StackVisitor& visitor)
    {
        CallFrame* currentCallFrame = visitor->callFrame();

        if (currentCallFrame == m_startCallFrame)
            m_foundStartCallFrame = true;

        if (m_foundStartCallFrame) {
            if (visitor->callFrame()->codeBlock() == m_codeBlock) {
                m_didRecurse = true;
                return StackVisitor::Done;
            }

            if (!m_depthToCheck--)
                return StackVisitor::Done;
        }

        return StackVisitor::Continue;
    }

    bool didRecurse() const { return m_didRecurse; }

private:
    CallFrame* m_startCallFrame;
    CodeBlock* m_codeBlock;
    unsigned m_depthToCheck;
    bool m_foundStartCallFrame;
    bool m_didRecurse;
};

void CodeBlock::noticeIncomingCall(ExecState* callerFrame)
{
    CodeBlock* callerCodeBlock = callerFrame->codeBlock();
    
    if (Options::verboseCallLink())
        dataLog("Noticing call link from ", pointerDump(callerCodeBlock), " to ", *this, "\n");
    
#if ENABLE(DFG_JIT)
    if (!m_shouldAlwaysBeInlined)
        return;
    
    if (!callerCodeBlock) {
        m_shouldAlwaysBeInlined = false;
        if (Options::verboseCallLink())
            dataLog("    Clearing SABI because caller is native.\n");
        return;
    }

    if (!hasBaselineJITProfiling())
        return;

    if (!DFG::mightInlineFunction(this))
        return;

    if (!canInline(capabilityLevelState()))
        return;
    
    if (!DFG::isSmallEnoughToInlineCodeInto(callerCodeBlock)) {
        m_shouldAlwaysBeInlined = false;
        if (Options::verboseCallLink())
            dataLog("    Clearing SABI because caller is too large.\n");
        return;
    }

    if (callerCodeBlock->jitType() == JITCode::InterpreterThunk) {
        // If the caller is still in the interpreter, then we can't expect inlining to
        // happen anytime soon. Assume it's profitable to optimize it separately. This
        // ensures that a function is SABI only if it is called no more frequently than
        // any of its callers.
        m_shouldAlwaysBeInlined = false;
        if (Options::verboseCallLink())
            dataLog("    Clearing SABI because caller is in LLInt.\n");
        return;
    }
    
    if (JITCode::isOptimizingJIT(callerCodeBlock->jitType())) {
        m_shouldAlwaysBeInlined = false;
        if (Options::verboseCallLink())
            dataLog("    Clearing SABI bcause caller was already optimized.\n");
        return;
    }
    
    if (callerCodeBlock->codeType() != FunctionCode) {
        // If the caller is either eval or global code, assume that that won't be
        // optimized anytime soon. For eval code this is particularly true since we
        // delay eval optimization by a *lot*.
        m_shouldAlwaysBeInlined = false;
        if (Options::verboseCallLink())
            dataLog("    Clearing SABI because caller is not a function.\n");
        return;
    }

    // Recursive calls won't be inlined.
    RecursionCheckFunctor functor(callerFrame, this, Options::maximumInliningDepth());
    vm()->topCallFrame->iterate(functor);

    if (functor.didRecurse()) {
        if (Options::verboseCallLink())
            dataLog("    Clearing SABI because recursion was detected.\n");
        m_shouldAlwaysBeInlined = false;
        return;
    }
    
    if (callerCodeBlock->capabilityLevelState() == DFG::CapabilityLevelNotSet) {
        dataLog("In call from ", *callerCodeBlock, " ", callerFrame->codeOrigin(), " to ", *this, ": caller's DFG capability level is not set.\n");
        CRASH();
    }
    
    if (canCompile(callerCodeBlock->capabilityLevelState()))
        return;
    
    if (Options::verboseCallLink())
        dataLog("    Clearing SABI because the caller is not a DFG candidate.\n");
    
    m_shouldAlwaysBeInlined = false;
#endif
}

unsigned CodeBlock::reoptimizationRetryCounter() const
{
#if ENABLE(JIT)
    ASSERT(m_reoptimizationRetryCounter <= Options::reoptimizationRetryCounterMax());
    return m_reoptimizationRetryCounter;
#else
    return 0;
#endif // ENABLE(JIT)
}

#if ENABLE(JIT)
void CodeBlock::setCalleeSaveRegisters(RegisterSet calleeSaveRegisters)
{
    m_calleeSaveRegisters = std::make_unique<RegisterAtOffsetList>(calleeSaveRegisters);
}

void CodeBlock::setCalleeSaveRegisters(std::unique_ptr<RegisterAtOffsetList> registerAtOffsetList)
{
    m_calleeSaveRegisters = WTFMove(registerAtOffsetList);
}
    
static size_t roundCalleeSaveSpaceAsVirtualRegisters(size_t calleeSaveRegisters)
{
    static const unsigned cpuRegisterSize = sizeof(void*);
    return (WTF::roundUpToMultipleOf(sizeof(Register), calleeSaveRegisters * cpuRegisterSize) / sizeof(Register));

}

size_t CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters()
{
    return roundCalleeSaveSpaceAsVirtualRegisters(numberOfLLIntBaselineCalleeSaveRegisters());
}

size_t CodeBlock::calleeSaveSpaceAsVirtualRegisters()
{
    return roundCalleeSaveSpaceAsVirtualRegisters(m_calleeSaveRegisters->size());
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
    
    result *= codeTypeThresholdMultiplier();
    
    if (Options::verboseOSR()) {
        dataLog(
            *this, ": instruction count is ", instructionCount,
            ", scaling execution counter by ", result, " * ", codeTypeThresholdMultiplier(),
            "\n");
    }
    return result;
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
    JITCode::JITType type = jitType();
    if (type != JITCode::BaselineJIT) {
        dataLog(*this, ": expected to have baseline code but have ", type, "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    CodeBlock* theReplacement = replacement();
    if ((result == CompilationSuccessful) != (theReplacement != this)) {
        dataLog(*this, ": we have result = ", result, " but ");
        if (theReplacement == this)
            dataLog("we are our own replacement.\n");
        else
            dataLog("our replacement is ", pointerDump(theReplacement), "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
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
    
    dataLog("Unrecognized result: ", static_cast<int>(result), "\n");
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

#if ENABLE(DFG_JIT)
Vector<CodeOrigin, 0, UnsafeVectorOverflow>& CodeBlock::codeOrigins()
{
    return m_jitCode->dfgCommon()->codeOrigins;
}

size_t CodeBlock::numberOfDFGIdentifiers() const
{
    if (!JITCode::isOptimizingJIT(jitType()))
        return 0;
    
    return m_jitCode->dfgCommon()->dfgIdentifiers.size();
}

const Identifier& CodeBlock::identifier(int index) const
{
    size_t unlinkedIdentifiers = m_unlinkedCode->numberOfIdentifiers();
    if (static_cast<unsigned>(index) < unlinkedIdentifiers)
        return m_unlinkedCode->identifier(index);
    ASSERT(JITCode::isOptimizingJIT(jitType()));
    return m_jitCode->dfgCommon()->dfgIdentifiers[index - unlinkedIdentifiers];
}
#endif // ENABLE(DFG_JIT)

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
#if ENABLE(WEBASSEMBLY)
    if (m_ownerExecutable->isWebAssemblyExecutable())
        return;
#endif
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
            exit.considerAddingAsFrequentExitSite(profiledBlock);
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
            exit.considerAddingAsFrequentExitSite(profiledBlock);
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
    dataLog("ResultProfile for ", *this, ":\n");
    for (unsigned i = 0; i < numberOfResultProfiles(); ++i) {
        const ResultProfile& profile = *resultProfile(i);
        dataLog("   bc = ", profile.bytecodeOffset(), ": ", profile, "\n");
    }
}
#endif // ENABLE(VERBOSE_VALUE_PROFILE)

unsigned CodeBlock::frameRegisterCount()
{
    switch (jitType()) {
    case JITCode::InterpreterThunk:
        return LLInt::frameRegisterCountFor(this);

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
    for (unsigned i = 0; i < m_constantRegisters.size(); i++) {
        if (m_constantRegisters[i].get().isEmpty())
            continue;
        if (SymbolTable* symbolTable = jsDynamicCast<SymbolTable*>(m_constantRegisters[i].get())) {
            ConcurrentJITLocker locker(symbolTable->m_lock);
            auto end = symbolTable->end(locker);
            for (auto ptr = symbolTable->begin(locker); ptr != end; ++ptr) {
                if (ptr->value.varOffset() == VarOffset(virtualRegister)) {
                    // FIXME: This won't work from the compilation thread.
                    // https://bugs.webkit.org/show_bug.cgi?id=115300
                    return ptr->key.get();
                }
            }
        }
    }
    if (virtualRegister == thisRegister())
        return ASCIILiteral("this");
    if (virtualRegister.isArgument())
        return String::format("arguments[%3d]", virtualRegister.toArgument());

    return "";
}

ValueProfile* CodeBlock::valueProfileForBytecodeOffset(int bytecodeOffset)
{
    ValueProfile* result = binarySearch<ValueProfile, int>(
        m_valueProfiles, m_valueProfiles.size(), bytecodeOffset,
        getValueProfileBytecodeOffset<ValueProfile>);
    ASSERT(result->m_bytecodeOffset != -1);
    ASSERT(instructions()[bytecodeOffset + opcodeLength(
        m_vm->interpreter->getOpcodeID(
            instructions()[bytecodeOffset].u.opcode)) - 1].u.profile == result);
    return result;
}

void CodeBlock::validate()
{
    BytecodeLivenessAnalysis liveness(this); // Compute directly from scratch so it doesn't effect CodeBlock footprint.
    
    FastBitVector liveAtHead = liveness.getLivenessInfoAtBytecodeOffset(0);
    
    if (liveAtHead.numBits() != static_cast<size_t>(m_numCalleeLocals)) {
        beginValidationDidFail();
        dataLog("    Wrong number of bits in result!\n");
        dataLog("    Result: ", liveAtHead, "\n");
        dataLog("    Bit count: ", liveAtHead.numBits(), "\n");
        endValidationDidFail();
    }
    
    for (unsigned i = m_numCalleeLocals; i--;) {
        VirtualRegister reg = virtualRegisterForLocal(i);
        
        if (liveAtHead.get(i)) {
            beginValidationDidFail();
            dataLog("    Variable ", reg, " is expected to be dead.\n");
            dataLog("    Result: ", liveAtHead, "\n");
            endValidationDidFail();
        }
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
        jettison(Profiler::JettisonDueToDebuggerBreakpoint);
}

void CodeBlock::setSteppingMode(CodeBlock::SteppingMode mode)
{
    m_steppingMode = mode;
    if (mode == SteppingModeEnabled && JITCode::isOptimizingJIT(jitType()))
        jettison(Profiler::JettisonDueToDebuggerStepping);
}

RareCaseProfile* CodeBlock::rareCaseProfileForBytecodeOffset(int bytecodeOffset)
{
    return tryBinarySearch<RareCaseProfile, int>(
        m_rareCaseProfiles, m_rareCaseProfiles.size(), bytecodeOffset,
        getRareCaseProfileBytecodeOffset);
}

unsigned CodeBlock::rareCaseProfileCountForBytecodeOffset(int bytecodeOffset)
{
    RareCaseProfile* profile = rareCaseProfileForBytecodeOffset(bytecodeOffset);
    if (profile)
        return profile->m_counter;
    return 0;
}

ResultProfile* CodeBlock::resultProfileForBytecodeOffset(int bytecodeOffset)
{
    if (!m_bytecodeOffsetToResultProfileIndexMap)
        return nullptr;
    auto iterator = m_bytecodeOffsetToResultProfileIndexMap->find(bytecodeOffset);
    if (iterator == m_bytecodeOffsetToResultProfileIndexMap->end())
        return nullptr;
    return &m_resultProfiles[iterator->value];
}

#if ENABLE(JIT)
DFG::CapabilityLevel CodeBlock::capabilityLevel()
{
    DFG::CapabilityLevel result = computeCapabilityLevel();
    m_capabilityLevelState = result;
    return result;
}
#endif

void CodeBlock::insertBasicBlockBoundariesForControlFlowProfiler(RefCountedArray<Instruction>& instructions)
{
    if (!unlinkedCodeBlock()->hasOpProfileControlFlowBytecodeOffsets())
        return;
    const Vector<size_t>& bytecodeOffsets = unlinkedCodeBlock()->opProfileControlFlowBytecodeOffsets();
    for (size_t i = 0, offsetsLength = bytecodeOffsets.size(); i < offsetsLength; i++) {
        // Because op_profile_control_flow is emitted at the beginning of every basic block, finding 
        // the next op_profile_control_flow will give us the text range of a single basic block.
        size_t startIdx = bytecodeOffsets[i];
        RELEASE_ASSERT(vm()->interpreter->getOpcodeID(instructions[startIdx].u.opcode) == op_profile_control_flow);
        int basicBlockStartOffset = instructions[startIdx + 1].u.operand;
        int basicBlockEndOffset;
        if (i + 1 < offsetsLength) {
            size_t endIdx = bytecodeOffsets[i + 1];
            RELEASE_ASSERT(vm()->interpreter->getOpcodeID(instructions[endIdx].u.opcode) == op_profile_control_flow);
            basicBlockEndOffset = instructions[endIdx + 1].u.operand - 1;
        } else {
            basicBlockEndOffset = m_sourceOffset + ownerScriptExecutable()->source().length() - 1; // Offset before the closing brace.
            basicBlockStartOffset = std::min(basicBlockStartOffset, basicBlockEndOffset); // Some start offsets may be at the closing brace, ensure it is the offset before.
        }

        // The following check allows for the same textual JavaScript basic block to have its bytecode emitted more
        // than once and still play nice with the control flow profiler. When basicBlockStartOffset is larger than 
        // basicBlockEndOffset, it indicates that the bytecode generator has emitted code for the same AST node 
        // more than once (for example: ForInNode, Finally blocks in TryNode, etc). Though these are different 
        // basic blocks at the bytecode level, they are generated from the same textual basic block in the JavaScript 
        // program. The condition: 
        // (basicBlockEndOffset < basicBlockStartOffset) 
        // is encountered when op_profile_control_flow lies across the boundary of these duplicated bytecode basic 
        // blocks and the textual offset goes from the end of the duplicated block back to the beginning. These 
        // ranges are dummy ranges and are ignored. The duplicated bytecode basic blocks point to the same 
        // internal data structure, so if any of them execute, it will record the same textual basic block in the 
        // JavaScript program as executing.
        // At the bytecode level, this situation looks like:
        // j: op_profile_control_flow (from j->k, we have basicBlockEndOffset < basicBlockStartOffset)
        // ...
        // k: op_profile_control_flow (we want to skip over the j->k block and start fresh at offset k as the start of a new basic block k->m).
        // ...
        // m: op_profile_control_flow
        if (basicBlockEndOffset < basicBlockStartOffset) {
            RELEASE_ASSERT(i + 1 < offsetsLength); // We should never encounter dummy blocks at the end of a CodeBlock.
            instructions[startIdx + 1].u.basicBlockLocation = vm()->controlFlowProfiler()->dummyBasicBlock();
            continue;
        }

        BasicBlockLocation* basicBlockLocation = vm()->controlFlowProfiler()->getBasicBlockLocation(ownerScriptExecutable()->sourceID(), basicBlockStartOffset, basicBlockEndOffset);

        // Find all functions that are enclosed within the range: [basicBlockStartOffset, basicBlockEndOffset]
        // and insert these functions' start/end offsets as gaps in the current BasicBlockLocation.
        // This is necessary because in the original source text of a JavaScript program, 
        // function literals form new basic blocks boundaries, but they aren't represented 
        // inside the CodeBlock's instruction stream.
        auto insertFunctionGaps = [basicBlockLocation, basicBlockStartOffset, basicBlockEndOffset] (const WriteBarrier<FunctionExecutable>& functionExecutable) {
            const UnlinkedFunctionExecutable* executable = functionExecutable->unlinkedExecutable();
            int functionStart = executable->typeProfilingStartOffset();
            int functionEnd = executable->typeProfilingEndOffset();
            if (functionStart >= basicBlockStartOffset && functionEnd <= basicBlockEndOffset)
                basicBlockLocation->insertGap(functionStart, functionEnd);
        };

        for (const WriteBarrier<FunctionExecutable>& executable : m_functionDecls)
            insertFunctionGaps(executable);
        for (const WriteBarrier<FunctionExecutable>& executable : m_functionExprs)
            insertFunctionGaps(executable);

        instructions[startIdx + 1].u.basicBlockLocation = basicBlockLocation;
    }
}

#if ENABLE(JIT)
void CodeBlock::setPCToCodeOriginMap(std::unique_ptr<PCToCodeOriginMap>&& map) 
{ 
    m_pcToCodeOriginMap = WTFMove(map);
}

Optional<CodeOrigin> CodeBlock::findPC(void* pc)
{
    if (m_pcToCodeOriginMap) {
        if (Optional<CodeOrigin> codeOrigin = m_pcToCodeOriginMap->findPC(pc))
            return codeOrigin;
    }

    for (Bag<StructureStubInfo>::iterator iter = m_stubInfos.begin(); !!iter; ++iter) {
        StructureStubInfo* stub = *iter;
        if (stub->containsPC(pc))
            return Optional<CodeOrigin>(stub->codeOrigin);
    }

    if (Optional<CodeOrigin> codeOrigin = m_jitCode->findPC(this, pc))
        return codeOrigin;

    return Nullopt;
}
#endif // ENABLE(JIT)

} // namespace JSC
