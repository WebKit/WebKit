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

#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/IntRect.h>
#include <WebCore/ResourceError.h>
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>

#if USE(QUICK_LOOK)
OBJC_CLASS QLPreviewController;
OBJC_CLASS _WKPreviewControllerDataSource;
OBJC_CLASS _WKPreviewControllerDelegate;
#endif

namespace WebKit {

class WebPageProxy;

class SystemPreviewController {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit SystemPreviewController(WebPageProxy&);

    bool canPreview(const String& mimeType) const;

    void start(URL originatingPageURL, const String& mimeType, const WebCore::SystemPreviewInfo&);
    void setDestinationURL(URL);
    void updateProgress(float);
    void finish(URL);
    void cancel();
    void fail(const WebCore::ResourceError&);

    WebPageProxy& page() { return m_webPageProxy; }
    const WebCore::SystemPreviewInfo& previewInfo() const { return m_systemPreviewInfo; }

    void triggerSystemPreviewAction();

    void triggerSystemPreviewActionWithTargetForTesting(uint64_t elementID, NSString* documentID, uint64_t pageID);

private:
    WebPageProxy& m_webPageProxy;
    WebCore::SystemPreviewInfo m_systemPreviewInfo;
    URL m_destinationURL;
#if USE(QUICK_LOOK)
    RetainPtr<QLPreviewController> m_qlPreviewController;
    RetainPtr<_WKPreviewControllerDelegate> m_qlPreviewControllerDelegate;
    RetainPtr<_WKPreviewControllerDataSource> m_qlPreviewControllerDataSource;
#endif
};

}

#endif
