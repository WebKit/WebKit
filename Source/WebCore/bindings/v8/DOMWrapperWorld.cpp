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

#include "config.h"
#include "DOMWrapperWorld.h"

#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

int DOMWrapperWorld::isolatedWorldCount = 0;

PassRefPtr<DOMWrapperWorld>  DOMWrapperWorld::createUninitializedWorld()
{
    return adoptRef(new DOMWrapperWorld(uninitializedWorldId, uninitializedExtensionGroup));
}

PassRefPtr<DOMWrapperWorld> DOMWrapperWorld::createMainWorld()
{
    return adoptRef(new DOMWrapperWorld(mainWorldId, mainWorldExtensionGroup));
}

DOMWrapperWorld* mainThreadNormalWorld()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(RefPtr<DOMWrapperWorld>, cachedNormalWorld, (DOMWrapperWorld::createMainWorld()));
    return cachedNormalWorld.get();
}

// FIXME: This should probably go to PerIsolateData.
typedef HashMap<int, DOMWrapperWorld*> WorldMap;
static WorldMap& isolatedWorldMap()
{
    DEFINE_STATIC_LOCAL(WorldMap, map, ());
    return map;
}

void DOMWrapperWorld::deallocate(DOMWrapperWorld* world)
{
    int worldId = world->worldId();

    // Ensure we never deallocate mainThreadNormalWorld
    if (worldId == mainWorldId)
        return;

    delete world;

    if (worldId == uninitializedWorldId)
        return;

    WorldMap& map = isolatedWorldMap();
    WorldMap::iterator i = map.find(worldId);
    if (i == map.end()) {
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT(i->value == world);

    map.remove(i);
    isolatedWorldCount--;
}

static int temporaryWorldId = DOMWrapperWorld::uninitializedWorldId-1;

PassRefPtr<DOMWrapperWorld> DOMWrapperWorld::ensureIsolatedWorld(int worldId, int extensionGroup)
{
    ASSERT(worldId != mainWorldId);

    WorldMap& map = isolatedWorldMap();
    if (worldId == uninitializedWorldId)
        worldId = temporaryWorldId--;
    else {
        WorldMap::iterator i = map.find(worldId);
        if (i != map.end()) {
            ASSERT(i->value->worldId() == worldId);
            ASSERT(i->value->extensionGroup() == extensionGroup);
            return i->value;
        }
    }

    RefPtr<DOMWrapperWorld> world = adoptRef(new DOMWrapperWorld(worldId, extensionGroup));
    map.add(worldId, world.get());
    isolatedWorldCount++;
    return world.release();
}

typedef HashMap<int, RefPtr<SecurityOrigin> > IsolatedWorldSecurityOriginMap;
static IsolatedWorldSecurityOriginMap& isolatedWorldSecurityOrigins()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(IsolatedWorldSecurityOriginMap, map, ());
    return map;
}

SecurityOrigin* DOMWrapperWorld::isolatedWorldSecurityOrigin()
{
    ASSERT(this->isIsolatedWorld());
    IsolatedWorldSecurityOriginMap& origins = isolatedWorldSecurityOrigins();
    IsolatedWorldSecurityOriginMap::iterator it = origins.find(worldId());
    return it == origins.end() ? 0 : it->value.get();
}

void DOMWrapperWorld::setIsolatedWorldSecurityOrigin(int worldID, PassRefPtr<SecurityOrigin> securityOrigin)
{
    ASSERT(DOMWrapperWorld::isIsolatedWorldId(worldID));
    if (securityOrigin)
        isolatedWorldSecurityOrigins().set(worldID, securityOrigin);
    else
        isolatedWorldSecurityOrigins().remove(worldID);
}

void DOMWrapperWorld::clearIsolatedWorldSecurityOrigin(int worldID)
{
    ASSERT(DOMWrapperWorld::isIsolatedWorldId(worldID));
    isolatedWorldSecurityOrigins().remove(worldID);
}

} // namespace WebCore
