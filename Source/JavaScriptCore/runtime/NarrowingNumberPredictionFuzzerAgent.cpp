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

#include "config.h"
#include "NarrowingNumberPredictionFuzzerAgent.h"

#include "CodeBlock.h"

namespace JSC {

NarrowingNumberPredictionFuzzerAgent::NarrowingNumberPredictionFuzzerAgent(VM& vm)
    : NumberPredictionFuzzerAgent(vm)
{
}

SpeculatedType NarrowingNumberPredictionFuzzerAgent::getPrediction(CodeBlock* codeBlock, const CodeOrigin& codeOrigin, SpeculatedType original)
{
    auto locker = holdLock(m_lock);

    if (!(original && speculationChecked(original, SpecBytecodeNumber)))
        return original;

    Vector<SpeculatedType> numberTypesThatCouldBePartOfSpeculation;
    for (auto numberType : bytecodeNumberTypes()) {
        if (numberType & original)
            numberTypesThatCouldBePartOfSpeculation.append(numberType);
    }

    unsigned numberOfTypesToKeep = m_random.getUint32(numberTypesThatCouldBePartOfSpeculation.size()) + 1;
    if (numberOfTypesToKeep == numberTypesThatCouldBePartOfSpeculation.size())
        return original;

    SpeculatedType generated = SpecNone;
    for (unsigned i = 0; i < numberOfTypesToKeep; i++) {
        unsigned indexOfTypeToKeep = m_random.getUint32(numberTypesThatCouldBePartOfSpeculation.size());
        mergeSpeculation(generated, numberTypesThatCouldBePartOfSpeculation[indexOfTypeToKeep]);
        numberTypesThatCouldBePartOfSpeculation.remove(indexOfTypeToKeep);
    }

    if (Options::dumpFuzzerAgentPredictions())
        dataLogLn("NarrowingNumberPredictionFuzzerAgent::getPrediction name:(", codeBlock->inferredName(), "#", codeBlock->hashAsStringIfPossible(), "),bytecodeIndex:(", codeOrigin.bytecodeIndex(), "),original:(", SpeculationDump(original), "),generated:(", SpeculationDump(generated), ")");

    return generated;
}

} // namespace JSC
