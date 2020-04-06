/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "CacheableIdentifier.h"
#include "CallLinkStatus.h"
#include "ObjectPropertyConditionSet.h"
#include "PropertyOffset.h"
#include "StructureSet.h"
#include <wtf/Box.h>

namespace JSC {

class CallLinkStatus;
class DeleteByStatus;
struct DumpContext;

class DeleteByIdVariant {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DeleteByIdVariant(
        CacheableIdentifier, bool result,
        Structure* oldStrucutre, Structure* newStructure, PropertyOffset);

    ~DeleteByIdVariant();

    DeleteByIdVariant(const DeleteByIdVariant&);
    DeleteByIdVariant& operator=(const DeleteByIdVariant&);

    Structure* oldStructure() const { return m_oldStructure; }
    Structure* newStructure() const { return m_newStructure; }
    bool result() const { return m_result; }
    bool writesStructures() const;

    PropertyOffset offset() const { return m_offset; }

    bool isPropertyUnset() const { return offset() == invalidOffset; }

    bool attemptToMerge(const DeleteByIdVariant& other);

    void visitAggregate(SlotVisitor&);
    void markIfCheap(SlotVisitor&);
    bool finalize(VM&);

    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;

    CacheableIdentifier identifier() const { return m_identifier; }

    bool overlaps(const DeleteByIdVariant& other)
    {
        if (!!m_identifier != !!other.m_identifier)
            return true;
        if (m_identifier) {
            if (m_identifier != other.m_identifier)
                return false;
        }
        return m_oldStructure == other.m_oldStructure;
    }

private:
    friend class DeleteByStatus;

    bool m_result;
    Structure* m_oldStructure;
    Structure* m_newStructure;
    PropertyOffset m_offset;
    CacheableIdentifier m_identifier;
};

} // namespace JSC
