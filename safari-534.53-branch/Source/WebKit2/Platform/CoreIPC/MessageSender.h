/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef MessageSender_h
#define MessageSender_h

#include <wtf/Assertions.h>
#include "Connection.h"

namespace CoreIPC {
    
template<typename T> class MessageSender {
public:
    template<typename U> bool send(const U& message)
    {
        return send(message, static_cast<T*>(this)->destinationID());
    }

    template<typename U> bool send(const U& message, uint64_t destinationID)
    {
        OwnPtr<ArgumentEncoder> argumentEncoder = ArgumentEncoder::create(destinationID);
        argumentEncoder->encode(message);
        
        return static_cast<T*>(this)->sendMessage(MessageID(U::messageID), argumentEncoder.release());
    }
    
    bool sendMessage(MessageID messageID, PassOwnPtr<ArgumentEncoder> argumentEncoder)
    {
        Connection* connection = static_cast<T*>(this)->connection();
        ASSERT(connection);

        return connection->sendMessage(messageID, argumentEncoder);
    }

    template<typename U> bool sendSync(const U& message, const typename U::Reply& reply, double timeout = Connection::NoTimeout)
    {
        return sendSync(message, reply, static_cast<T*>(this)->destinationID(), timeout);
    }
    
    template<typename U> bool sendSync(const U& message, const typename U::Reply& reply, uint64_t destinationID, double timeout = Connection::NoTimeout)
    {
        Connection* connection = static_cast<T*>(this)->connection();
        ASSERT(connection);

        return connection->sendSync(message, reply, destinationID, timeout);
    }
};

} // namespace CoreIPC

#endif // MessageSender_h
