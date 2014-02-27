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
#import "FindIndicatorWindow.h"

#if PLATFORM(MAC)

#import "FindIndicator.h"
#import "WKView.h"
#import <WebCore/GraphicsContext.h>

static const double bounceAnimationDuration = 0.12;
static const double timeBeforeFadeStarts = bounceAnimationDuration + 0.2;
static const double fadeOutAnimationDuration = 0.3;

using namespace WebCore;

@interface WKFindIndicatorView : NSView {
    RefPtr<WebKit::FindIndicator> _findIndicator;
}

- (id)_initWithFindIndicator:(PassRefPtr<WebKit::FindIndicator>)findIndicator;
@end

@implementation WKFindIndicatorView

- (id)_initWithFindIndicator:(PassRefPtr<WebKit::FindIndicator>)findIndicator
{
    if ((self = [super initWithFrame:NSZeroRect]))
        _findIndicator = findIndicator;

    return self;
}

- (void)drawRect:(NSRect)rect
{
    GraphicsContext graphicsContext(static_cast<CGContextRef>([[NSGraphicsContext currentContext] graphicsPort]));

    _findIndicator->draw(graphicsContext, enclosingIntRect(rect));
}

- (BOOL)isFlipped
{
    return YES;
}

@end

@interface WKFindIndicatorWindowAnimation : NSAnimation<NSAnimationDelegate> {
    WebKit::FindIndicatorWindow* _findIndicatorWindow;
    void (WebKit::FindIndicatorWindow::*_animationProgressCallback)(double progress);
    void (WebKit::FindIndicatorWindow::*_animationDidEndCallback)();
}

- (id)_initWithFindIndicatorWindow:(WebKit::FindIndicatorWindow *)findIndicatorWindow 
                 animationDuration:(CFTimeInterval)duration
         animationProgressCallback:(void (WebKit::FindIndicatorWindow::*)(double progress))animationProgressCallback
           animationDidEndCallback:(void (WebKit::FindIndicatorWindow::*)())animationDidEndCallback;
@end

@implementation WKFindIndicatorWindowAnimation

