/*
 * Copyright (C) 2019-2024 Apple Inc. All rights reserved.
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
#include "FileBasedFuzzerAgentBase.h"

#include "CodeBlock.h"
#include "JSCellInlines.h"
#include <wtf/text/StringBuilder.h>

namespace JSC {

FileBasedFuzzerAgentBase::FileBasedFuzzerAgentBase(VM&)
{
}

String FileBasedFuzzerAgentBase::createLookupKey(const String& sourceFilename, OpcodeID opcodeId, int startLocation, int endLocation)
{
    StringBuilder lookupKey;
    lookupKey.append(sourceFilename);
    lookupKey.append("|");
    lookupKey.append(opcodeNames[opcodeAliasForLookupKey(opcodeId)]);
    lookupKey.append("|");
    lookupKey.append(startLocation);
    lookupKey.append("|");
    lookupKey.append(endLocation);
    return lookupKey.toString();
}

OpcodeID FileBasedFuzzerAgentBase::opcodeAliasForLookupKey(const OpcodeID& opcodeId)
{
    if (opcodeId == op_call_varargs || opcodeId == op_call_direct_eval || opcodeId == op_tail_call || opcodeId == op_tail_call_varargs)
        return op_call;
    if (opcodeId == op_enumerator_get_by_val || opcodeId == op_get_by_val_with_this)
        return op_get_by_val;
    if (opcodeId == op_construct_varargs)
        return op_construct;
    return opcodeId;
}

SpeculatedType FileBasedFuzzerAgentBase::getPrediction(CodeBlock* codeBlock, const CodeOrigin& codeOrigin, SpeculatedType original)
{
    Locker locker { m_lock };

    ScriptExecutable* ownerExecutable = codeBlock->ownerExecutable();
    const auto& sourceURL = ownerExecutable->sourceURL();
    if (sourceURL.isEmpty())
        return original;

    PredictionTarget predictionTarget;
    BytecodeIndex bytecodeIndex = codeOrigin.bytecodeIndex();
    predictionTarget.info = codeBlock->expressionInfoForBytecodeIndex(bytecodeIndex);

    Vector<String> urlParts = sourceURL.split('/');
    predictionTarget.sourceFilename = urlParts.isEmpty() ? sourceURL : urlParts.last();

    const auto& instructions = codeBlock->instructions();
    const auto* anInstruction = instructions.at(bytecodeIndex).ptr();
    predictionTarget.opcodeId = anInstruction->opcodeID();

    int startLocation = predictionTarget.info.divot - predictionTarget.info.startOffset;
    int endLocation = predictionTarget.info.divot + predictionTarget.info.endOffset;
    predictionTarget.lookupKey = createLookupKey(predictionTarget.sourceFilename, predictionTarget.opcodeId, startLocation, endLocation);
    return getPredictionInternal(codeBlock, predictionTarget, original);
}

} // namespace JSC
