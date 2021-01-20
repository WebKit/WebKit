/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#include "FuzzerAgent.h"
#include "Opcode.h"
#include <wtf/Lock.h>

namespace JSC {

class VM;

struct PredictionTarget {
    BytecodeIndex bytecodeIndex;
    int divot;
    int startOffset;
    int endOffset;
    unsigned line;
    unsigned column;
    OpcodeID opcodeId;
    String sourceFilename;
    String lookupKey;
};

class FileBasedFuzzerAgentBase : public FuzzerAgent {
    WTF_MAKE_FAST_ALLOCATED;

public:
    FileBasedFuzzerAgentBase(VM&);

protected:
    Lock m_lock;
    virtual SpeculatedType getPredictionInternal(CodeBlock*, PredictionTarget&, SpeculatedType original) = 0;

public:
    SpeculatedType getPrediction(CodeBlock*, const CodeOrigin&, SpeculatedType original) final;

protected:
    static String createLookupKey(const String& sourceFilename, OpcodeID, int startLocation, int endLocation);
    static OpcodeID opcodeAliasForLookupKey(const OpcodeID&);
};

} // namespace JSC
