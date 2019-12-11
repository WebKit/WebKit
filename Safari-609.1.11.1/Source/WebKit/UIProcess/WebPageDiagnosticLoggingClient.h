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

#ifndef WebPageDiagnosticLoggingClient_h
#define WebPageDiagnosticLoggingClient_h

#include "APIClient.h"
#include "APIDiagnosticLoggingClient.h"
#include "WKPage.h"
#include <WebCore/DiagnosticLoggingResultType.h>
#include <wtf/Forward.h>

namespace API {

template<> struct ClientTraits<WKPageDiagnosticLoggingClientBase> {
    typedef std::tuple<WKPageDiagnosticLoggingClientV0, WKPageDiagnosticLoggingClientV1> Versions;
};

} // namespace API

namespace WebKit {

class WebPageProxy;

class WebPageDiagnosticLoggingClient final : public API::Client<WKPageDiagnosticLoggingClientBase>, public API::DiagnosticLoggingClient {
public:
    explicit WebPageDiagnosticLoggingClient(const WKPageDiagnosticLoggingClientBase*);

    void logDiagnosticMessage(WebPageProxy*, const String& message, const String& description) override;
    void logDiagnosticMessageWithResult(WebPageProxy*, const String& message, const String& description, WebCore::DiagnosticLoggingResultType) override;
    void logDiagnosticMessageWithValue(WebPageProxy*, const String& message, const String& description, const String& value) override;
    void logDiagnosticMessageWithEnhancedPrivacy(WebPageProxy*, const String& message, const String& description) override;
    void logDiagnosticMessageWithValueDictionary(WebKit::WebPageProxy*, const WTF::String& message, const WTF::String& description, Ref<API::Dictionary>&&) override { }
};

} // namespace WebKit

#endif // WebPageDiagnosticLoggingClient_h
