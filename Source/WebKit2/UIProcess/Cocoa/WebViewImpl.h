/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef WebViewImpl_h
#define WebViewImpl_h

#if PLATFORM(MAC)

#import "WKLayoutMode.h"
#import <wtf/RetainPtr.h>

OBJC_CLASS NSTextInputContext;
OBJC_CLASS NSView;
OBJC_CLASS WKFullScreenWindowController;
OBJC_CLASS WKViewLayoutStrategy;

@protocol WebViewImplDelegate

- (NSTextInputContext *)_superInputContext;

@end

namespace WebKit {

class WebPageProxy;

class WebViewImpl {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(WebViewImpl);
public:
    WebViewImpl(NSView <WebViewImplDelegate> *, WebPageProxy&);

    ~WebViewImpl();

    void setDrawsBackground(bool);
    bool drawsBackground() const;
    void setDrawsTransparentBackground(bool);
    bool drawsTransparentBackground() const;

    bool acceptsFirstResponder();
    bool becomeFirstResponder();
    bool resignFirstResponder();
    bool isFocused() const;

    void viewWillStartLiveResize();
    void viewDidEndLiveResize();

    void setFrameSize(CGSize);
    void disableFrameSizeUpdates();
    void enableFrameSizeUpdates();
    bool frameSizeUpdatesDisabled() const;
    void setFrameAndScrollBy(CGRect, CGSize);

    void setFixedLayoutSize(CGSize);
    CGSize fixedLayoutSize() const;

    void setDrawingAreaSize(CGSize);

    void setContentPreparationRect(CGRect);
    void updateViewExposedRect();
    void setClipsToVisibleRect(bool);
    bool clipsToVisibleRect() const { return m_clipsToVisibleRect; }

    void setIntrinsicContentSize(CGSize);
    CGSize intrinsicContentSize() const;

    void setViewScale(CGFloat);
    CGFloat viewScale() const;

    WKLayoutMode layoutMode() const;
    void setLayoutMode(WKLayoutMode);
    void updateSupportsArbitraryLayoutModes();

    void updateSecureInputState();
    void resetSecureInputState();
    bool inSecureInputState() const { return m_inSecureInputState; }
    void notifyInputContextAboutDiscardedComposition();

#if ENABLE(FULLSCREEN_API)
    bool hasFullScreenWindowController() const;
    WKFullScreenWindowController *fullScreenWindowController();
    void closeFullScreenWindowController();
#endif
    NSView *fullScreenPlaceholderView();
    NSWindow *createFullScreenWindow();

private:
    bool supportsArbitraryLayoutModes() const;

    NSView <WebViewImplDelegate> *m_view;
    WebPageProxy& m_page;

    bool m_willBecomeFirstResponderAgain { false };
    bool m_inBecomeFirstResponder { false };
    bool m_inResignFirstResponder { false };

    CGRect m_contentPreparationRect;
    bool m_useContentPreparationRectForVisibleRect { false };
    bool m_clipsToVisibleRect { false };

    CGSize m_resizeScrollOffset;

    CGSize m_intrinsicContentSize;

    RetainPtr<WKViewLayoutStrategy> m_layoutStrategy;
    WKLayoutMode m_lastRequestedLayoutMode { kWKLayoutModeViewSize };
    CGFloat m_lastRequestedViewScale { 1 };

    bool m_inSecureInputState { false };

#if ENABLE(FULLSCREEN_API)
    RetainPtr<WKFullScreenWindowController> m_fullScreenWindowController;
#endif
};
    
} // namespace WebKit

#endif // PLATFORM(MAC)

#endif // WebViewImpl_h
