/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "AbstractSlotVisitorInlines.h"
#include "VerifierSlotVisitor.h"

namespace JSC {

template<typename Functor>
void VerifierSlotVisitor::forEachLiveCell(const Functor& functor)
{
    forEachLivePreciseAllocation(functor);
    forEachLiveMarkedBlockCell(functor);
}

template<typename Functor>
void VerifierSlotVisitor::forEachLivePreciseAllocation(const Functor& func)
{
    for (auto& entry : m_preciseAllocationMap) {
        PreciseAllocation* allocation = entry.value->allocation();
        func(allocation->cell());
    }
}

template<typename Functor>
void VerifierSlotVisitor::forEachLiveMarkedBlockCell(const Functor& func)
{
    for (auto& entry : m_markedBlockMap) {
        auto& data = entry.value;
        MarkedBlock* block = data->block();
        const MarkedBlockData::AtomsBitmap& atoms =  data->atoms();
        for (auto atomNumber : atoms)
            func(bitwise_cast<HeapCell*>(block->handle().atomAt(atomNumber)));
    }
}

} // namespace JSC
