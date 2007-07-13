// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ResourceError_h
#define ResourceError_h

#include "PlatformString.h"

#if PLATFORM(CF)
#include <wtf/RetainPtr.h>
#endif

#ifdef __OBJC__
@class NSError;
#else
class NSError;
#endif

#if USE(CFNETWORK)
#include <CoreFoundation/CFStream.h>
#endif

namespace WebCore {

    class ResourceError {
    public:
        ResourceError()
            : m_errorCode(0)
#if PLATFORM(CF)
            , m_dataIsUpToDate(true)
#endif
            , m_isNull(true)
        {
        }

        ResourceError(const String& domain, int errorCode, const String& failingURL, const String& localizedDescription)
            : m_domain(domain)
            , m_errorCode(errorCode)
            , m_failingURL(failingURL)
            , m_localizedDescription(localizedDescription)
#if PLATFORM(CF)
            , m_dataIsUpToDate(true)
#endif
            , m_isNull(false)
        {
        }

#if PLATFORM(CF)
#if PLATFORM(MAC)
        ResourceError(NSError* error)
#else
        ResourceError(CFStreamError error);
        ResourceError(CFErrorRef error)
#endif
            : m_dataIsUpToDate(false)
            , m_platformError(error)
            , m_isNull(!error)
        {
        }
#endif

#if 0
        static const String CocoaErrorDomain;
        static const String POSIXDomain;
        static const String OSStatusDomain;
        static const String MachDomain;
        static const String WebKitDomain;
#endif

        bool isNull() const { return m_isNull; }

        const String& domain() const { unpackPlatformErrorIfNeeded(); return m_domain; }
        int errorCode() const { unpackPlatformErrorIfNeeded(); return m_errorCode; }
        const String& failingURL() const { unpackPlatformErrorIfNeeded(); return m_failingURL; }
        const String& localizedDescription() const { unpackPlatformErrorIfNeeded(); return m_localizedDescription; }

#if PLATFORM(CF)
#if PLATFORM(MAC)
        operator NSError*() const;
#else
        operator CFErrorRef() const;
        operator CFStreamError() const;
#endif
#endif

    private:
        void unpackPlatformErrorIfNeeded() const
        {
#if PLATFORM(CF)
            if (!m_dataIsUpToDate)
                const_cast<ResourceError*>(this)->unpackPlatformError();
#endif
        }

#if PLATFORM(CF)
        void unpackPlatformError();
#endif

        String m_domain;
        int m_errorCode;
        String m_failingURL;
        String m_localizedDescription;
 
#if PLATFORM(CF)
        bool m_dataIsUpToDate;
#endif
#if PLATFORM(MAC)
        mutable RetainPtr<NSError> m_platformError;
#elif PLATFORM(CF)
        mutable RetainPtr<CFErrorRef> m_platformError;
#endif
        bool m_isNull;
};

inline bool operator==(const ResourceError& a, const ResourceError& b)
{
    if (a.isNull() && b.isNull())
        return true;
    if (a.isNull() || b.isNull())
        return false;
    if (a.domain() != b.domain())
        return false;
    if (a.errorCode() != b.errorCode())
        return false;
    if (a.failingURL() != b.failingURL())
        return false;
    if (a.localizedDescription() != b.localizedDescription())
        return false;
#if PLATFORM(CF)
#if PLATFORM(MAC)
    if ((NSError *)a != (NSError *)b)
        return false;
#else
    if ((CFErrorRef)a != (CFErrorRef)b)
        return false;
#endif
#endif
    return true;
}

inline bool operator!=(const ResourceError& a, const ResourceError& b) { return !(a == b); }

} // namespace WebCore

#endif // ResourceError_h_
