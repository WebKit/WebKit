/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "CheckPrivateBrandVariant.h"

#include "CacheableIdentifierInlines.h"

namespace JSC {

CheckPrivateBrandVariant::CheckPrivateBrandVariant(CacheableIdentifier identifier, const StructureSet& structureSet)
    : m_structureSet(structureSet)
    , m_identifier(WTFMove(identifier))
{ }

CheckPrivateBrandVariant::~CheckPrivateBrandVariant() = default;

bool CheckPrivateBrandVariant::attemptToMerge(const CheckPrivateBrandVariant& other)
{
    if (!!m_identifier != !!other.m_identifier)
        return false;

    if (m_identifier && (m_identifier != other.m_identifier))
        return false;

    m_structureSet.merge(other.m_structureSet);

    return true;
}

template<typename Visitor>
void CheckPrivateBrandVariant::markIfCheap(Visitor& visitor)
{
    m_structureSet.markIfCheap(visitor);
}

template void CheckPrivateBrandVariant::markIfCheap(AbstractSlotVisitor&);
template void CheckPrivateBrandVariant::markIfCheap(SlotVisitor&);

bool CheckPrivateBrandVariant::finalize(VM& vm)
{
    if (!m_structureSet.isStillAlive(vm))
        return false;
    return true;
}

template<typename Visitor>
void CheckPrivateBrandVariant::visitAggregateImpl(Visitor& visitor)
{
    m_identifier.visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE(CheckPrivateBrandVariant);

void CheckPrivateBrandVariant::dump(PrintStream& out) const
{
    dumpInContext(out, nullptr);
}

void CheckPrivateBrandVariant::dumpInContext(PrintStream& out, DumpContext* context) const
{
    out.print("<id='", m_identifier, "', ", inContext(structureSet(), context), ">");
}

} // namespace JSC
