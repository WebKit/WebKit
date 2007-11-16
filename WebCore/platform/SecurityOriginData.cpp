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
#include "config.h"
#include "SecurityOriginData.h"

namespace WebCore {

SecurityOriginData::SecurityOriginData()
    : m_port(0)
{
}

SecurityOriginData::SecurityOriginData(const String& protocol, const String& host, unsigned short port)
    : m_protocol(protocol)
    , m_host(host)
    , m_port(port)
{
}    

SecurityOriginData::SecurityOriginData(const String& stringIdentifier)
    : m_port(0)
{ 
    // Make sure there's a first colon
    int colon1 = stringIdentifier.find(':');
    if (colon1 == -1)
        return;
            
    // Make sure there's a second colon
    int colon2 = stringIdentifier.find(':', colon1 + 1);
    if (colon2 == -1)
        return;
        
    // Make sure there's not a third colon
    if (stringIdentifier.reverseFind(':') != colon2)
        return;
        
    // Make sure the port section is a valid port number or doesn't exist
    bool portOkay;
    int port = stringIdentifier.right(stringIdentifier.length() - colon2 - 1).toInt(&portOkay);
    if (!portOkay && colon2 + 1 == static_cast<int>(stringIdentifier.length()))
        return;

    if (port < 0 || port > 65535)
        return;
            
    // Split out the 3 sections of data
    m_protocol = stringIdentifier.substring(0, colon1);
    m_host = stringIdentifier.substring(colon1 + 1, colon2 - colon1 - 1);
    m_port = port;
}

String SecurityOriginData::stringIdentifier() const 
{
    return m_protocol + ":" + m_host + ":" + String::number(m_port); 
}


} // namespace WebCore
