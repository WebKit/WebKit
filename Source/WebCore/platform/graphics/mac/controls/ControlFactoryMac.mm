/*
 * Copyright (C) 2022 Apple Inc. All Rights Reserved.
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
#import "ControlFactoryMac.h"

#if PLATFORM(MAC)

#import "MeterMac.h"
#import <pal/spi/mac/NSViewSPI.h>

namespace WebCore {

std::unique_ptr<ControlFactory> ControlFactory::createControlFactory()
{
    return makeUnique<ControlFactoryMac>();
}

NSView *ControlFactoryMac::drawingView(const FloatRect& rect, const ControlStyle& style) const
{
    if (!m_drawingView)
        m_drawingView = adoptNS([[WebControlView alloc] init]);

    // Use a fake view.
    [m_drawingView setFrameSize:NSSizeFromCGSize(rect.size())];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [m_drawingView setAppearance:[NSAppearance currentAppearance]];
    ALLOW_DEPRECATED_DECLARATIONS_END
#if USE(NSVIEW_SEMANTICCONTEXT)
    if (style.states.contains(ControlStyle::State::FormSemanticContext))
        [m_drawingView _setSemanticContext:NSViewSemanticContextForm];
#else
    UNUSED_PARAM(style);
#endif
    return m_drawingView.get();
}

NSLevelIndicatorCell* ControlFactoryMac::levelIndicatorCell() const
{
    if (!m_levelIndicatorCell) {
        m_levelIndicatorCell = adoptNS([[NSLevelIndicatorCell alloc] initWithLevelIndicatorStyle:NSLevelIndicatorStyleContinuousCapacity]);
        [m_levelIndicatorCell setLevelIndicatorStyle:NSLevelIndicatorStyleContinuousCapacity];
    }
    return m_levelIndicatorCell.get();
}

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformMeter(MeterPart& part)
{
    return makeUnique<MeterMac>(part, *this, levelIndicatorCell());
}

} // namespace WebCore

#endif // PLATFORM(MAC)
