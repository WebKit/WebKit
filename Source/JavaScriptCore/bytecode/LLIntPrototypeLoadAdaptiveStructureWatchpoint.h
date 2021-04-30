/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "BytecodeStructs.h"
#include "CodeBlock.h"
#include "ObjectPropertyCondition.h"
#include "PackedCellPtr.h"
#include "Watchpoint.h"

namespace JSC {

class LLIntPrototypeLoadAdaptiveStructureWatchpoint final : public Watchpoint {
public:
    LLIntPrototypeLoadAdaptiveStructureWatchpoint(CodeBlock*, const ObjectPropertyCondition&, unsigned bytecodeOffset);
    LLIntPrototypeLoadAdaptiveStructureWatchpoint();

    void initialize(CodeBlock*, const ObjectPropertyCondition&, unsigned bytecodeOffset);

    void install(VM&);

    static void clearLLIntGetByIdCache(GetByIdModeMetadata&);

    const ObjectPropertyCondition& key() const { return m_key; }

    void fireInternal(VM&, const FireDetail&);

private:
    // Own destructor may not be called. Keep members trivially destructible.
    JSC_WATCHPOINT_FIELD(PackedCellPtr<CodeBlock>, m_owner);
    JSC_WATCHPOINT_FIELD(Packed<unsigned>, m_bytecodeOffset);
    JSC_WATCHPOINT_FIELD(ObjectPropertyCondition, m_key);
};

} // namespace JSC
