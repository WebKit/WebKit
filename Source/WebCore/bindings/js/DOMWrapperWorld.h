/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2009 Google, Inc. All rights reserved.
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

#include "JSDOMGlobalObject.h"
#include <wtf/Forward.h>

namespace WebCore {

class WindowProxy;

typedef HashMap<void*, JSC::Weak<JSC::JSObject>> DOMObjectWrapperMap;

class DOMWrapperWorld : public RefCounted<DOMWrapperWorld> {
public:
    static Ref<DOMWrapperWorld> create(JSC::VM& vm, bool isNormal = false)
    {
        return adoptRef(*new DOMWrapperWorld(vm, isNormal));
    }
    WEBCORE_EXPORT ~DOMWrapperWorld();

    // Free as much memory held onto by this world as possible.
    WEBCORE_EXPORT void clearWrappers();

    void didCreateWindowProxy(WindowProxy* controller) { m_jsWindowProxies.add(controller); }
    void didDestroyWindowProxy(WindowProxy* controller) { m_jsWindowProxies.remove(controller); }

    void setShadowRootIsAlwaysOpen() { m_shadowRootIsAlwaysOpen = true; }
    bool shadowRootIsAlwaysOpen() const { return m_shadowRootIsAlwaysOpen; }

    void disableOverrideBuiltinsBehavior() { m_shouldDisableOverrideBuiltinsBehavior = true; }
    bool shouldDisableOverrideBuiltinsBehavior() const { return m_shouldDisableOverrideBuiltinsBehavior; }

    DOMObjectWrapperMap& wrappers() { return m_wrappers; }

    bool isNormal() const { return m_isNormal; }

    JSC::VM& vm() const { return m_vm; }

protected:
    DOMWrapperWorld(JSC::VM&, bool isNormal);

private:
    JSC::VM& m_vm;
    HashSet<WindowProxy*> m_jsWindowProxies;
    DOMObjectWrapperMap m_wrappers;

    bool m_isNormal;
    bool m_shadowRootIsAlwaysOpen { false };
    bool m_shouldDisableOverrideBuiltinsBehavior { false };
};

DOMWrapperWorld& normalWorld(JSC::VM&);
WEBCORE_EXPORT DOMWrapperWorld& mainThreadNormalWorld();

inline DOMWrapperWorld& debuggerWorld() { return mainThreadNormalWorld(); }
inline DOMWrapperWorld& pluginWorld() { return mainThreadNormalWorld(); }

DOMWrapperWorld& currentWorld(JSC::ExecState&);
DOMWrapperWorld& worldForDOMObject(JSC::JSObject&);

// Helper function for code paths that must not share objects across isolated DOM worlds.
bool isWorldCompatible(JSC::ExecState&, JSC::JSValue);

inline DOMWrapperWorld& currentWorld(JSC::ExecState& state)
{
    return JSC::jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject())->world();
}

inline DOMWrapperWorld& worldForDOMObject(JSC::JSObject& object)
{
    return JSC::jsCast<JSDOMGlobalObject*>(object.globalObject())->world();
}

inline bool isWorldCompatible(JSC::ExecState& state, JSC::JSValue value)
{
    return !value.isObject() || &worldForDOMObject(*value.getObject()) == &currentWorld(state);
}

} // namespace WebCore
