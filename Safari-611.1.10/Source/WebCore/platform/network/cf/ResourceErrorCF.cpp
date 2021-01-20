/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "ResourceError.h"

#if USE(CFURLCONNECTION)

#include <CoreFoundation/CFError.h>
#include <CFNetwork/CFNetworkErrors.h>
#include <pal/spi/win/CFNetworkSPIWin.h>
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>

namespace WebCore {

ResourceError::ResourceError(CFErrorRef cfError)
    : ResourceErrorBase(Type::Null)
    , m_dataIsUpToDate(false)
    , m_platformError(cfError)
{
    if (cfError)
        setType((CFErrorGetCode(m_platformError.get()) == kCFURLErrorTimedOut) ? Type::Timeout : Type::General);
}

ResourceError::ResourceError(const String& domain, int errorCode, const URL& failingURL, const String& localizedDescription, CFDataRef certificate)
    : ResourceErrorBase(domain, errorCode, failingURL, localizedDescription, Type::General)
    , m_dataIsUpToDate(true)
    , m_certificate(certificate)
{
}

PCCERT_CONTEXT ResourceError::certificate() const
{
    if (!m_certificate)
        return 0;
    
    return reinterpret_cast<PCCERT_CONTEXT>(CFDataGetBytePtr(m_certificate.get()));
}

void ResourceError::setCertificate(CFDataRef certificate)
{
    m_certificate = certificate;
}

const CFStringRef failingURLStringKey = CFSTR("NSErrorFailingURLStringKey");
const CFStringRef failingURLKey = CFSTR("NSErrorFailingURLKey");

static CFDataRef getSSLPeerCertificateData(CFDictionaryRef dict)
{
    if (!dict)
        return nullptr;
    return reinterpret_cast<CFDataRef>(CFDictionaryGetValue(dict, _kCFWindowsSSLPeerCert));
}

static void setSSLPeerCertificateData(CFMutableDictionaryRef dict, CFDataRef data)
{
    if (!dict)
        return;
    
    if (!data)
        CFDictionaryRemoveValue(dict, _kCFWindowsSSLPeerCert);
    else
        CFDictionarySetValue(dict, _kCFWindowsSSLPeerCert, data);
}

const void* ResourceError::getSSLPeerCertificateDataBytePtr(CFDictionaryRef dict)
{
    CFDataRef data = getSSLPeerCertificateData(dict);
    return data ? reinterpret_cast<const void*>(CFDataGetBytePtr(data)) : nullptr;
}

void ResourceError::platformLazyInit()
{
    if (m_dataIsUpToDate)
        return;

    if (!m_platformError)
        return;

    CFStringRef domain = CFErrorGetDomain(m_platformError.get());
    if (domain == kCFErrorDomainMach || domain == kCFErrorDomainCocoa)
        m_domain ="NSCustomErrorDomain";
    else if (domain == kCFErrorDomainCFNetwork)
        m_domain = "CFURLErrorDomain";
    else if (domain == kCFErrorDomainPOSIX)
        m_domain = "NSPOSIXErrorDomain";
    else if (domain == kCFErrorDomainOSStatus)
        m_domain = "NSOSStatusErrorDomain";
    else if (domain == kCFErrorDomainWinSock)
        m_domain = "kCFErrorDomainWinSock";
    else
        m_domain = domain;

    m_errorCode = CFErrorGetCode(m_platformError.get());

    RetainPtr<CFDictionaryRef> userInfo = adoptCF(CFErrorCopyUserInfo(m_platformError.get()));
    if (userInfo.get()) {
        CFStringRef failingURLString = (CFStringRef) CFDictionaryGetValue(userInfo.get(), failingURLStringKey);
        if (failingURLString)
            m_failingURL = URL(URL(), failingURLString);
        else {
            CFURLRef failingURL = (CFURLRef) CFDictionaryGetValue(userInfo.get(), failingURLKey);
            if (failingURL) {
                if (RetainPtr<CFURLRef> absoluteURLRef = adoptCF(CFURLCopyAbsoluteURL(failingURL)))
                    m_failingURL = URL(absoluteURLRef.get());
            }
        }
        m_localizedDescription = (CFStringRef) CFDictionaryGetValue(userInfo.get(), kCFErrorLocalizedDescriptionKey);
        
        m_certificate = getSSLPeerCertificateData(userInfo.get());
    }

    m_dataIsUpToDate = true;
}


void ResourceError::doPlatformIsolatedCopy(const ResourceError& other)
{
    m_certificate = other.m_certificate;
}

bool ResourceError::platformCompare(const ResourceError& a, const ResourceError& b)
{
    return a.cfError() == b.cfError();
}

CFErrorRef ResourceError::cfError() const
{
    if (isNull()) {
        ASSERT(!m_platformError);
        return 0;
    }

    if (!m_platformError) {
        RetainPtr<CFMutableDictionaryRef> userInfo = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

        if (!m_localizedDescription.isEmpty())
            CFDictionarySetValue(userInfo.get(), kCFErrorLocalizedDescriptionKey, m_localizedDescription.createCFString().get());

        if (!m_failingURL.isEmpty()) {
            RetainPtr<CFStringRef> failingURLString = m_failingURL.string().createCFString();
            CFDictionarySetValue(userInfo.get(), failingURLStringKey, failingURLString.get());
            if (RetainPtr<CFURLRef> url = m_failingURL.createCFURL())
                CFDictionarySetValue(userInfo.get(), failingURLKey, url.get());
        }

        if (m_certificate)
            setSSLPeerCertificateData(userInfo.get(), m_certificate.get());
        
        m_platformError = adoptCF(CFErrorCreate(0, m_domain.createCFString().get(), m_errorCode, userInfo.get()));
    }

    return m_platformError.get();
}

ResourceError::operator CFErrorRef() const
{
    return cfError();
}

// FIXME: Once <rdar://problem/5050841> is fixed we can remove this constructor.
ResourceError::ResourceError(CFStreamError error)
    : ResourceErrorBase(Type::General)
    , m_dataIsUpToDate(true)
{
    m_errorCode = error.error;

    switch(error.domain) {
    case kCFStreamErrorDomainCustom:
        m_domain ="NSCustomErrorDomain";
        break;
    case kCFStreamErrorDomainPOSIX:
        m_domain = "NSPOSIXErrorDomain";
        break;
    case kCFStreamErrorDomainMacOSStatus:
        m_domain = "NSOSStatusErrorDomain";
        break;
    }
}

CFStreamError ResourceError::cfStreamError() const
{
    lazyInit();

    CFStreamError result;
    result.error = m_errorCode;

    if (m_domain == "NSCustomErrorDomain")
        result.domain = kCFStreamErrorDomainCustom;
    else if (m_domain == "NSPOSIXErrorDomain")
        result.domain = kCFStreamErrorDomainPOSIX;
    else if (m_domain == "NSOSStatusErrorDomain")
        result.domain = kCFStreamErrorDomainMacOSStatus;
    else {
        result.domain = kCFStreamErrorDomainCustom;
        ASSERT_NOT_REACHED();
    }

    return result;
}

ResourceError::operator CFStreamError() const
{
    return cfStreamError();
}

} // namespace WebCore

#endif // USE(CFURLCONNECTION)
