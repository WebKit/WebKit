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

#include "config.h"
#include "InjectedBundleScriptWorld.h"

#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/ScriptController.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

typedef HashMap<DOMWrapperWorld*, InjectedBundleScriptWorld*> WorldMap;

static WorldMap& allWorlds()
{
    static NeverDestroyed<WorldMap> map;
    return map;
}

static String uniqueWorldName()
{
    static uint64_t uniqueWorldNameNumber = 0;
    return makeString("UniqueWorld_", uniqueWorldNameNumber++);
}

Ref<InjectedBundleScriptWorld> InjectedBundleScriptWorld::create()
{
    return adoptRef(*new InjectedBundleScriptWorld(ScriptController::createWorld(), uniqueWorldName()));
}

Ref<InjectedBundleScriptWorld> InjectedBundleScriptWorld::create(const String& name)
{
    return adoptRef(*new InjectedBundleScriptWorld(ScriptController::createWorld(), name));
}

Ref<InjectedBundleScriptWorld> InjectedBundleScriptWorld::getOrCreate(DOMWrapperWorld& world)
{
    if (&world == &mainThreadNormalWorld())
        return normalWorld();

    if (InjectedBundleScriptWorld* existingWorld = allWorlds().get(&world))
        return *existingWorld;

    return adoptRef(*new InjectedBundleScriptWorld(world, uniqueWorldName()));
}

InjectedBundleScriptWorld* InjectedBundleScriptWorld::find(const String& name)
{
    for (auto* world : allWorlds().values()) {
        if (world->name() == name)
            return world;
    }
    return nullptr;
}

InjectedBundleScriptWorld& InjectedBundleScriptWorld::normalWorld()
{
    static InjectedBundleScriptWorld& world = adoptRef(*new InjectedBundleScriptWorld(mainThreadNormalWorld(), String())).leakRef();
    return world;
}

InjectedBundleScriptWorld::InjectedBundleScriptWorld(DOMWrapperWorld& world, const String& name)
    : m_world(world)
    , m_name(name)
{
    ASSERT(!allWorlds().contains(m_world.ptr()));
    allWorlds().add(m_world.ptr(), this);
}

InjectedBundleScriptWorld::~InjectedBundleScriptWorld()
{
    ASSERT(allWorlds().contains(m_world.ptr()));
    allWorlds().remove(m_world.ptr());
}

const DOMWrapperWorld& InjectedBundleScriptWorld::coreWorld() const
{
    return m_world;
}

DOMWrapperWorld& InjectedBundleScriptWorld::coreWorld()
{
    return m_world;
}
    
void InjectedBundleScriptWorld::clearWrappers()
{
    m_world->clearWrappers();
}

void InjectedBundleScriptWorld::makeAllShadowRootsOpen()
{
    m_world->setShadowRootIsAlwaysOpen();
}

void InjectedBundleScriptWorld::disableOverrideBuiltinsBehavior()
{
    m_world->disableOverrideBuiltinsBehavior();
}

} // namespace WebKit