- (id)_initWithFindIndicatorWindow:(WebKit::FindIndicatorWindow *)findIndicatorWindow 
                 animationDuration:(CFTimeInterval)animationDuration
         animationProgressCallback:(void (WebKit::FindIndicatorWindow::*)(double progress))animationProgressCallback
           animationDidEndCallback:(void (WebKit::FindIndicatorWindow::*)())animationDidEndCallback
{
    if ((self = [super initWithDuration:animationDuration animationCurve:NSAnimationEaseInOut])) {
        _findIndicatorWindow = findIndicatorWindow;
        _animationProgressCallback = animationProgressCallback;
        _animationDidEndCallback = animationDidEndCallback;
        [self setDelegate:self];
        [self setAnimationBlockingMode:NSAnimationNonblocking];
    }
    return self;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress
{
    (_findIndicatorWindow->*_animationProgressCallback)(progress);
}

- (void)animationDidEnd:(NSAnimation *)animation
{
    ASSERT(animation == self);

    (_findIndicatorWindow->*_animationDidEndCallback)();
}

@end

namespace WebKit {

FindIndicatorWindow::FindIndicatorWindow(WKView *wkView)
    : m_wkView(wkView)
    , m_bounceAnimationContext(0)
    , m_startFadeOutTimer(RunLoop::main(), this, &FindIndicatorWindow::startFadeOutTimerFired)
{
}

FindIndicatorWindow::~FindIndicatorWindow()
{
    closeWindow();
}

void FindIndicatorWindow::setFindIndicator(PassRefPtr<FindIndicator> findIndicator, bool fadeOut, bool animate)
{
    if (m_findIndicator == findIndicator)
        return;

    m_findIndicator = findIndicator;

    // Get rid of the old window.
    closeWindow();

    if (!m_findIndicator)
        return;

    NSRect contentRect = m_findIndicator->frameRect();
    NSRect windowFrameRect = NSIntegralRect([m_wkView convertRect:contentRect toView:nil]);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    windowFrameRect.origin = [[m_wkView window] convertBaseToScreen:windowFrameRect.origin];
#pragma clang diagnostic pop
    NSRect windowContentRect = [NSWindow contentRectForFrameRect:windowFrameRect styleMask:NSBorderlessWindowMask];
    
    m_findIndicatorWindow = adoptNS([[NSWindow alloc] initWithContentRect:windowContentRect 
                                                              styleMask:NSBorderlessWindowMask 
                                                                backing:NSBackingStoreBuffered
                                                                  defer:NO]);

    [m_findIndicatorWindow setBackgroundColor:[NSColor clearColor]];
    [m_findIndicatorWindow setOpaque:NO];
    [m_findIndicatorWindow setIgnoresMouseEvents:YES];

    RetainPtr<WKFindIndicatorView> findIndicatorView = adoptNS([[WKFindIndicatorView alloc] _initWithFindIndicator:m_findIndicator]);
    [m_findIndicatorWindow setContentView:findIndicatorView.get()];

    [[m_wkView window] addChildWindow:m_findIndicatorWindow.get() ordered:NSWindowAbove];
    [m_findIndicatorWindow setReleasedWhenClosed:NO];

    if (animate) {
        // Start the bounce animation.
        m_bounceAnimationContext = WKWindowBounceAnimationContextCreate(m_findIndicatorWindow.get());
        m_bounceAnimation = adoptNS([[WKFindIndicatorWindowAnimation alloc] _initWithFindIndicatorWindow:this
                                                                                    animationDuration:bounceAnimationDuration
                                                                            animationProgressCallback:&FindIndicatorWindow::bounceAnimationCallback
                                                                              animationDidEndCallback:&FindIndicatorWindow::bounceAnimationDidEnd]);
        [m_bounceAnimation startAnimation];
    }

    if (fadeOut)
        m_startFadeOutTimer.startOneShot(timeBeforeFadeStarts);
}

void FindIndicatorWindow::closeWindow()
{
    if (!m_findIndicatorWindow)
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
    
    [[m_findIndicatorWindow parentWindow] removeChildWindow:m_findIndicatorWindow.get()];
    [m_findIndicatorWindow close];
    m_findIndicatorWindow = nullptr;
}

void FindIndicatorWindow::startFadeOutTimerFired()
{
    ASSERT(!m_fadeOutAnimation);
    
    m_fadeOutAnimation = adoptNS([[WKFindIndicatorWindowAnimation alloc] _initWithFindIndicatorWindow:this
                                                                                   animationDuration:fadeOutAnimationDuration
                                                                           animationProgressCallback:&FindIndicatorWindow::fadeOutAnimationCallback
                                                                             animationDidEndCallback:&FindIndicatorWindow::fadeOutAnimationDidEnd]);
    [m_fadeOutAnimation startAnimation];
}
                        
void FindIndicatorWindow::fadeOutAnimationCallback(double progress)
{
    ASSERT(m_fadeOutAnimation);

    [m_findIndicatorWindow setAlphaValue:1.0 - progress];
}

void FindIndicatorWindow::fadeOutAnimationDidEnd()
{
    ASSERT(m_fadeOutAnimation);
    ASSERT(m_findIndicatorWindow);

    closeWindow();
}

void FindIndicatorWindow::bounceAnimationCallback(double progress)
{
    ASSERT(m_bounceAnimation);
    ASSERT(m_bounceAnimationContext);

    WKWindowBounceAnimationSetAnimationProgress(m_bounceAnimationContext, progress);
}

void FindIndicatorWindow::bounceAnimationDidEnd()
{
    ASSERT(m_bounceAnimation);
    ASSERT(m_bounceAnimationContext);
    ASSERT(m_findIndicatorWindow);

    WKWindowBounceAnimationContextDestroy(m_bounceAnimationContext);
    m_bounceAnimationContext = 0;
}

} // namespace WebKit

#endif // PLATFORM(MAC)
