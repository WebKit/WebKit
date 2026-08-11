/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ScrollbarMac.h"

#if PLATFORM(MAC)

#import "NSScrollerImpDetails.h"
#import "ScrollTypesMac.h"
#import "ScrollbarThemeMac.h"

#import <pal/spi/mac/NSScrollerImpSPI.h>

namespace WebCore {

ScrollbarMac::ScrollbarMac(ScrollableArea& scrollableArea, ScrollbarOrientation orientation, ScrollbarWidth width)
    : Scrollbar(scrollableArea, orientation, width)
{
    createScrollerImp();
}

NSScrollerImp* ScrollbarMac::scrollerImp() const
{
    return m_scrollerImp.get();
}

void ScrollbarMac::createScrollerImp(NSScrollerImp* oldScrollerImp)
{
    if (shouldRegisterScrollbar()) {
        m_scrollerImp = retainPtr([NSScrollerImp scrollerImpWithStyle:ScrollerStyle::recommendedScrollerStyle() controlSize:nsControlSizeFromScrollbarWidth(widthStyle()) horizontal:orientation() == ScrollbarOrientation::Horizontal replacingScrollerImp:oldScrollerImp]);
        updateScrollerImpState();
    }
}

void ScrollbarMac::updateScrollerImpState()
{
    theme().didCreateScrollerImp(*this);
    theme().updateEnabledState(*this);
    theme().updateScrollbarOverlayStyle(*this);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
