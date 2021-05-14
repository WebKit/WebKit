/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "config.h"
#import "TextIndicatorWindow.h"

#if PLATFORM(MAC)

#import "GeometryUtilities.h"
#import "GraphicsContext.h"
#import "PathUtilities.h"
#import "TextIndicator.h"
#import "WebActionDisablingCALayerDelegate.h"
#import "WebTextIndicatorLayer.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/NSColorSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

using namespace WebCore;

namespace WebCore {

TextIndicatorWindow::TextIndicatorWindow(NSView *targetView)
    : m_targetView(targetView)
    , m_temporaryTextIndicatorTimer(RunLoop::main(), this, &TextIndicatorWindow::startFadeOut)
{
}

TextIndicatorWindow::~TextIndicatorWindow()
{
    clearTextIndicator(WebCore::TextIndicatorDismissalAnimation::FadeOut);
}

void TextIndicatorWindow::setAnimationProgress(float progress)
{
    if (!m_textIndicator)
        return;

    [m_textIndicatorLayer setAnimationProgress:progress];
}

void TextIndicatorWindow::clearTextIndicator(WebCore::TextIndicatorDismissalAnimation animation)
{
    RefPtr<TextIndicator> textIndicator = WTFMove(m_textIndicator);

    if ([m_textIndicatorLayer isFadingOut])
        return;

    if (textIndicator && [m_textIndicatorLayer indicatorWantsManualAnimation:*textIndicator] && [m_textIndicatorLayer hasCompletedAnimation] && animation == WebCore::TextIndicatorDismissalAnimation::FadeOut) {
        startFadeOut();
        return;
    }

    closeWindow();
}

void TextIndicatorWindow::setTextIndicator(Ref<TextIndicator> textIndicator, CGRect textBoundingRectInScreenCoordinates, TextIndicatorLifetime lifetime)
{
    if (m_textIndicator == textIndicator.ptr())
        return;

    closeWindow();

    m_textIndicator = textIndicator.ptr();

    CGFloat horizontalMargin = dropShadowBlurRadius * 2 + TextIndicator::defaultHorizontalMargin;
    CGFloat verticalMargin = dropShadowBlurRadius * 2 + TextIndicator::defaultVerticalMargin;
    
    if ([m_textIndicatorLayer indicatorWantsBounce:*m_textIndicator]) {
        horizontalMargin = std::max(horizontalMargin, textBoundingRectInScreenCoordinates.size.width * (WebCore::midBounceScale - 1) + horizontalMargin);
        verticalMargin = std::max(verticalMargin, textBoundingRectInScreenCoordinates.size.height * (WebCore::midBounceScale - 1) + verticalMargin);
    }

    horizontalMargin = CGCeiling(horizontalMargin);
    verticalMargin = CGCeiling(verticalMargin);

    CGRect contentRect = CGRectInset(textBoundingRectInScreenCoordinates, -horizontalMargin, -verticalMargin);
    NSRect windowContentRect = [NSWindow contentRectForFrameRect:NSRectFromCGRect(contentRect) styleMask:NSWindowStyleMaskBorderless];
    NSRect integralWindowContentRect = NSIntegralRect(windowContentRect);
    NSPoint fractionalTextOffset = NSMakePoint(windowContentRect.origin.x - integralWindowContentRect.origin.x, windowContentRect.origin.y - integralWindowContentRect.origin.y);
    m_textIndicatorWindow = adoptNS([[NSWindow alloc] initWithContentRect:integralWindowContentRect styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]);
    [m_textIndicatorWindow setBackgroundColor:[NSColor clearColor]];
    [m_textIndicatorWindow setOpaque:NO];
    [m_textIndicatorWindow setIgnoresMouseEvents:YES];

    NSRect frame = NSMakeRect(0, 0, [m_textIndicatorWindow frame].size.width, [m_textIndicatorWindow frame].size.height);
    m_textIndicatorLayer = adoptNS([[WebTextIndicatorLayer alloc] initWithFrame:frame
        textIndicator:*m_textIndicator margin:NSMakeSize(horizontalMargin, verticalMargin) offset:fractionalTextOffset]);
    m_textIndicatorView = adoptNS([[NSView alloc] initWithFrame:frame]);
    [m_textIndicatorView setLayer:m_textIndicatorLayer.get()];
    [m_textIndicatorView setWantsLayer:YES];
    [m_textIndicatorWindow setContentView:m_textIndicatorView.get()];

    [[m_targetView window] addChildWindow:m_textIndicatorWindow.get() ordered:NSWindowAbove];
    [m_textIndicatorWindow setReleasedWhenClosed:NO];

    if (m_textIndicator->presentationTransition() != TextIndicatorPresentationTransition::None)
        [m_textIndicatorLayer present];

    if (lifetime == TextIndicatorLifetime::Temporary)
        m_temporaryTextIndicatorTimer.startOneShot(WebCore::timeBeforeFadeStarts);
}

void TextIndicatorWindow::closeWindow()
{
    if (!m_textIndicatorWindow)
        return;

    if ([m_textIndicatorLayer isFadingOut])
        return;

    m_temporaryTextIndicatorTimer.stop();

    [[m_textIndicatorWindow parentWindow] removeChildWindow:m_textIndicatorWindow.get()];
    [m_textIndicatorWindow close];
    m_textIndicatorWindow = nullptr;
}

void TextIndicatorWindow::startFadeOut()
{
    [m_textIndicatorLayer setFadingOut:YES];
    RetainPtr<NSWindow> indicatorWindow = m_textIndicatorWindow;
    [m_textIndicatorLayer hideWithCompletionHandler:[indicatorWindow] {
        [[indicatorWindow parentWindow] removeChildWindow:indicatorWindow.get()];
        [indicatorWindow close];
    }];
}

} // namespace WebCore

#endif // PLATFORM(MAC)
