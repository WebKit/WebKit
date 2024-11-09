/*
 * Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AccessibilityScrollbar.h"

#include "AXObjectCache.h"
#include "LocalFrameView.h"
#include "ScrollView.h"
#include "Scrollbar.h"

namespace WebCore {

AccessibilityScrollbar::AccessibilityScrollbar(AXID axID, Scrollbar& scrollbar)
    : AccessibilityMockObject(axID)
    , m_scrollbar(scrollbar)
{
}

Ref<AccessibilityScrollbar> AccessibilityScrollbar::create(AXID axID, Scrollbar& scrollbar)
{
    return adoptRef(*new AccessibilityScrollbar(axID, scrollbar));
}
    
LayoutRect AccessibilityScrollbar::elementRect() const
{
    return m_scrollbar->frameRect();
}
    
Document* AccessibilityScrollbar::document() const
{
    RefPtr parent = parentObject();
    return parent ? parent->document() : nullptr;
}

AccessibilityOrientation AccessibilityScrollbar::orientation() const
{
    // ARIA 1.1 Elements with the role scrollbar have an implicit aria-orientation value of vertical.
    if (m_scrollbar->orientation() == ScrollbarOrientation::Horizontal)
        return AccessibilityOrientation::Horizontal;
    if (m_scrollbar->orientation() == ScrollbarOrientation::Vertical)
        return AccessibilityOrientation::Vertical;

    return AccessibilityOrientation::Vertical;
}

bool AccessibilityScrollbar::isEnabled() const
{
    return m_scrollbar->enabled();
}
    
float AccessibilityScrollbar::valueForRange() const
{
    return m_scrollbar->currentPos() / m_scrollbar->maximum();
}

bool AccessibilityScrollbar::setValue(float value)
{
    float newValue = value * m_scrollbar->maximum();
    m_scrollbar->scrollableArea().scrollToOffsetWithoutAnimation(m_scrollbar->orientation(), newValue);
    return true;
}
    
} // namespace WebCore
