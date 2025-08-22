/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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
#import "ScrollbarThemeMockMac.h"

#if PLATFORM(MAC)

#import "ScrollTypesMac.h"

#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

int ScrollbarThemeMockMac::scrollbarThickness(ScrollbarWidth scrollbarWidth, ScrollbarExpansionState expansionState, OverlayScrollbarSizeRelevancy overlayRelevancy)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (scrollbarWidth == ScrollbarWidth::None || (usesOverlayScrollbars() && overlayRelevancy == OverlayScrollbarSizeRelevancy::IgnoreOverlayScrollbarSize))
        return 0;
    NSScrollerImp *scrollerImp = [NSScrollerImp scrollerImpWithStyle:NSScrollerStyleOverlay controlSize:nsControlSizeFromScrollbarWidth(scrollbarWidth) horizontal:NO replacingScrollerImp:nil];
    [scrollerImp setExpanded:(expansionState == ScrollbarExpansionState::Expanded)];
    return [scrollerImp trackBoxWidth];
    END_BLOCK_OBJC_EXCEPTIONS
}


} // namespace WebCore

#endif // PLATFORM(MAC)
