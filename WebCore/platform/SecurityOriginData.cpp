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

static const char SeparatorCharacter = '_';

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
    // Make sure there's a first separator
    int separator1 = stringIdentifier.find(SeparatorCharacter);
    if (separator1 == -1)
        return;
            
    // Make sure there's a second separator
    int separator2 = stringIdentifier.find(SeparatorCharacter, separator1 + 1);
    if (separator2 == -1)
        return;
        
    // Make sure there's not a third separator
    if (stringIdentifier.reverseFind(SeparatorCharacter) != separator2)
        return;
        
    // Make sure the port section is a valid port number or doesn't exist
    bool portOkay;
    int port = stringIdentifier.right(stringIdentifier.length() - separator2 - 1).toInt(&portOkay);
    if (!portOkay && separator2 + 1 == static_cast<int>(stringIdentifier.length()))
        return;

    if (port < 0 || port > 65535)
        return;
            
    // Split out the 3 sections of data
    m_protocol = stringIdentifier.substring(0, separator1);
    m_host = stringIdentifier.substring(separator1 + 1, separator2 - separator1 - 1);
    m_port = port;
}

SecurityOriginData SecurityOriginData::copy() const
{
    return SecurityOriginData(m_protocol.copy(), m_host.copy(), m_port);
}

String SecurityOriginData::stringIdentifier() const 
{
    static String separatorString = String(&SeparatorCharacter, 1);
    return m_protocol + separatorString + m_host + separatorString + String::number(m_port); 
}

} // namespace WebCore
