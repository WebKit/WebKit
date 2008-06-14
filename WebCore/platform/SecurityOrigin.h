/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SecurityOrigin_h
#define SecurityOrigin_h

#include <wtf/RefCounted.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>

#include "PlatformString.h"

namespace WebCore {

    class KURL;
    
    class SecurityOrigin : public ThreadSafeShared<SecurityOrigin> {
    public:
        static PassRefPtr<SecurityOrigin> createFromDatabaseIdentifier(const String&);
        static PassRefPtr<SecurityOrigin> createFromString(const String&);
        static PassRefPtr<SecurityOrigin> create(const KURL&);
        static PassRefPtr<SecurityOrigin> createEmpty();

        PassRefPtr<SecurityOrigin> copy();

        void setDomainFromDOM(const String& newDomain);
        String protocol() const { return m_protocol; }
        String host() const { return m_host; }
        String domain() const { return m_domain; }
        unsigned short port() const { return m_port; }

        bool canAccess(const SecurityOrigin*) const;
        bool isSecureTransitionTo(const KURL&) const;

        bool isEmpty() const;
        String toString() const;

        // Serialize the security origin for storage in the database. This format is
        // deprecated and should be used only for compatibility with old databases;
        // use toString() and createFromString() instead.
        String databaseIdentifier() const;

        // This method checks for equality between SecurityOrigins, not whether
        // one origin can access another.  It is used for hash table keys.
        // For access checks, use canAccess().
        // FIXME: If this method is really only useful for hash table keys, it
        // should be refactored into SecurityOriginHash.
        bool equal(const SecurityOrigin*) const;

        // This method checks for equality, ignoring the value of document.domain
        // (and whether it was set) but considering the host. It is used for postMessage.
        bool isSameSchemeHostPort(const SecurityOrigin*) const;

    private:
        explicit SecurityOrigin(const KURL&);
        explicit SecurityOrigin(const SecurityOrigin*);

        String m_protocol;
        String m_host;
        String m_domain;
        unsigned short m_port;
        bool m_noAccess;
        bool m_domainWasSetInDOM;
    };

} // namespace WebCore

#endif // SecurityOrigin_h
