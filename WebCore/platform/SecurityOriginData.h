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
 
#ifndef SecurityOriginData_h
#define SecurityOriginData_h

#include "PlatformString.h"

namespace WebCore {

class SecurityOriginData {
public:
    SecurityOriginData();
    SecurityOriginData(const String& protocol, const String& host, unsigned short port);
    SecurityOriginData(const String& stringIdentifier);
    
    const String& protocol() const { return m_protocol; }
    const String& host() const { return m_host; }
    unsigned short port() const { return m_port; }

    // Needed to make a deep copy of the object for thread safety when crossing a thread boundary
    SecurityOriginData copy() const;

    String stringIdentifier() const;
private:
    String m_protocol;
    String m_host;
    unsigned short m_port;
};
    
inline bool operator==(const SecurityOriginData& a, const SecurityOriginData& b) { return a.protocol() == b.protocol() && a.host() == b.host() && a.port() == b.port(); }
inline bool operator!=(const SecurityOriginData& a, const SecurityOriginData& b) { return !(a == b); }

} // namespace WebCore

#endif // SecurityOriginData_h
