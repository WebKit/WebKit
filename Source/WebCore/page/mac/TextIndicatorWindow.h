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

#ifndef TextIndicatorWindow_h
#define TextIndicatorWindow_h

#if PLATFORM(MAC)

#import <functional>
#import <wtf/Noncopyable.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>

@class NSView;
@class WebTextIndicatorView;

namespace WebCore {

class TextIndicator;

class TextIndicatorWindow {
    WTF_MAKE_NONCOPYABLE(TextIndicatorWindow);

public:
    explicit TextIndicatorWindow(NSView *);
    ~TextIndicatorWindow();

    void setTextIndicator(PassRefPtr<TextIndicator>, bool fadeOut, std::function<void ()> animationCompletionHandler);

private:
    void closeWindow();

    void startFadeOutTimerFired();

    NSView *m_targetView;
    RefPtr<TextIndicator> m_textIndicator;
    RetainPtr<NSWindow> m_textIndicatorWindow;
    RetainPtr<WebTextIndicatorView> m_textIndicatorView;

    RunLoop::Timer<TextIndicatorWindow> m_startFadeOutTimer;

    std::function<void ()> m_bounceAnimationCompletionHandler;
};

} // namespace WebKit

#endif // TextIndicatorWindow_h

#endif // PLATFORM(MAC)
