/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
#include "PredictionFileCreatingFuzzerAgent.h"
#include <wtf/DataLog.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PredictionFileCreatingFuzzerAgent);

PredictionFileCreatingFuzzerAgent::PredictionFileCreatingFuzzerAgent(VM& vm)
    : FileBasedFuzzerAgentBase(vm)
{
}

SpeculatedType PredictionFileCreatingFuzzerAgent::getPredictionInternal(CodeBlock*, PredictionTarget& predictionTarget, SpeculatedType original)
{
    switch (predictionTarget.opcodeId) {
    case op_to_this:
    case op_get_by_val:
    case op_get_argument:
    case op_get_from_arguments:
    case op_get_from_scope:
    case op_get_by_id:
    case op_get_by_id_with_this:
    case op_get_by_val_with_this:
    case op_enumerator_get_by_val:
    case op_construct:
    case op_construct_varargs:
    case op_call:
    case op_call_ignore_result:
    case op_call_direct_eval:
    case op_call_varargs:
    case op_tail_call:
    case op_tail_call_varargs:
        dataLogF("%s:%" PRIx64 "\n", predictionTarget.lookupKey.utf8().data(), original);
        break;

    default:
        RELEASE_ASSERT_WITH_MESSAGE(false, "unhandled opcode: %s", opcodeNames[predictionTarget.opcodeId]);
    }
    return original;
}

} // namespace JSC
