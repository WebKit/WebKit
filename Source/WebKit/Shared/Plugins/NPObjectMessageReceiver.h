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

#pragma once

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "Connection.h"
#include <WebCore/npruntime.h>
#include <wtf/Noncopyable.h>

namespace WebKit {

class NPIdentifierData;
class NPRemoteObjectMap;
class NPVariantData;
class Plugin;

class NPObjectMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(NPObjectMessageReceiver);

public:
    NPObjectMessageReceiver(NPRemoteObjectMap*, Plugin*, uint64_t npObjectID, NPObject*);
    ~NPObjectMessageReceiver();

    bool didReceiveSyncNPObjectMessageReceiverMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    Plugin* plugin() const { return m_plugin; }
    NPObject* npObject() const { return m_npObject; }
    
private:
    // Message handlers.
    void deallocate(CompletionHandler<void()>&&);
    void hasMethod(const NPIdentifierData&, CompletionHandler<void(bool)>&&);
    void invoke(const NPIdentifierData&, const Vector<NPVariantData>& argumentsData, CompletionHandler<void(bool, NPVariantData&&)>&&);
    void invokeDefault(const Vector<NPVariantData>& argumentsData, CompletionHandler<void(bool, NPVariantData&&)>&&);
    void hasProperty(const NPIdentifierData&, CompletionHandler<void(bool)>&&);
    void getProperty(const NPIdentifierData&, CompletionHandler<void(bool, NPVariantData&&)>&&);
    void setProperty(const NPIdentifierData&, const NPVariantData& propertyValueData, CompletionHandler<void(bool)>&&);
    void removeProperty(const NPIdentifierData&, CompletionHandler<void(bool)>&&);
    void enumerate(CompletionHandler<void(bool, Vector<NPIdentifierData>&&)>&&);
    void construct(const Vector<NPVariantData>& argumentsData, CompletionHandler<void(bool, NPVariantData&&)>&&);

    NPRemoteObjectMap* m_npRemoteObjectMap;
    Plugin* m_plugin;
    uint64_t m_npObjectID;
    NPObject* m_npObject;
};
    
} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
