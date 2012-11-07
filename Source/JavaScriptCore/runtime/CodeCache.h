/*
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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

#ifndef CodeCache_h
#define CodeCache_h

#include "CodeSpecializationKind.h"
#include "ParserModes.h"
#include "Strong.h"
#include "WeakRandom.h"

#include <wtf/FixedArray.h>
#include <wtf/Forward.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class EvalExecutable;
class Identifier;
class ProgramExecutable;
class UnlinkedCodeBlock;
class UnlinkedEvalCodeBlock;
class UnlinkedFunctionCodeBlock;
class UnlinkedFunctionExecutable;
class UnlinkedProgramCodeBlock;
class JSGlobalData;
struct ParserError;
class SourceCode;
class SourceProvider;

class CodeCache {
public:
    static PassOwnPtr<CodeCache> create() { return adoptPtr(new CodeCache); }

    UnlinkedProgramCodeBlock* getProgramCodeBlock(JSGlobalData&, ProgramExecutable*, const SourceCode&, JSParserStrictness, DebuggerMode, ProfilerMode, ParserError&);
    UnlinkedEvalCodeBlock* getEvalCodeBlock(JSGlobalData&, EvalExecutable*, const SourceCode&, JSParserStrictness, DebuggerMode, ProfilerMode, ParserError&);
    UnlinkedFunctionCodeBlock* getFunctionCodeBlock(JSGlobalData&, UnlinkedFunctionExecutable*, const SourceCode&, CodeSpecializationKind, DebuggerMode, ProfilerMode, ParserError&);
    UnlinkedFunctionExecutable* getFunctionExecutableFromGlobalCode(JSGlobalData&, const Identifier&, const SourceCode&, ParserError&);
    ~CodeCache();

    enum CodeType { EvalType, ProgramType, FunctionType };
    typedef std::pair<String, unsigned> CodeBlockKey;
    typedef HashMap<CodeBlockKey, unsigned> CodeBlockIndicesMap;
    typedef std::pair<String, String> GlobalFunctionKey;
    typedef HashMap<GlobalFunctionKey, unsigned> GlobalFunctionIndicesMap;

private:
    CodeCache();

    UnlinkedFunctionCodeBlock* generateFunctionCodeBlock(JSGlobalData&, UnlinkedFunctionExecutable*, const SourceCode&, CodeSpecializationKind, DebuggerMode, ProfilerMode, ParserError&);

    template <class UnlinkedCodeBlockType, class ExecutableType> inline UnlinkedCodeBlockType* getCodeBlock(JSGlobalData&, ExecutableType*, const SourceCode&, JSParserStrictness, DebuggerMode, ProfilerMode, ParserError&);
    CodeBlockKey makeCodeBlockKey(const SourceCode&, CodeType, JSParserStrictness);
    CodeBlockIndicesMap m_cachedCodeBlockIndices;
    GlobalFunctionKey makeGlobalFunctionKey(const SourceCode&, const String&);
    GlobalFunctionIndicesMap m_cachedGlobalFunctionIndices;

    enum {
        kMaxCodeBlockEntries = 1024,
        kMaxGlobalFunctionEntries = 1024
    };

    FixedArray<std::pair<CodeBlockKey, Strong<UnlinkedCodeBlock> >, kMaxCodeBlockEntries> m_cachedCodeBlocks;
    FixedArray<std::pair<GlobalFunctionKey, Strong<UnlinkedFunctionExecutable> >, kMaxGlobalFunctionEntries> m_cachedGlobalFunctions;
    WeakRandom m_randomGenerator;
};

}

#endif
