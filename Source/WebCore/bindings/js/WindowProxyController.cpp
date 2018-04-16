/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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
#include "WindowProxyController.h"

#include "CommonVM.h"
#include "Frame.h"
#include "GCController.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PageGroup.h"
#include "RemoteFrame.h"
#include "ScriptController.h"
#include "runtime_root.h"
#include <JavaScriptCore/JSLock.h>
#include <wtf/MemoryPressureHandler.h>

namespace WebCore {

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

WindowProxyController::WindowProxyController(AbstractFrame& frame)
    : m_frame(frame)
{
}

WindowProxyController::~WindowProxyController()
{
    // It's likely that destroying windowProxies will create a lot of garbage.
    if (!m_windowProxies.isEmpty()) {
        while (!m_windowProxies.isEmpty()) {
            auto it = m_windowProxies.begin();
            it->value->window()->setConsoleClient(nullptr);
            destroyWindowProxy(*it->key);
        }
        collectGarbageAfterWindowProxyDestruction();
    }
}

void WindowProxyController::destroyWindowProxy(DOMWrapperWorld& world)
{
    ASSERT(m_windowProxies.contains(&world));
    m_windowProxies.remove(&world);
    world.didDestroyWindowProxy(this);
}

JSDOMWindowProxy& WindowProxyController::createWindowProxy(DOMWrapperWorld& world)
{
    ASSERT(!m_windowProxies.contains(&world));
    ASSERT(m_frame.window());

    VM& vm = world.vm();

    // FIXME: We do not support constructing a JSDOMWindowProxy for a RemoteDOMWindow yet.
    RELEASE_ASSERT(is<DOMWindow>(m_frame.window()));

    Strong<JSDOMWindowProxy> windowProxy(vm, &JSDOMWindowProxy::create(vm, *downcast<DOMWindow>(m_frame.window()), world));
    Strong<JSDOMWindowProxy> windowProxy2(windowProxy);
    m_windowProxies.add(&world, windowProxy);
    world.didCreateWindowProxy(this);
    return *windowProxy.get();
}

Vector<JSC::Strong<JSDOMWindowProxy>> WindowProxyController::windowProxiesAsVector() const
{
    return copyToVector(m_windowProxies.values());
}

JSDOMWindowProxy& WindowProxyController::createWindowProxyWithInitializedScript(DOMWrapperWorld& world)
{
    JSLockHolder lock(world.vm());
    auto& windowProxy = createWindowProxy(world);
    if (is<Frame>(m_frame))
        downcast<Frame>(m_frame).script().initScriptForWindowProxy(windowProxy);
    return windowProxy;
}

void WindowProxyController::clearWindowProxiesNotMatchingDOMWindow(AbstractDOMWindow* newDOMWindow, bool goingIntoPageCache)
{
    if (m_windowProxies.isEmpty())
        return;

    JSLockHolder lock(commonVM());

    for (auto& windowProxy : windowProxiesAsVector()) {
        if (&windowProxy->wrapped() == newDOMWindow)
            continue;

        // Clear the debugger and console from the current window before setting the new window.
        windowProxy->attachDebugger(nullptr);
        windowProxy->window()->setConsoleClient(nullptr);
        windowProxy->window()->willRemoveFromWindowProxy();
    }

    // It's likely that resetting our windows created a lot of garbage, unless
    // it went in a back/forward cache.
    if (!goingIntoPageCache)
        collectGarbageAfterWindowProxyDestruction();
}

void WindowProxyController::setDOMWindowForWindowProxy(AbstractDOMWindow* newDOMWindow)
{
    ASSERT(newDOMWindow);

    if (m_windowProxies.isEmpty())
        return;

    JSLockHolder lock(commonVM());

    for (auto& windowProxy : windowProxiesAsVector()) {
        if (&windowProxy->wrapped() == newDOMWindow)
            continue;

        // FIXME: We do not support setting a RemoteDOMWindow yet.
        ASSERT(is<DOMWindow>(newDOMWindow));

        windowProxy->setWindow(*downcast<DOMWindow>(newDOMWindow));

        ScriptController* scriptController = nullptr;
        Page* page = nullptr;
        if (is<Frame>(m_frame)) {
            auto& frame = downcast<Frame>(m_frame);
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

void WindowProxyController::attachDebugger(JSC::Debugger* debugger)
{
    for (auto& windowProxy : m_windowProxies.values())
        windowProxy->attachDebugger(debugger);
}

} // namespace WebCore
