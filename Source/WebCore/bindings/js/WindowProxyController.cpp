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

#include "Frame.h"
#include "RemoteFrame.h"
#include "ScriptController.h"
#include <JavaScriptCore/JSLock.h>

namespace WebCore {

WindowProxyController::WindowProxyController(AbstractFrame& frame)
    : m_frame(frame)
{
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

} // namespace WebCore
