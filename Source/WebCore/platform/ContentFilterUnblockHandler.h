/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#if ENABLE(CONTENT_FILTERING)

#include <functional>
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS NSCoder;
OBJC_CLASS NSNumber;

#if HAVE(WEBCONTENTRESTRICTIONS)
OBJC_CLASS WCRBrowserEngineClient;
#endif

#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
OBJC_CLASS WebFilterEvaluator;
#endif

namespace WebCore {

class ResourceRequest;

class ContentFilterUnblockHandler {
public:
    using DecisionHandlerFunction = std::function<void(bool unblocked)>;
    using UnblockRequesterFunction = std::function<void(DecisionHandlerFunction)>;

    ContentFilterUnblockHandler() = default;
    WEBCORE_EXPORT ContentFilterUnblockHandler(String unblockURLHost, UnblockRequesterFunction&&);
#if HAVE(WEBCONTENTRESTRICTIONS)
    ContentFilterUnblockHandler(const URL& evaluatedURL);
#endif
#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    ContentFilterUnblockHandler(String unblockURLHost, RetainPtr<WebFilterEvaluator>);
#endif

    WEBCORE_EXPORT ContentFilterUnblockHandler(
        String&& unblockURLHost,
        URL&& unreachableURL,
#if HAVE(WEBCONTENTRESTRICTIONS)
        std::optional<URL>&& evaluatedURL,
#endif
#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
        Vector<uint8_t>&& webFilterEvaluatorData,
#endif
        bool unblockedAfterRequest
    );

    WEBCORE_EXPORT bool needsUIProcess() const;
    WEBCORE_EXPORT bool canHandleRequest(const ResourceRequest&) const;
    WEBCORE_EXPORT void requestUnblockAsync(DecisionHandlerFunction&&);
    void wrapWithDecisionHandler(const DecisionHandlerFunction&);
#if HAVE(WEBCONTENTRESTRICTIONS)
    WEBCORE_EXPORT bool needsNetworkProcess() const;
#endif

    const String& unblockURLHost() const { return m_unblockURLHost; }
    const URL& unreachableURL() const { return m_unreachableURL; }
    void setUnreachableURL(const URL& url) { m_unreachableURL = url; }

#if HAVE(WEBCONTENTRESTRICTIONS)
    std::optional<URL> evaluatedURL() const { return m_evaluatedURL; }
#endif
#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    const String& configurationPath() const { return m_configurationPath; }
    void setConfigurationPath(const String& path) { m_configurationPath = path; }
#endif
#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    WEBCORE_EXPORT Vector<uint8_t> webFilterEvaluatorData() const;
#endif

    WEBCORE_EXPORT void setUnblockedAfterRequest(bool);
    bool unblockedAfterRequest() const { return m_unblockedAfterRequest; }

private:
#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    bool hasWebFilterEvaluator() const;
    RetainPtr<WebFilterEvaluator> webFilterEvaluator();
#endif

    String m_unblockURLHost;
    URL m_unreachableURL;
    UnblockRequesterFunction m_unblockRequester;
#if HAVE(WEBCONTENTRESTRICTIONS)
    std::optional<URL> m_evaluatedURL;
#endif
#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    String m_configurationPath;
#endif
#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    Vector<uint8_t> m_webFilterEvaluatorData;
    RetainPtr<WebFilterEvaluator> m_webFilterEvaluator;
#endif
    bool m_unblockedAfterRequest { false };
};

} // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)
