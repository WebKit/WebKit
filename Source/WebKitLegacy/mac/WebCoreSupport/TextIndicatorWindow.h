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

#import <WebCore/TextIndicator.h>
#import <wtf/CheckedPtr.h>
#import <wtf/Noncopyable.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/WeakObjCPtr.h>

OBJC_CLASS NSView;
OBJC_CLASS WebTextIndicatorLayer;

#if PLATFORM(MAC)

class TextIndicatorWindow final : public CanMakeCheckedPtr<TextIndicatorWindow> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(TextIndicatorWindow, WEBCORE_EXPORT);
    WTF_MAKE_NONCOPYABLE(TextIndicatorWindow);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(TextIndicatorWindow);
public:
    explicit TextIndicatorWindow(NSView *);
    ~TextIndicatorWindow();

    void setTextIndicator(Ref<WebCore::TextIndicator>, CGRect contentRect, WebCore::TextIndicatorLifetime);
    void updateTextIndicator(Ref<WebCore::TextIndicator>&&, CGRect contentRect);
    void clearTextIndicator(WebCore::TextIndicatorDismissalAnimation);

    void setAnimationProgress(float);

private:
    void closeWindow();

    void startFadeOut();

    WeakObjCPtr<NSView> m_targetView;
    RefPtr<WebCore::TextIndicator> m_textIndicator;
    RetainPtr<NSWindow> m_textIndicatorWindow;
    RetainPtr<NSView> m_textIndicatorView;
    RetainPtr<WebTextIndicatorLayer> m_textIndicatorLayer;

    RunLoop::Timer m_temporaryTextIndicatorTimer;
};

#endif // PLATFORM(MAC)


