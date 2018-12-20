/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include <wtf/Optional.h>

namespace WebCore {
namespace Layout {

struct VerticalMargin {
    struct ComputedValues {
        LayoutUnit before;
        LayoutUnit after;
    };
    ComputedValues usedValues() const;
    ComputedValues nonCollapsedValues() const { return m_nonCollapsed; }
    
    struct CollapsedValues {
        Optional<LayoutUnit> before;
        Optional<LayoutUnit> after;
    };
    Optional<CollapsedValues> collapsedValues() const { return m_collapsed; }
    void setCollapsedValues(CollapsedValues collapsedValues) { m_collapsed = collapsedValues; }

    VerticalMargin(ComputedValues nonCollapsed, Optional<CollapsedValues>);

    VerticalMargin() = default;
    ~VerticalMargin() = default;
private:
    ComputedValues m_nonCollapsed;
    Optional<CollapsedValues> m_collapsed;
};

struct HorizontalMargin {
    LayoutUnit start;
    LayoutUnit end;
};

struct PositiveAndNegativeVerticalMargin {
    struct Values {
        Optional<LayoutUnit> positive;
        Optional<LayoutUnit> negative;
    };
    Values before;
    Values after;
};

inline VerticalMargin::VerticalMargin(VerticalMargin::ComputedValues nonCollapsed, Optional<VerticalMargin::CollapsedValues> collapsed)
    : m_nonCollapsed(nonCollapsed)
    , m_collapsed(collapsed)
{
}

inline VerticalMargin::ComputedValues VerticalMargin::usedValues() const
{
    if (!m_collapsed)
        return m_nonCollapsed;
    return { m_collapsed->before.valueOr(m_nonCollapsed.before),
        m_collapsed->after.valueOr(m_nonCollapsed.after) };
}

}
}
#endif
