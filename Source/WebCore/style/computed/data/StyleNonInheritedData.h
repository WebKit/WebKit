/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <wtf/DataRef.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {
namespace Style {

class BackgroundData;
class BoxData;
class NonInheritedMiscData;
class NonInheritedRareData;
class SurroundData;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(NonInheritedData);
class NonInheritedData : public RefCounted<NonInheritedData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(NonInheritedData, NonInheritedData);
public:
    static Ref<NonInheritedData> create();
    Ref<NonInheritedData> copy() const;

    bool operator==(const NonInheritedData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const NonInheritedData&) const;
#endif

    DataRef<BoxData> boxData;
    DataRef<BackgroundData> backgroundData;
    DataRef<SurroundData> surroundData;
    DataRef<NonInheritedMiscData> miscData;
    DataRef<NonInheritedRareData> rareData;

private:
    NonInheritedData();
    NonInheritedData(const NonInheritedData&);
};

} // namespace Style
} // namespace WebCore
