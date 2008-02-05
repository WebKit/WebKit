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

#include "PlatformString.h"
#include "Threading.h"

namespace WebCore {

    class Frame;
    class KURL;
    
    class SecurityOrigin : public ThreadSafeShared<SecurityOrigin> {
    public:
        static PassRefPtr<SecurityOrigin> createForFrame(Frame*);
        static PassRefPtr<SecurityOrigin> createFromIdentifier(const String&);
        static PassRefPtr<SecurityOrigin> create(const String& protocol, const String& host, unsigned short port, SecurityOrigin* ownerFrameOrigin);

        PassRefPtr<SecurityOrigin> copy();

        void setDomainFromDOM(const String& newDomain);
        String host() const { return m_host; }
        String protocol() const { return m_protocol; }
        unsigned short port() const { return m_port; }
        
        enum Reason  {
            GenericMismatch,
            DomainSetInDOMMismatch
        };
        bool canAccess(const SecurityOrigin*, Reason&) const;
        bool isSecureTransitionTo(const KURL&) const;

        bool isEmpty() const;
        String toString() const;
        
        String stringIdentifier() const;

        // do not use this for access checks, it's there only for using this as a hashtable key
        bool equal(SecurityOrigin* other) const { return m_protocol == other->m_protocol && m_host == other->m_host && m_port == other->m_port; }
        
    private:
        SecurityOrigin(const String& protocol, const String& host, unsigned short port);

        String m_protocol;
        String m_host;
        unsigned short m_port;
        bool m_portSet;
        bool m_noAccess;
        bool m_domainWasSetInDOM;
    };

} // namespace WebCore

#endif // SecurityOrigin_h
