/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006-2019 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WindowProxy.h"

#include "CommonVM.h"
#include "Frame.h"
#include "GCController.h"
#include "JSWindowProxy.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PageGroup.h"
#include "RemoteFrame.h"
#include "ScriptController.h"
#include "runtime_root.h"
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/StrongInlines.h>
#include <JavaScriptCore/WeakGCMapInlines.h>
#include <wtf/MemoryPressureHandler.h>

namespace WebCore {

using namespace JSC;

static void collectGarbageAfterWindowProxyDestruction()
{
    // Make sure to GC Extra Soon(tm) during memory pressure conditions
    // to soften high peaks of memory usage during navigation.
    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        // NOTE: We do the collection on next runloop to ensure that there's no pointer
        //       to the window object on the stack.
        GCController::singleton().garbageCollectOnNextRunLoop();
    } else
        GCController::singleton().garbageCollectSoon();
}

WindowProxy::WindowProxy(AbstractFrame& frame)
    : m_frame(&frame)
    , m_jsWindowProxies(makeUniqueRef<ProxyMap>())
{
}

WindowProxy::~WindowProxy()
{
    ASSERT(!m_frame);
    ASSERT(m_jsWindowProxies->isEmpty());
}

void WindowProxy::detachFromFrame()
{
    ASSERT(m_frame);

    m_frame = nullptr;

    // It's likely that destroying windowProxies will create a lot of garbage.
    if (!m_jsWindowProxies->isEmpty()) {
        while (!m_jsWindowProxies->isEmpty()) {
            auto it = m_jsWindowProxies->begin();
            it->value->window()->setConsoleClient(nullptr);
            destroyJSWindowProxy(*it->key);
        }
        collectGarbageAfterWindowProxyDestruction();
    }
}

void WindowProxy::destroyJSWindowProxy(DOMWrapperWorld& world)
{
    ASSERT(m_jsWindowProxies->contains(&world));
    m_jsWindowProxies->remove(&world);
    world.didDestroyWindowProxy(this);
}

JSWindowProxy& WindowProxy::createJSWindowProxy(DOMWrapperWorld& world)
{
    ASSERT(m_frame);

    ASSERT(!m_jsWindowProxies->contains(&world));
    ASSERT(m_frame->window());

    VM& vm = world.vm();

    Strong<JSWindowProxy> jsWindowProxy(vm, &JSWindowProxy::create(vm, *m_frame->window(), world));
    m_jsWindowProxies->add(&world, jsWindowProxy);
    world.didCreateWindowProxy(this);
    return *jsWindowProxy.get();
}

Vector<JSC::Strong<JSWindowProxy>> WindowProxy::jsWindowProxiesAsVector() const
{
    return copyToVector(m_jsWindowProxies->values());
}

JSDOMGlobalObject* WindowProxy::globalObject(DOMWrapperWorld& world)
{
    if (auto* windowProxy = jsWindowProxy(world))
        return windowProxy->window();
    return nullptr;
}

JSWindowProxy& WindowProxy::createJSWindowProxyWithInitializedScript(DOMWrapperWorld& world)
{
    ASSERT(m_frame);

    JSLockHolder lock(world.vm());
    auto& windowProxy = createJSWindowProxy(world);
    if (is<Frame>(*m_frame))
        downcast<Frame>(*m_frame).script().initScriptForWindowProxy(windowProxy);
    return windowProxy;
}

void WindowProxy::clearJSWindowProxiesNotMatchingDOMWindow(AbstractDOMWindow* newDOMWindow, bool goingIntoBackForwardCache)
{
    if (m_jsWindowProxies->isEmpty())
        return;

    JSLockHolder lock(commonVM());

    for (auto& windowProxy : jsWindowProxiesAsVector()) {
        if (&windowProxy->wrapped() == newDOMWindow)
            continue;

        // Clear the debugger and console from the current window before setting the new window.
        windowProxy->attachDebugger(nullptr);
        windowProxy->window()->setConsoleClient(nullptr);
        if (auto* jsDOMWindow = jsDynamicCast<JSDOMWindowBase*>(windowProxy->vm(), windowProxy->window()))
            jsDOMWindow->willRemoveFromWindowProxy();
    }

    // It's likely that resetting our windows created a lot of garbage, unless
    // it went in a back/forward cache.
    if (!goingIntoBackForwardCache)
        collectGarbageAfterWindowProxyDestruction();
}

void WindowProxy::setDOMWindow(AbstractDOMWindow* newDOMWindow)
{
    ASSERT(newDOMWindow);

    if (m_jsWindowProxies->isEmpty())
        return;

    ASSERT(m_frame);

    JSLockHolder lock(commonVM());

    for (auto& windowProxy : jsWindowProxiesAsVector()) {
        if (&windowProxy->wrapped() == newDOMWindow)
            continue;

        windowProxy->setWindow(*newDOMWindow);

        ScriptController* scriptController = nullptr;
        Page* page = nullptr;
        if (is<Frame>(*m_frame)) {
            auto& frame = downcast<Frame>(*m_frame);
            scriptController = &frame.script();
            page = frame.page();
        }

        // ScriptController's m_cacheableBindingRootObject persists between page navigations
        // so needs to know about the new JSDOMWindow.
        if (auto* cacheableBindingRootObject = scriptController ? scriptController->existingCacheableBindingRootObject() : nullptr)
            cacheableBindingRootObject->updateGlobalObject(windowProxy->window());

        windowProxy->attachDebugger(page ? page->debugger() : nullptr);
        if (page)
            windowProxy->window()->setProfileGroup(page->group().identifier());
        windowProxy->window()->setConsoleClient(page ? &page->console() : nullptr);
    }
}

void WindowProxy::attachDebugger(JSC::Debugger* debugger)
{
    for (auto& windowProxy : m_jsWindowProxies->values())
        windowProxy->attachDebugger(debugger);
}

AbstractDOMWindow* WindowProxy::window() const
{
    return m_frame ? m_frame->window() : nullptr;
}

WindowProxy::ProxyMap::ValuesConstIteratorRange WindowProxy::jsWindowProxies() const
{
    return m_jsWindowProxies->values();
}

WindowProxy::ProxyMap WindowProxy::releaseJSWindowProxies()
{
    return std::exchange(m_jsWindowProxies, makeUniqueRef<ProxyMap>());
}

void WindowProxy::setJSWindowProxies(ProxyMap&& windowProxies)
{
    m_jsWindowProxies = makeUniqueRef<ProxyMap>(WTFMove(windowProxies));
}

} // namespace WebCore
