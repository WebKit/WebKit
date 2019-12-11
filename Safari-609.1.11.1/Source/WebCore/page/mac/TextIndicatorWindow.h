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

#pragma once

#import "TextIndicator.h"
#import <wtf/Noncopyable.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>

OBJC_CLASS NSView;
OBJC_CLASS WebTextIndicatorView;

namespace WebCore {

enum class TextIndicatorWindowLifetime : uint8_t {
    // The TextIndicator should indicate the text until dismissed.
    Permanent,

    // The TextIndicator should briefly indicate the text and then automatically dismiss.
    Temporary
};

enum class TextIndicatorWindowDismissalAnimation : uint8_t {
    None,
    FadeOut
};

#if PLATFORM(MAC)

class TextIndicatorWindow {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(TextIndicatorWindow);

public:
    WEBCORE_EXPORT explicit TextIndicatorWindow(NSView *);
    WEBCORE_EXPORT ~TextIndicatorWindow();

    WEBCORE_EXPORT void setTextIndicator(Ref<TextIndicator>, CGRect contentRect, TextIndicatorWindowLifetime);
    WEBCORE_EXPORT void clearTextIndicator(TextIndicatorWindowDismissalAnimation);

    WEBCORE_EXPORT void setAnimationProgress(float);

private:
    void closeWindow();

    void startFadeOut();

    NSView *m_targetView;
    RefPtr<TextIndicator> m_textIndicator;
    RetainPtr<NSWindow> m_textIndicatorWindow;
    RetainPtr<WebTextIndicatorView> m_textIndicatorView;

    RunLoop::Timer<TextIndicatorWindow> m_temporaryTextIndicatorTimer;
};

#endif // PLATFORM(MAC)

} // namespace WebCore
