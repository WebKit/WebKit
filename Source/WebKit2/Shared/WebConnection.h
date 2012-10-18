/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef WebConnection_h
#define WebConnection_h

#include "APIObject.h"
#include "WebConnectionClient.h"
#include <wtf/RefPtr.h>

namespace CoreIPC {
    class ArgumentDecoder;
    class ArgumentEncoder;
    class Connection;
    class DataReference;
    class MessageDecoder;
    class MessageEncoder;
    class MessageID;
}

namespace WebKit {

class WebConnection : public APIObject {
public:
    static const Type APIType = TypeConnection;
    virtual ~WebConnection();

    CoreIPC::Connection* connection() { return m_connection.get(); }

    void initializeConnectionClient(const WKConnectionClient*);
    void postMessage(const String&, APIObject*);

    void invalidate();

protected:
    explicit WebConnection(PassRefPtr<CoreIPC::Connection>);

    virtual Type type() const { return APIType; }
    virtual void encodeMessageBody(CoreIPC::ArgumentEncoder&, APIObject*) = 0;
    virtual bool decodeMessageBody(CoreIPC::ArgumentDecoder&, RefPtr<APIObject>&) = 0;

    // Implemented in generated WebConnectionMessageReceiver.cpp
    void didReceiveWebConnectionMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);
    void handleMessage(const CoreIPC::DataReference& messageData);

    RefPtr<CoreIPC::Connection> m_connection;
    WebConnectionClient m_client;
};

} // namespace WebKit

#endif // WebConnection_h
