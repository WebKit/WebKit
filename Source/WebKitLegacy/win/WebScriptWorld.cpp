/*
 * Copyright (C) 2009, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebKitDLL.h"
#include "WebScriptWorld.h"

#include <JavaScriptCore/APICast.h>
#include <WebCore/JSDOMBinding.h>
#include <WebCore/ScriptController.h>

using namespace WebCore;

typedef HashMap<DOMWrapperWorld*, WebScriptWorld*> WorldMap;
static WorldMap& allWorlds()
{
    static WorldMap& map = *new WorldMap;
    return map;
}

inline WebScriptWorld::WebScriptWorld(RefPtr<DOMWrapperWorld>&& world)
    : m_world(WTFMove(world))
{
    ASSERT_ARG(world, m_world);

    ASSERT_ARG(world, !allWorlds().contains(m_world.get()));
    allWorlds().add(m_world.get(), this);

    ++gClassCount;
    gClassNameCount().add("WebScriptWorld"_s);
}

WebScriptWorld::~WebScriptWorld()
{
    ASSERT(allWorlds().contains(m_world.get()));
    allWorlds().remove(m_world.get());

    --gClassCount;
    gClassNameCount().remove("WebScriptWorld"_s);
}

WebScriptWorld* WebScriptWorld::standardWorld()
{
    static WebScriptWorld* standardWorld = createInstance(&mainThreadNormalWorld()).leakRef();
    return standardWorld;
}

COMPtr<WebScriptWorld> WebScriptWorld::createInstance()
{
    return createInstance(ScriptController::createWorld("WebScriptWorld"_s, WebCore::ScriptController::WorldType::User));
}

COMPtr<WebScriptWorld> WebScriptWorld::createInstance(RefPtr<DOMWrapperWorld>&& world)
{
    return new WebScriptWorld(WTFMove(world));
}

COMPtr<WebScriptWorld> WebScriptWorld::findOrCreateWorld(DOMWrapperWorld& world)
{
    if (&world == &mainThreadNormalWorld())
        return standardWorld();

    if (WebScriptWorld* existingWorld = allWorlds().get(&world))
        return existingWorld;

    return createInstance(&world);
}

ULONG WebScriptWorld::AddRef()
{
    return ++m_refCount;
}

ULONG WebScriptWorld::Release()
{
    ULONG newRefCount = --m_refCount;
    if (!newRefCount)
        delete this;
    return newRefCount;
}

HRESULT WebScriptWorld::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;

    if (IsEqualIID(riid, __uuidof(WebScriptWorld)))
        *ppvObject = this;
    else if (IsEqualIID(riid, __uuidof(IWebScriptWorld)))
        *ppvObject = static_cast<IWebScriptWorld*>(this);
    else if (IsEqualIID(riid, IID_IUnknown))
        *ppvObject = static_cast<IUnknown*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

HRESULT WebScriptWorld::standardWorld(_COM_Outptr_opt_ IWebScriptWorld** outWorld)
{
    if (!outWorld)
        return E_POINTER;

    *outWorld = standardWorld();
    (*outWorld)->AddRef();
    return S_OK;
}

HRESULT WebScriptWorld::scriptWorldForGlobalContext(JSGlobalContextRef context, IWebScriptWorld** outWorld)
{
    if (!context)
        return E_POINTER;
    if (!outWorld)
        return E_POINTER;
    return findOrCreateWorld(currentWorld(*toJS(context))).copyRefTo(outWorld);
}

HRESULT WebScriptWorld::unregisterWorld()
{
    m_world->clearWrappers();
    return S_OK;
}
