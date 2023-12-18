/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#pragma once

#include "CacheableIdentifier.h"
#include "StructureSet.h"
#include <wtf/Box.h>
#include <wtf/TZoneMalloc.h>

namespace JSC {

class CheckPrivateBrandStatus;

class CheckPrivateBrandVariant {
    WTF_MAKE_TZONE_ALLOCATED(CheckPrivateBrandVariant);
public:
    CheckPrivateBrandVariant(CacheableIdentifier, const StructureSet& = StructureSet());

    ~CheckPrivateBrandVariant();

    const StructureSet& structureSet() const { return m_structureSet; }
    StructureSet& structureSet() { return m_structureSet; }

    bool attemptToMerge(const CheckPrivateBrandVariant& other);

    DECLARE_VISIT_AGGREGATE;
    template<typename Visitor> void markIfCheap(Visitor&);
    bool finalize(VM&);

    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;

    CacheableIdentifier identifier() const { return m_identifier; }

    bool overlaps(const CheckPrivateBrandVariant& other)
    {
        if (!!m_identifier != !!other.m_identifier)
            return true;
        if (m_identifier) {
            if (m_identifier != other.m_identifier)
                return false;
        }
        return structureSet().overlaps(other.structureSet());
    }

private:
    friend class CheckPrivateBrandStatus;

    StructureSet m_structureSet;
    CacheableIdentifier m_identifier;
};

} // namespace JSC
