/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if USE(SYSTEM_PREVIEW)

#include "ProcessThrottler.h"
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/IntRect.h>
#include <WebCore/ResourceError.h>
#include <wtf/BlockPtr.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSString;
#if USE(QUICK_LOOK)
OBJC_CLASS QLPreviewController;
OBJC_CLASS _WKPreviewControllerDataSource;
OBJC_CLASS _WKPreviewControllerDelegate;
OBJC_CLASS _WKSystemPreviewDataTaskDelegate;
#endif

namespace WebCore {
class SecurityOriginData;
}

namespace WebKit {

class WebPageProxy;

class SystemPreviewController : public RefCountedAndCanMakeWeakPtr<SystemPreviewController> {
    WTF_MAKE_TZONE_ALLOCATED(SystemPreviewController);
public:
    static Ref<SystemPreviewController> create(WebPageProxy&);

    bool canPreview(const String& mimeType) const;

    void begin(const URL&, const WebCore::SecurityOriginData& topOrigin, const WebCore::SystemPreviewInfo&, CompletionHandler<void()>&&);
    void updateProgress(float);
    void loadStarted(const URL& localFileURL);
    void loadCompleted(const URL& localFileURL);
    void loadFailed();
    void end();

    WebPageProxy* page() { return m_webPageProxy.get(); }
    const WebCore::SystemPreviewInfo& previewInfo() const { return m_systemPreviewInfo; }

    void triggerSystemPreviewAction();

    void triggerSystemPreviewActionWithTargetForTesting(uint64_t elementID, NSString* documentID, uint64_t pageID);
    void setCompletionHandlerForLoadTesting(CompletionHandler<void(bool)>&&);

private:
    explicit SystemPreviewController(WebPageProxy&);

    void takeActivityToken();
    void releaseActivityTokenIfNecessary();

    NSArray *localFileURLs() const;

    enum class State : uint8_t {
        Initial,
        Began,
        Loading,
        Viewing
    };

    State m_state { State::Initial };

    WeakPtr<WebPageProxy> m_webPageProxy;
    WebCore::SystemPreviewInfo m_systemPreviewInfo;
    URL m_downloadURL;
    URL m_localFileURL;
    String m_fragmentIdentifier;
#if USE(QUICK_LOOK)
    RetainPtr<QLPreviewController> m_qlPreviewController;
    RetainPtr<_WKPreviewControllerDelegate> m_qlPreviewControllerDelegate;
    RetainPtr<_WKPreviewControllerDataSource> m_qlPreviewControllerDataSource;
    RetainPtr<_WKSystemPreviewDataTaskDelegate> m_wkSystemPreviewDataTaskDelegate;
#endif

    RefPtr<ProcessThrottler::BackgroundActivity> m_activity;
    CompletionHandler<void(bool)> m_testingCallback;
    BlockPtr<void(bool)> m_allowPreviewCallback;
    double m_showPreviewDelay { 0 };

};

}

#endif
