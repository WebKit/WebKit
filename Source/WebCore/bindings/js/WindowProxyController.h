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

#pragma once

#include "JSDOMWindowProxy.h"
#include <JavaScriptCore/Strong.h>
#include <wtf/HashMap.h>

namespace WebCore {

class AbstractFrame;

class WindowProxyController {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using ProxyMap = HashMap<RefPtr<DOMWrapperWorld>, JSC::Strong<JSDOMWindowProxy>>;

    explicit WindowProxyController(AbstractFrame&);

    void destroyWindowProxy(DOMWrapperWorld&);

    ProxyMap::ValuesConstIteratorRange windowProxies() const { return m_windowProxies.values(); }
    Vector<JSC::Strong<JSDOMWindowProxy>> windowProxiesAsVector() const;

    ProxyMap releaseWindowProxies() { return std::exchange(m_windowProxies, ProxyMap()); }
    void setWindowProxies(ProxyMap&& windowProxies) { m_windowProxies = WTFMove(windowProxies); }

    JSDOMWindowProxy& windowProxy(DOMWrapperWorld& world)
    {
        auto it = m_windowProxies.find(&world);
        if (it != m_windowProxies.end())
            return *it->value.get();

        return createWindowProxyWithInitializedScript(world);
    }

    JSDOMWindowProxy* existingWindowProxy(DOMWrapperWorld& world) const
    {
        auto it = m_windowProxies.find(&world);
        return (it != m_windowProxies.end()) ? it->value.get() : nullptr;
    }

    JSDOMGlobalObject* globalObject(DOMWrapperWorld& world)
    {
        return windowProxy(world).window();
    }

private:
    JSDOMWindowProxy& createWindowProxy(DOMWrapperWorld&);
    WEBCORE_EXPORT JSDOMWindowProxy& createWindowProxyWithInitializedScript(DOMWrapperWorld&);

    AbstractFrame& m_frame;
    ProxyMap m_windowProxies;
};

} // namespace WebCore
