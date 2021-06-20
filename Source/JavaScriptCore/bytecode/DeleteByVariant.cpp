/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "DeleteByVariant.h"

#include "CacheableIdentifierInlines.h"

namespace JSC {

DeleteByVariant::DeleteByVariant(CacheableIdentifier identifier, bool result, Structure* oldStructure, Structure* newStructure, PropertyOffset offset)
    : m_result(result)
    , m_oldStructure(oldStructure)
    , m_newStructure(newStructure)
    , m_offset(offset)
    , m_identifier(WTFMove(identifier))
{
    ASSERT(oldStructure);
    if (m_offset == invalidOffset)
        ASSERT(!newStructure);
    else
        ASSERT(newStructure);
}

DeleteByVariant::~DeleteByVariant() { }

DeleteByVariant::DeleteByVariant(const DeleteByVariant& other)
{
    *this = other;
}

DeleteByVariant& DeleteByVariant::operator=(const DeleteByVariant& other)
{
    m_identifier = other.m_identifier;
    m_result = other.m_result;
    m_oldStructure = other.m_oldStructure;
    m_newStructure = other.m_newStructure;
    m_offset = other.m_offset;
    return *this;
}

bool DeleteByVariant::attemptToMerge(const DeleteByVariant& other)
{
    if (!!m_identifier != !!other.m_identifier)
        return false;

    if (m_result != other.m_result)
        return false;

    if (m_identifier && (m_identifier != other.m_identifier))
        return false;

    if (m_offset != other.m_offset)
        return false;

    if (m_oldStructure != other.m_oldStructure)
        return false;
    ASSERT(m_newStructure == other.m_newStructure);

    return true;
}

bool DeleteByVariant::writesStructures() const
{
    return !!newStructure();
}

template<typename Visitor>
void DeleteByVariant::visitAggregateImpl(Visitor& visitor)
{
    m_identifier.visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE(DeleteByVariant);

template<typename Visitor>
void DeleteByVariant::markIfCheap(Visitor& visitor)
{
    if (m_oldStructure)
        m_oldStructure->markIfCheap(visitor);
    if (m_newStructure)
        m_newStructure->markIfCheap(visitor);
}

template void DeleteByVariant::markIfCheap(AbstractSlotVisitor&);
template void DeleteByVariant::markIfCheap(SlotVisitor&);

void DeleteByVariant::dump(PrintStream& out) const
{
    dumpInContext(out, nullptr);
}

bool DeleteByVariant::finalize(VM& vm)
{
    if (!vm.heap.isMarked(m_oldStructure))
        return false;
    if (m_newStructure && !vm.heap.isMarked(m_newStructure))
        return false;
    return true;
}

void DeleteByVariant::dumpInContext(PrintStream& out, DumpContext*) const
{
    out.print("<");
    out.print("id='", m_identifier, "', result=", m_result);
    if (m_oldStructure)
        out.print(", ", *m_oldStructure);
    if (m_newStructure)
        out.print(" -> ", *m_newStructure);
    out.print(", offset = ", offset());
    out.print(">");
}

} // namespace JSC

