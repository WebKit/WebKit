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

#ifndef NPRemoteObjectMap_h
#define NPRemoteObjectMap_h

#if ENABLE(PLUGIN_PROCESS)

#include <WebCore/npruntime.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>

namespace CoreIPC {
    class Connection;
}

namespace WebKit {

class NPObjectMessageReceiver;
class NPObjectProxy;

class NPRemoteObjectMap {
    WTF_MAKE_NONCOPYABLE(NPRemoteObjectMap);

public:
    explicit NPRemoteObjectMap(CoreIPC::Connection*);

    // Creates an NPObjectProxy wrapper for the remote object with the given remote object ID.
    NPObjectProxy* getOrCreateNPObjectProxy(uint64_t remoteObjectID);

    // Expose the given NPObject as a remote object. Returns the objectID.
    uint64_t registerNPObject(NPObject*);

private:
    CoreIPC::Connection* m_connection;

    // A map of NPObjectMessageReceiver classes, wrapping objects that we export to the
    // other end of the connection.
    HashMap<uint64_t, NPObjectMessageReceiver*> m_registeredNPObjects;
};

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)

#endif // NPRemoteObjectMap_h
