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
#include "FileBasedFuzzerAgent.h"

#include "CodeBlock.h"
#include "FuzzerPredictions.h"
#include "JSCellInlines.h"
#include <wtf/AnsiColors.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FileBasedFuzzerAgent);

FileBasedFuzzerAgent::FileBasedFuzzerAgent(VM& vm)
    : FileBasedFuzzerAgentBase(vm)
{
}

SpeculatedType FileBasedFuzzerAgent::getPredictionInternal(CodeBlock* codeBlock, PredictionTarget& target, SpeculatedType original)
{
    FuzzerPredictions& fuzzerPredictions = ensureGlobalFuzzerPredictions();
    std::optional<SpeculatedType> generated = fuzzerPredictions.predictionFor(target.lookupKey);

    SourceProvider* provider = codeBlock->source().provider();
    auto sourceUpToDivot = provider->source().substring(target.info.divot - target.info.startOffset, target.info.startOffset);
    auto sourceAfterDivot = provider->source().substring(target.info.divot, target.info.endOffset);

    switch (target.opcodeId) {
    // FIXME: these can not be targeted at all due to the bugs below
    case op_to_this: // broken https://bugs.webkit.org/show_bug.cgi?id=203555
    case op_get_argument: // broken https://bugs.webkit.org/show_bug.cgi?id=203554
        return original;

    // FIXME: the output of codeBlock->expressionInfoForBytecodeIndex() allows for some of
    // these opcodes to have predictions, but not all instances can be reliably targeted.
    case op_get_from_scope: // partially broken https://bugs.webkit.org/show_bug.cgi?id=203603
    case op_get_from_arguments: // partially broken https://bugs.webkit.org/show_bug.cgi?id=203608
    case op_get_by_val: // partially broken https://bugs.webkit.org/show_bug.cgi?id=203665
    case op_get_by_id: // sometimes occurs implicitly for things related to Symbol.iterator
        if (!generated)
            return original;
        break;

    case op_call: // op_call appears implicitly in for-of loops, generators, spread/rest elements, destructuring assignment
    case op_call_ignore_result:
        if (!generated) {
            if (sourceAfterDivot.containsIgnoringASCIICase("of "_s))
                return original;
            if (sourceAfterDivot.containsIgnoringASCIICase("..."_s))
                return original;
            if (sourceAfterDivot.containsIgnoringASCIICase("yield"_s))
                return original;
            if (sourceAfterDivot.startsWith('[') && sourceAfterDivot.endsWith(']'))
                return original;
            if (sourceUpToDivot.containsIgnoringASCIICase("yield"_s))
                return original;
            if (sourceUpToDivot == "..."_s)
                return original;
            if (!target.info.startOffset && !target.info.endOffset)
                return original;
        }
        break;

    case op_get_by_val_with_this:
    case op_construct:
    case op_construct_varargs:
    case op_call_varargs:
    case op_call_direct_eval:
    case op_tail_call:
    case op_tail_call_varargs:
    case op_get_by_id_with_this:
        break;

    default:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled opcode %s", opcodeNames[target.opcodeId]);
    }
    if (!generated) {
        if (Options::dumpFuzzerAgentPredictions())
            dataLogLn(MAGENTA(BOLD(target.info.instPC)), " ", BOLD(YELLOW(target.opcodeId)), " missing prediction for: ", RED(BOLD(target.lookupKey)), " ", GREEN(target.sourceFilename), ":", CYAN(target.info.lineColumn.line), ":", CYAN(target.info.lineColumn.column), " divot: ", target.info.divot, " -", target.info.startOffset, " +", target.info.endOffset, " name: '", YELLOW(codeBlock->inferredName()), "' source: '", BLUE(sourceUpToDivot), BLUE(BOLD(sourceAfterDivot)), "'");

        RELEASE_ASSERT_WITH_MESSAGE(!Options::requirePredictionForFileBasedFuzzerAgent(), "Missing expected prediction in FuzzerAgent");
        return original;
    }
    if (Options::dumpFuzzerAgentPredictions())
        dataLogLn(YELLOW(target.opcodeId), " ", CYAN(target.lookupKey), " original: ", CYAN(BOLD(SpeculationDump(original))), " generated: ", MAGENTA(BOLD(SpeculationDump(*generated))));
    return *generated;
}

} // namespace JSC
