/*
 * Copyright (C) 2006-2018 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "ResourceErrorBase.h"

#include <wtf/RetainPtr.h>
#if USE(CFURLCONNECTION)
#include <CoreFoundation/CFStream.h>
#endif
#if PLATFORM(WIN)
#include <windows.h>
#include <wincrypt.h> // windows.h must be included before wincrypt.h.
#endif

#if PLATFORM(COCOA)
OBJC_CLASS NSError;
#endif

namespace WebCore {

class ResourceError : public ResourceErrorBase {
public:
    ResourceError(Type type = Type::Null)
        : ResourceErrorBase(type)
        , m_dataIsUpToDate(true)
    {
    }

    ResourceError(const String& domain, int errorCode, const URL& failingURL, const String& localizedDescription, Type type = Type::General, IsSanitized isSanitized = IsSanitized::No)
        : ResourceErrorBase(domain, errorCode, failingURL, localizedDescription, type, isSanitized)
        , m_dataIsUpToDate(true)
    {
#if PLATFORM(COCOA)
        ASSERT(domain != getNSURLErrorDomain());
        ASSERT(domain != getCFErrorDomainCFNetwork());
#endif
    }

    WEBCORE_EXPORT ResourceError(CFErrorRef error);

    WEBCORE_EXPORT CFErrorRef cfError() const;
    WEBCORE_EXPORT operator CFErrorRef() const;

#if USE(CFURLCONNECTION)
    ResourceError(const String& domain, int errorCode, const URL& failingURL, const String& localizedDescription, CFDataRef certificate, IsSanitized = IsSanitized::No);
    PCCERT_CONTEXT certificate() const;
    void setCertificate(CFDataRef);
    ResourceError(CFStreamError error);
    CFStreamError cfStreamError() const;
    operator CFStreamError() const;
    static const void* getSSLPeerCertificateDataBytePtr(CFDictionaryRef);
#endif

#if PLATFORM(COCOA)
    WEBCORE_EXPORT ResourceError(NSError *);
    WEBCORE_EXPORT NSError *nsError() const;
    WEBCORE_EXPORT operator NSError *() const;
#endif

    bool compromisedNetworkConnectionIntegrity() const { return m_compromisedNetworkConnectionIntegrity; }

    static bool platformCompare(const ResourceError& a, const ResourceError& b);

private:
    friend class ResourceErrorBase;

#if PLATFORM(COCOA)
    WEBCORE_EXPORT const String& getNSURLErrorDomain() const;
    WEBCORE_EXPORT const String& getCFErrorDomainCFNetwork() const;
    WEBCORE_EXPORT void mapPlatformError();
#endif
    void platformLazyInit();

    void doPlatformIsolatedCopy(const ResourceError&);

    bool m_dataIsUpToDate;
#if USE(CFURLCONNECTION)
    mutable RetainPtr<CFErrorRef> m_platformError;
#if PLATFORM(WIN)
    RetainPtr<CFDataRef> m_certificate;
#endif
#else
    mutable RetainPtr<NSError> m_platformError;
#endif
    bool m_compromisedNetworkConnectionIntegrity { false };
};

} // namespace WebCore
