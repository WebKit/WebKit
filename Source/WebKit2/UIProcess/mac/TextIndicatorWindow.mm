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

#import "TextIndicator.h"
#import "WKView.h"
#import <WebCore/GraphicsContext.h>

static const double bounceAnimationDuration = 0.12;
static const double timeBeforeFadeStarts = bounceAnimationDuration + 0.2;
static const double fadeOutAnimationDuration = 0.3;

using namespace WebCore;

@interface WKTextIndicatorView : NSView {
    RefPtr<WebKit::TextIndicator> _textIndicator;
}

- (id)_initWithTextIndicator:(PassRefPtr<WebKit::TextIndicator>)textIndicator;
@end

@implementation WKTextIndicatorView

- (id)_initWithTextIndicator:(PassRefPtr<WebKit::TextIndicator>)textIndicator
{
    if ((self = [super initWithFrame:NSZeroRect]))
        _textIndicator = textIndicator;

    return self;
}

- (void)drawRect:(NSRect)rect
{
    GraphicsContext graphicsContext(static_cast<CGContextRef>([[NSGraphicsContext currentContext] graphicsPort]));

    _textIndicator->draw(graphicsContext, enclosingIntRect(rect));
}

- (BOOL)isFlipped
{
    return YES;
}

@end

@interface WKTextIndicatorWindowAnimation : NSAnimation<NSAnimationDelegate> {
    WebKit::TextIndicatorWindow* _textIndicatorWindow;
    void (WebKit::TextIndicatorWindow::*_animationProgressCallback)(double progress);
    void (WebKit::TextIndicatorWindow::*_animationDidEndCallback)();
}

- (id)_initWithTextIndicatorWindow:(WebKit::TextIndicatorWindow *)textIndicatorWindow animationDuration:(CFTimeInterval)duration animationProgressCallback:(void (WebKit::TextIndicatorWindow::*)(double progress))animationProgressCallback animationDidEndCallback:(void (WebKit::TextIndicatorWindow::*)())animationDidEndCallback;
@end

@implementation WKTextIndicatorWindowAnimation

