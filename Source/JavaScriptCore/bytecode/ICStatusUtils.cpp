/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "ICStatusUtils.h"

#include "CodeBlock.h"
#include "ExitFlag.h"
#include "UnlinkedCodeBlock.h"

namespace JSC {

ExitFlag hasBadCacheExitSite(CodeBlock* profiledBlock, BytecodeIndex bytecodeIndex)
{
#if ENABLE(DFG_JIT)
    UnlinkedCodeBlock* unlinkedCodeBlock = profiledBlock->unlinkedCodeBlock();
    ConcurrentJSLocker locker(unlinkedCodeBlock->m_lock);
    auto exitFlag = [&] (ExitKind exitKind) -> ExitFlag {
        auto withInlined = [&] (ExitingInlineKind inlineKind) -> ExitFlag {
            return ExitFlag(unlinkedCodeBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, exitKind, ExitFromAnything, inlineKind)), inlineKind);
        };
        return withInlined(ExitFromNotInlined) | withInlined(ExitFromInlined);
    };
    return exitFlag(BadCache) | exitFlag(BadConstantCache) | exitFlag(BadType);
#else
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
    return ExitFlag();
#endif
}

} // namespace JSC

