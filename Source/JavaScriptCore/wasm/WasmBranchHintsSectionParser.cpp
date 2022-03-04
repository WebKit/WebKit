/*
 * Copyright (C) 2022 Leaning Technologies Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include "WasmBranchHintsSectionParser.h"

namespace JSC {
namespace Wasm {

auto BranchHintsSectionParser::parse() -> PartialResult
{
    uint32_t functionCount;
    int64_t previousFunctionIndex = -1;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(functionCount), "can't get function count");

    for (uint32_t i = 0; i < functionCount; ++i) {
        uint32_t functionIndex;
        uint32_t hintCount;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(functionIndex), "can't get function index for function ", i);
        WASM_PARSER_FAIL_IF(static_cast<int64_t>(functionIndex) < previousFunctionIndex, "invalid function index ", functionIndex, " for function ", i);

        previousFunctionIndex = functionIndex;

        WASM_PARSER_FAIL_IF(!parseVarUInt32(hintCount), "can't get number of hints for function ", i);

        if (!hintCount)
            continue;

        int64_t previousBranchOffset = -1;
        BranchHintMap branchHintsForFunction;
        for (uint32_t j = 0; j < hintCount; ++j) {
            uint32_t branchOffset;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(branchOffset), "can't get branch offset for hint ", j);
            WASM_PARSER_FAIL_IF(static_cast<int64_t>(branchOffset) < previousBranchOffset
                || !m_info->branchHints.isValidKey(branchOffset), "invalid branch offset ", branchOffset, " for hint ", j);

            previousBranchOffset = branchOffset;

            uint32_t payloadSize;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(payloadSize), "can't get payload size for hint ", j);
            WASM_PARSER_FAIL_IF(payloadSize != 0x1, "invalid payload size for hint ", j);

            uint8_t parsedBranchHint;
            WASM_PARSER_FAIL_IF(!parseVarUInt1(parsedBranchHint), "can't get or invalid branch hint value for hint ", j);

            BranchHint branchHint = static_cast<BranchHint>(parsedBranchHint);
            ASSERT(isValidBranchHint(branchHint));

            branchHintsForFunction.add(branchOffset, branchHint);
        }
        m_info->branchHints.add(functionIndex, WTFMove(branchHintsForFunction));
    }
    return { };
}

}
} // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
