/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#import <CoreFoundation/CoreFoundation.h>
#import <wtf/ArgumentCoder.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

OBJC_CLASS NSError;

namespace WebKit {

class CoreIPCError {
    WTF_MAKE_TZONE_ALLOCATED(CoreIPCError);
public:
    struct NetworkResolutionReportInterface {
        RefPtr<StringImpl> type;
        RefPtr<StringImpl> name;
    };

    struct NetworkResolutionReport {
        RefPtr<StringImpl> provider;
        RefPtr<StringImpl> dnsFailureReason;
        RefPtr<StringImpl> extendedDNSErrorExtraText;
        Vector<NetworkResolutionReportInterface> interfaces;
    };

    CoreIPCError(CoreIPCError&&) = default;
    CoreIPCError& operator=(CoreIPCError&&) = default;

    CoreIPCError(NSError *);
    CoreIPCError(RefPtr<StringImpl>&& domain, int64_t code, std::unique_ptr<CoreIPCError>&& underlyingError, std::optional<Vector<RetainPtr<SecCertificateRef>>>&& clientCertificateChain, std::optional<Vector<RetainPtr<SecCertificateRef>>>&& peerCertificateChain, RefPtr<StringImpl>&& localizedDescription, RefPtr<StringImpl>&& localizedFailureReasonError, RefPtr<StringImpl>&& localizedRecoverySuggestionError, std::optional<Vector<RefPtr<StringImpl>>>&& localizedRecoveryOptionsError, RefPtr<StringImpl>&& localizedFailureError, RefPtr<StringImpl>&& helpAnchorError, RefPtr<StringImpl>&& debugDescriptionError, RetainPtr<NSNumber>&& stringEncodingError, RetainPtr<SecTrustRef>&& failingURLPeerTrustError, RetainPtr<NSURL>&& urlError, RetainPtr<NSURL>&& failingURLError,
        RefPtr<StringImpl>&& filePathError, RefPtr<StringImpl>&& networkTaskDescription, RefPtr<StringImpl>&& networkTaskMetricsPrivacyStance, std::optional<NetworkResolutionReport>&& networkResolutionReport, RefPtr<StringImpl>&& description)
        : m_domain(WTF::move(domain))
        , m_code(WTF::move(code))
        , m_underlyingError(WTF::move(underlyingError))
        , m_clientCertificateChain(WTF::move(clientCertificateChain))
        , m_peerCertificateChain(WTF::move(peerCertificateChain))
        , m_localizedDescription(WTF::move(localizedDescription))
        , m_localizedFailureReasonError(WTF::move(localizedFailureReasonError))
        , m_localizedRecoverySuggestionError(WTF::move(localizedRecoverySuggestionError))
        , m_localizedRecoveryOptionsError(WTF::move(localizedRecoveryOptionsError))
        , m_localizedFailureError(WTF::move(localizedFailureError))
        , m_helpAnchorError(WTF::move(helpAnchorError))
        , m_debugDescriptionError(WTF::move(debugDescriptionError))
        , m_stringEncodingError(WTF::move(stringEncodingError))
        , m_failingURLPeerTrustError(WTF::move(failingURLPeerTrustError))
        , m_urlError(WTF::move(urlError))
        , m_failingURLError(WTF::move(failingURLError))
        , m_filePathError(WTF::move(filePathError))
        , m_networkTaskDescription(WTF::move(networkTaskDescription))
        , m_networkTaskMetricsPrivacyStance(WTF::move(networkTaskMetricsPrivacyStance))
        , m_networkResolutionReport(WTF::move(networkResolutionReport))
        , m_description(WTF::move(description))
    {
    }

    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCError>;

    RefPtr<StringImpl> m_domain;
    int64_t m_code;
    std::unique_ptr<CoreIPCError> m_underlyingError;

    std::optional<Vector<RetainPtr<SecCertificateRef>>> m_clientCertificateChain;
    std::optional<Vector<RetainPtr<SecCertificateRef>>> m_peerCertificateChain;
    RefPtr<StringImpl> m_localizedDescription;
    RefPtr<StringImpl> m_localizedFailureReasonError;
    RefPtr<StringImpl> m_localizedRecoverySuggestionError;
    std::optional<Vector<RefPtr<StringImpl>>> m_localizedRecoveryOptionsError;
    RefPtr<StringImpl> m_localizedFailureError;

    RefPtr<StringImpl> m_helpAnchorError;
    RefPtr<StringImpl> m_debugDescriptionError;

    RetainPtr<NSNumber> m_stringEncodingError;

    RetainPtr<SecTrustRef> m_failingURLPeerTrustError;
    RetainPtr<NSURL> m_urlError;
    RetainPtr<NSURL> m_failingURLError;

    RefPtr<StringImpl> m_filePathError;

    RefPtr<StringImpl> m_networkTaskDescription;
    RefPtr<StringImpl> m_networkTaskMetricsPrivacyStance;
    std::optional<NetworkResolutionReport> m_networkResolutionReport;

    RefPtr<StringImpl> m_description;
};

}

#endif // PLATFORM(COCOA)
