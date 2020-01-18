/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ExceptionOr.h"
#include "Position.h"
#include "StaticRange.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class CSSStyleDeclaration;
class DOMSetAdapter;
class StaticRange;
class PropertySetCSSStyleDeclaration;

struct HighlightRangeData : RefCounted<HighlightRangeData>, public CanMakeWeakPtr<HighlightRangeData> {
    
    HighlightRangeData(Ref<StaticRange>&& range)
        : range(WTFMove(range))
    {
    }
    
    static Ref<HighlightRangeData> create(Ref<StaticRange>&& range)
    {
        return adoptRef(*new HighlightRangeData(WTFMove(range)));
    }
    
    Ref<StaticRange> range;
    Optional<Position> startPosition;
    Optional<Position> endPosition;
};

class HighlightRangeGroup : public RefCounted<HighlightRangeGroup> {
public:
    static Ref<HighlightRangeGroup> create(StaticRange&);
    
    void clearFromSetLike();
    bool addToSetLike(StaticRange&);
    bool removeFromSetLike(const StaticRange&);
    void initializeSetLike(DOMSetAdapter&);
    
    const Vector<Ref<HighlightRangeData>>& rangesData() const { return m_rangesData; }

    // FIXME: Add WEBCORE_EXPORT CSSStyleDeclaration& style();
    
private:
    Vector<Ref<HighlightRangeData>> m_rangesData; // FIXME: use a HashSet instead of a Vector <rdar://problem/57760614>
    explicit HighlightRangeGroup(Ref<StaticRange>&&);
};

}