- (id)_initWithTextIndicatorWindow:(WebKit::TextIndicatorWindow *)textIndicatorWindow animationDuration:(CFTimeInterval)animationDuration animationProgressCallback:(void (WebKit::TextIndicatorWindow::*)(double progress))animationProgressCallback animationDidEndCallback:(void (WebKit::TextIndicatorWindow::*)())animationDidEndCallback
{
    if ((self = [super initWithDuration:animationDuration animationCurve:NSAnimationEaseInOut])) {
        _textIndicatorWindow = textIndicatorWindow;
        _animationProgressCallback = animationProgressCallback;
        _animationDidEndCallback = animationDidEndCallback;
        [self setDelegate:self];
        [self setAnimationBlockingMode:NSAnimationNonblocking];
    }
    return self;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress
{
    (_textIndicatorWindow->*_animationProgressCallback)(progress);
}

- (void)animationDidEnd:(NSAnimation *)animation
{
    ASSERT(animation == self);

    (_textIndicatorWindow->*_animationDidEndCallback)();
}

@end

namespace WebKit {

TextIndicatorWindow::TextIndicatorWindow(WKView *wkView)
    : m_wkView(wkView)
    , m_bounceAnimationContext(0)
    , m_startFadeOutTimer(RunLoop::main(), this, &TextIndicatorWindow::startFadeOutTimerFired)
{
}

TextIndicatorWindow::~TextIndicatorWindow()
{
    closeWindow();
}

void TextIndicatorWindow::setTextIndicator(PassRefPtr<TextIndicator> textIndicator, bool fadeOut, bool animate)
{
    if (m_textIndicator == textIndicator)
        return;

    m_textIndicator = textIndicator;

    // Get rid of the old window.
    closeWindow();

    if (!m_textIndicator)
        return;

    NSRect contentRect = m_textIndicator->frameRect();
    NSRect windowFrameRect = NSIntegralRect([m_wkView convertRect:contentRect toView:nil]);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    windowFrameRect.origin = [[m_wkView window] convertBaseToScreen:windowFrameRect.origin];
#pragma clang diagnostic pop
    NSRect windowContentRect = [NSWindow contentRectForFrameRect:windowFrameRect styleMask:NSBorderlessWindowMask];
    
    m_textIndicatorWindow = adoptNS([[NSWindow alloc] initWithContentRect:windowContentRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);

    [m_textIndicatorWindow setBackgroundColor:[NSColor clearColor]];
    [m_textIndicatorWindow setOpaque:NO];
    [m_textIndicatorWindow setIgnoresMouseEvents:YES];

    RetainPtr<WKTextIndicatorView> textIndicatorView = adoptNS([[WKTextIndicatorView alloc] _initWithTextIndicator:m_textIndicator]);
    [m_textIndicatorWindow setContentView:textIndicatorView.get()];

    [[m_wkView window] addChildWindow:m_textIndicatorWindow.get() ordered:NSWindowAbove];
    [m_textIndicatorWindow setReleasedWhenClosed:NO];

    if (animate) {
        // Start the bounce animation.
        m_bounceAnimationContext = WKWindowBounceAnimationContextCreate(m_textIndicatorWindow.get());
        m_bounceAnimation = adoptNS([[WKTextIndicatorWindowAnimation alloc] _initWithTextIndicatorWindow:this animationDuration:bounceAnimationDuration
            animationProgressCallback:&TextIndicatorWindow::bounceAnimationCallback animationDidEndCallback:&TextIndicatorWindow::bounceAnimationDidEnd]);
        [m_bounceAnimation startAnimation];
    }

    if (fadeOut)
        m_startFadeOutTimer.startOneShot(timeBeforeFadeStarts);
}

void TextIndicatorWindow::closeWindow()
{
    if (!m_textIndicatorWindow)
        return;

    m_startFadeOutTimer.stop();

    if (m_fadeOutAnimation) {
        [m_fadeOutAnimation stopAnimation];
        m_fadeOutAnimation = nullptr;
    }

    if (m_bounceAnimation) {
        [m_bounceAnimation stopAnimation];
        m_bounceAnimation = nullptr;
    }

    if (m_bounceAnimationContext)
        WKWindowBounceAnimationContextDestroy(m_bounceAnimationContext);
    
    [[m_textIndicatorWindow parentWindow] removeChildWindow:m_textIndicatorWindow.get()];
    [m_textIndicatorWindow close];
    m_textIndicatorWindow = nullptr;
}

void TextIndicatorWindow::startFadeOutTimerFired()
{
    ASSERT(!m_fadeOutAnimation);
    
    m_fadeOutAnimation = adoptNS([[WKTextIndicatorWindowAnimation alloc] _initWithTextIndicatorWindow:this animationDuration:fadeOutAnimationDuration
        animationProgressCallback:&TextIndicatorWindow::fadeOutAnimationCallback animationDidEndCallback:&TextIndicatorWindow::fadeOutAnimationDidEnd]);
    [m_fadeOutAnimation startAnimation];
}

void TextIndicatorWindow::fadeOutAnimationCallback(double progress)
{
    ASSERT(m_fadeOutAnimation);

    [m_textIndicatorWindow setAlphaValue:1.0 - progress];
}

void TextIndicatorWindow::fadeOutAnimationDidEnd()
{
    ASSERT(m_fadeOutAnimation);
    ASSERT(m_textIndicatorWindow);

    closeWindow();
}

void TextIndicatorWindow::bounceAnimationCallback(double progress)
{
    ASSERT(m_bounceAnimation);
    ASSERT(m_bounceAnimationContext);

    WKWindowBounceAnimationSetAnimationProgress(m_bounceAnimationContext, progress);
}

void TextIndicatorWindow::bounceAnimationDidEnd()
{
    ASSERT(m_bounceAnimation);
    ASSERT(m_bounceAnimationContext);
    ASSERT(m_textIndicatorWindow);

    WKWindowBounceAnimationContextDestroy(m_bounceAnimationContext);
    m_bounceAnimationContext = 0;
}

} // namespace WebKit

#endif // PLATFORM(MAC)
