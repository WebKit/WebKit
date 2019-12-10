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

#include "config.h"
#include "HighlightRangeGroup.h"

#include "IDLTypes.h"
#include "JSDOMSetLike.h"
#include "JSStaticRange.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "StaticRange.h"
#include "StyleProperties.h"

namespace WebCore {

HighlightRangeGroup::HighlightRangeGroup(Ref<StaticRange>&& range)
{
    m_ranges.append(WTFMove(range));
}

Ref<HighlightRangeGroup> HighlightRangeGroup::create(StaticRange& range)
{
    return adoptRef(*new HighlightRangeGroup(range));
}

void HighlightRangeGroup::initializeSetLike(DOMSetAdapter& set)
{
    for (auto& range : m_ranges)
        set.add<IDLInterface<StaticRange>>(range);
}

bool HighlightRangeGroup::removeFromSetLike(const StaticRange& range)
{
    return m_ranges.removeFirstMatching([&range](const Ref<StaticRange>& current) {
        return current.get() == range;
    });
}

void HighlightRangeGroup::clearFromSetLike()
{
    m_ranges.clear();
}

bool HighlightRangeGroup::addToSetLike(StaticRange& range)
{
    if (notFound != m_ranges.findMatching([&range](const Ref<StaticRange>& current) { return current.get() == range; }))
        return false;
    m_ranges.append(makeRef(range));
    return true;
}

}

