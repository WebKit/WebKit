/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2021 Igalia S.A. All rights reserved.
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
#include "SetPrivateBrandVariant.h"

#include "CacheableIdentifierInlines.h"

namespace JSC {

SetPrivateBrandVariant::SetPrivateBrandVariant(CacheableIdentifier identifier, Structure* oldStructure, Structure* newStructure)
    : m_oldStructure(oldStructure)
    , m_newStructure(newStructure)
    , m_identifier(WTFMove(identifier))
{ }

SetPrivateBrandVariant::~SetPrivateBrandVariant() = default;

bool SetPrivateBrandVariant::attemptToMerge(const SetPrivateBrandVariant& other)
{
    if (!!m_identifier != !!other.m_identifier)
        return false;

    if (m_identifier && (m_identifier != other.m_identifier))
        return false;

    if (m_oldStructure != other.m_oldStructure)
        return false;
    ASSERT(m_newStructure == other.m_newStructure);

    return true;
}

template<typename Visitor>
void SetPrivateBrandVariant::markIfCheap(Visitor& visitor)
{
    if (m_oldStructure)
        m_oldStructure->markIfCheap(visitor);
    if (m_newStructure)
        m_newStructure->markIfCheap(visitor);
}

template void SetPrivateBrandVariant::markIfCheap(AbstractSlotVisitor&);
template void SetPrivateBrandVariant::markIfCheap(SlotVisitor&);

bool SetPrivateBrandVariant::finalize(VM& vm)
{
    if (!vm.heap.isMarked(m_oldStructure))
        return false;
    if (m_newStructure && !vm.heap.isMarked(m_newStructure))
        return false;
    return true;
}

template<typename Visitor>
void SetPrivateBrandVariant::visitAggregateImpl(Visitor& visitor)
{
    m_identifier.visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE(SetPrivateBrandVariant);

void SetPrivateBrandVariant::dump(PrintStream& out) const
{
    dumpInContext(out, nullptr);
}

void SetPrivateBrandVariant::dumpInContext(PrintStream& out, DumpContext*) const
{
    out.print("<");
    out.print("id='", m_identifier, "'");
    if (m_oldStructure)
        out.print(", ", *m_oldStructure);
    if (m_newStructure)
        out.print(" -> ", *m_newStructure);
    out.print(">");
}

} // namespace JSC
