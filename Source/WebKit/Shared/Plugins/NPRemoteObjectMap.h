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

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "Connection.h"
#include <WebCore/npruntime.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class NPObjectMessageReceiver;
class NPObjectProxy;
class NPVariantData;
class Plugin;

class NPRemoteObjectMap : public RefCounted<NPRemoteObjectMap> {
public:
    static Ref<NPRemoteObjectMap> create(IPC::Connection*);
    ~NPRemoteObjectMap();

    // Creates an NPObjectProxy wrapper for the remote object with the given remote object ID.
    NPObject* createNPObjectProxy(uint64_t remoteObjectID, Plugin*);
    void npObjectProxyDestroyed(NPObject*);

    // Expose the given NPObject as a remote object. Returns the objectID.
    uint64_t registerNPObject(NPObject*, Plugin*);
    void unregisterNPObject(uint64_t);

    NPVariantData npVariantToNPVariantData(const NPVariant&, Plugin*);
    NPVariant npVariantDataToNPVariant(const NPVariantData&, Plugin*);

    IPC::Connection* connection() const { return m_connection; }

    void pluginDestroyed(Plugin*);

    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

private:
    explicit NPRemoteObjectMap(IPC::Connection*);
    IPC::Connection* m_connection;

    // A map of NPObjectMessageReceiver classes, wrapping objects that we export to the
    // other end of the connection.
    HashMap<uint64_t, NPObjectMessageReceiver*> m_registeredNPObjects;

    // A set of NPObjectProxy objects associated with this map.
    HashSet<NPObjectProxy*> m_npObjectProxies;
};

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)

#endif // NPRemoteObjectMap_h
