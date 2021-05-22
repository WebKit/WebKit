/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "PageClient.h"
#include <wtf/Forward.h>
#include <wtf/WeakObjCPtr.h>

@class NSTextAlternatives;
@class WKWebView;

namespace API {
class Attachment;
}

namespace WebCore {
class AlternativeTextUIController;

struct AppHighlight;
}

namespace WebKit {

class PageClientImplCocoa : public PageClient {
public:
    PageClientImplCocoa(WKWebView *);
    virtual ~PageClientImplCocoa();

    void pageClosed() override;

    void themeColorWillChange() final;
    void themeColorDidChange() final;
    void pageExtendedBackgroundColorWillChange() final;
    void pageExtendedBackgroundColorDidChange() final;
    void sampledPageTopColorWillChange() final;
    void sampledPageTopColorDidChange() final;
    void isPlayingAudioWillChange() final;
    void isPlayingAudioDidChange() final;

    bool scrollingUpdatesDisabledForTesting() final;

#if ENABLE(ATTACHMENT_ELEMENT)
    void didInsertAttachment(API::Attachment&, const String& source) final;
    void didRemoveAttachment(API::Attachment&) final;
    void didInvalidateDataForAttachment(API::Attachment&) final;
    NSFileWrapper *allocFileWrapperInstance() const final;
    NSSet *serializableFileWrapperClasses() const final;
#endif

    WebCore::DictationContext addDictationAlternatives(NSTextAlternatives *) final;
    void removeDictationAlternatives(WebCore::DictationContext) final;
    Vector<String> dictationAlternatives(WebCore::DictationContext) final;
    NSTextAlternatives *platformDictationAlternatives(WebCore::DictationContext) final;

#if ENABLE(APP_HIGHLIGHTS)
    void storeAppHighlight(const WebCore::AppHighlight&) final;
#endif

    void microphoneCaptureWillChange() final;
    void cameraCaptureWillChange() final;
    void microphoneCaptureChanged() final;
    void cameraCaptureChanged() final;

protected:
    WeakObjCPtr<WKWebView> m_webView;
    std::unique_ptr<WebCore::AlternativeTextUIController> m_alternativeTextUIController;
};

}
