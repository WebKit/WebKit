/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DOMWrapperWorld_h
#define DOMWrapperWorld_h

#include "SecurityOrigin.h"
#include "V8DOMActivityLogger.h"
#include "V8PerContextData.h"
#include <v8.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DOMDataStore;

// This class represent a collection of DOM wrappers for a specific world.
class DOMWrapperWorld : public RefCounted<DOMWrapperWorld> {
public:
    static const int mainWorldId = 0;
    static const int mainWorldExtensionGroup = 0;
    static const int uninitializedWorldId = -1;
    static const int uninitializedExtensionGroup = -1;
    // If uninitializedWorldId is passed as worldId, the world will be assigned a temporary id instead.
    static PassRefPtr<DOMWrapperWorld> ensureIsolatedWorld(int worldId, int extensionGroup);
    ~DOMWrapperWorld();

    static bool isolatedWorldsExist() { return isolatedWorldCount; }
    static bool isIsolatedWorldId(int worldId) { return worldId != mainWorldId && worldId != uninitializedWorldId; }
    static void getAllWorlds(Vector<RefPtr<DOMWrapperWorld> >& worlds);

    void makeContextWeak(v8::Handle<v8::Context>);
    void setIsolatedWorldField(v8::Handle<v8::Context>);

    static DOMWrapperWorld* isolatedWorld(v8::Handle<v8::Context> context)
    {
        ASSERT(contextHasCorrectPrototype(context));
        return static_cast<DOMWrapperWorld*>(context->GetAlignedPointerFromEmbedderData(v8ContextIsolatedWorld));
    }

    // Associates an isolated world (see above for description) with a security
    // origin. XMLHttpRequest instances used in that world will be considered
    // to come from that origin, not the frame's.
    static void setIsolatedWorldSecurityOrigin(int worldID, PassRefPtr<SecurityOrigin>);
    static void clearIsolatedWorldSecurityOrigin(int worldID);
    SecurityOrigin* isolatedWorldSecurityOrigin();

    // Associated an isolated world with a Content Security Policy. Resources
    // embedded into the main world's DOM from script executed in an isolated
    // world should be restricted based on the isolated world's DOM, not the
    // main world's.
    //
    // FIXME: Right now, resource injection simply bypasses the main world's
    // DOM. More work is necessary to allow the isolated world's policy to be
    // applied correctly.
    static void setIsolatedWorldContentSecurityPolicy(int worldID, const String& policy);
    static void clearIsolatedWorldContentSecurityPolicy(int worldID);
    bool isolatedWorldHasContentSecurityPolicy();

    // Associate a logger with the world identified by worldId (worlId may be 0
    // identifying the main world).  
    static void setActivityLogger(int worldId, PassOwnPtr<V8DOMActivityLogger>);
    static V8DOMActivityLogger* activityLogger(int worldId);

    // FIXME: this is a workaround for a problem in WebViewImpl.
    // Do not use this anywhere else!!
    static PassRefPtr<DOMWrapperWorld> createUninitializedWorld();

    bool isMainWorld() const { return m_worldId == mainWorldId; }
    bool isIsolatedWorld() const { return isIsolatedWorldId(m_worldId); }
    bool createdFromUnitializedWorld() const { return m_worldId < uninitializedWorldId; }

    int worldId() const { return m_worldId; }
    int extensionGroup() const { return m_extensionGroup; }
    DOMDataStore* isolatedWorldDOMDataStore() const
    {
        ASSERT(isIsolatedWorld());
        return m_domDataStore.get();
    }

    static void setInitializingWindow(bool);

private:
    static int isolatedWorldCount;
    static PassRefPtr<DOMWrapperWorld> createMainWorld();
    static bool contextHasCorrectPrototype(v8::Handle<v8::Context>);

    DOMWrapperWorld(int worldId, int extensionGroup);

    const int m_worldId;
    const int m_extensionGroup;
    OwnPtr<DOMDataStore> m_domDataStore;

    friend DOMWrapperWorld* mainThreadNormalWorld();
};

DOMWrapperWorld* mainThreadNormalWorld();

} // namespace WebCore

#endif // DOMWrapperWorld_h
