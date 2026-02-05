/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006-2025 Apple Inc. All rights reserved.
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
#include "DOMWrapperWorld.h"
#include "DocumentPage.h"
#include "FrameConsoleClient.h"
#include "GarbageCollectionController.h"
#include "JSDOMWindowBase.h"
#include "JSWindowProxy.h"
#include "LocalFrame.h"
#include "Page.h"
#include "PageGroup.h"
#include "RemoteFrame.h"
#include "ScriptController.h"
#include "runtime_root.h"
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/StrongInlines.h>
#include <JavaScriptCore/WeakGCMapInlines.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace JSC;

static void collectGarbageAfterWindowProxyDestruction()
{
    // Make sure to GC Extra Soon(tm) during memory pressure conditions
    // to soften high peaks of memory usage during navigation.
    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        // NOTE: We do the collection on next runloop to ensure that there's no pointer
        //       to the window object on the stack.
        GarbageCollectionController::singleton().garbageCollectOnNextRunLoop();
    } else
        GarbageCollectionController::singleton().garbageCollectSoon();
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(WindowProxy);

WindowProxy::WindowProxy(Frame& frame)
    : m_frame(frame)
{
}

Ref<WindowProxy> WindowProxy::create(Frame& frame)
{
    return adoptRef(*new WindowProxy(frame));
}

WindowProxy::~WindowProxy()
{
    ASSERT(!m_frame);
    ASSERT(m_jsWindowProxies.isEmpty());
}

Frame* WindowProxy::frame() const
{
    return m_frame;
}

void WindowProxy::detachFromFrame()
{
    ASSERT(m_frame);

    m_frame = nullptr;

    // It's likely that destroying proxies will create a lot of garbage.
    if (!m_jsWindowProxies.isEmpty()) {
        do {
            auto it = m_jsWindowProxies.begin();
            it->value->window()->setConsoleClient(nullptr);
            destroyJSWindowProxy(it->key);
        } while (!m_jsWindowProxies.isEmpty());
        collectGarbageAfterWindowProxyDestruction();
    }
}

void WindowProxy::replaceFrame(Frame& frame)
{
    ASSERT(m_frame);
    m_frame = frame;
    setDOMWindow(protect(frame.window()).get());
}

void WindowProxy::destroyJSWindowProxy(DOMWrapperWorld& world)
{
    ASSERT(m_jsWindowProxies.contains(&world));
    m_jsWindowProxies.remove(&world);
    world.didDestroyWindowProxy(this);
}

JSWindowProxy& WindowProxy::createJSWindowProxy(DOMWrapperWorld& world)
{
    ASSERT(m_frame);

    ASSERT(!m_jsWindowProxies.contains(&world));
    ASSERT(m_frame->window());

    VM& vm = world.vm();

    Strong<JSWindowProxy> jsWindowProxy(vm, &JSWindowProxy::create(vm, *protect(m_frame->window()).get(), world));
    m_jsWindowProxies.add(world, jsWindowProxy);
    world.didCreateWindowProxy(this);
    return *jsWindowProxy.get();
}

Vector<JSC::Strong<JSWindowProxy>> WindowProxy::jsWindowProxiesAsVector() const
{
    return copyToVector(m_jsWindowProxies.values());
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
    if (RefPtr localFrame = dynamicDowncast<LocalFrame>(*m_frame))
        localFrame->checkedScript()->initScriptForWindowProxy(windowProxy);
    return windowProxy;
}

void WindowProxy::clearJSWindowProxiesNotMatchingDOMWindow(DOMWindow* newDOMWindow, bool goingIntoBackForwardCache)
{
    if (m_jsWindowProxies.isEmpty())
        return;

    JSLockHolder lock(commonVM());

    for (auto& windowProxy : jsWindowProxiesAsVector()) {
        if (&windowProxy->wrapped() == newDOMWindow)
            continue;

        // Clear the debugger and console from the current window before setting the new window.
        windowProxy->attachDebugger(nullptr);
        windowProxy->window()->setConsoleClient(nullptr);
        if (auto* jsDOMWindow = jsDynamicCast<JSDOMWindowBase*>(windowProxy->window()))
            jsDOMWindow->willRemoveFromWindowProxy();
    }

    // It's likely that resetting our windows created a lot of garbage, unless
    // it went in a back/forward cache.
    if (!goingIntoBackForwardCache)
        collectGarbageAfterWindowProxyDestruction();
}

void WindowProxy::setDOMWindow(DOMWindow* newDOMWindow)
{
    ASSERT(newDOMWindow);

    if (m_jsWindowProxies.isEmpty())
        return;

    ASSERT(m_frame);

    JSLockHolder lock(commonVM());

    for (auto& windowProxy : jsWindowProxiesAsVector()) {
        if (&windowProxy->wrapped() == newDOMWindow)
            continue;

        windowProxy->setWindow(*newDOMWindow);

        if (RefPtr localFrame = dynamicDowncast<LocalFrame>(m_frame.get())) {
            CheckedRef scriptController = localFrame->script();

            // ScriptController's m_cacheableBindingRootObject persists between page navigations
            // so needs to know about the new JSDOMWindow.
            if (RefPtr cacheableBindingRootObject = scriptController->existingCacheableBindingRootObject())
                cacheableBindingRootObject->updateGlobalObject(windowProxy->window());

            windowProxy->window()->setConsoleClient(localFrame->console());
        }

        RefPtr page = m_frame->page();
        windowProxy->attachDebugger(page ? page->debugger() : nullptr);
        if (page)
            windowProxy->window()->setProfileGroup(page->group().identifier());
    }
}

void WindowProxy::attachDebugger(JSC::Debugger* debugger)
{
    for (auto& windowProxy : m_jsWindowProxies.values())
        windowProxy->attachDebugger(debugger);
}

DOMWindow* WindowProxy::window() const
{
    RefPtr frame = m_frame.get();
    return frame ? frame->window() : nullptr;
}

JSWindowProxy* WindowProxy::existingJSWindowProxy(DOMWrapperWorld& world) const
{
    return m_jsWindowProxies.get(&world).get();
}

JSWindowProxy* WindowProxy::jsWindowProxy(DOMWrapperWorld& world)
{
    if (!m_frame)
        return nullptr;

    if (auto* existingProxy = existingJSWindowProxy(world))
        return existingProxy;

    return &createJSWindowProxyWithInitializedScript(world);
}

} // namespace WebCore
