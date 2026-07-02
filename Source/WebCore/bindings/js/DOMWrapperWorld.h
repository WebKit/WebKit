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

#include <WebCore/JSDOMGlobalObject.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class JSEventListener;
class WindowProxy;

#if PLATFORM(COCOA)
// Diagnostic (rdar://157587352): allocate the m_wrappers hash-table backing in its own page(s)
// so it can be kept read-only except during the world's own mutating operations. Any stray write
// that corrupts the map (the bug we are hunting) then faults immediately with the writer's stack.
// To keep the cost off Speedometer, guarding is sampled per-process: gGuardWrapperMaps is decided
// once at startup, and when false the allocator falls back to FastMalloc and the scopes are no-ops.
// The whole facility relies on mmap/mprotect and is only built on Cocoa; elsewhere m_wrappers is a
// plain HashMap and none of this code exists.
WEBCORE_EXPORT extern bool gGuardWrapperMaps;

struct WrapperMapTableMalloc {
    WEBCORE_EXPORT static void* malloc(size_t);
    WEBCORE_EXPORT static void* zeroedMalloc(size_t);
    WEBCORE_EXPORT static void free(void*);
private:
    static void* allocate(size_t);
};

using DOMObjectWrapperMap = HashMap<void*, JSC::Weak<JSC::JSObject>, WTF::DefaultHash<void*>, WTF::HashTraits<void*>, WTF::HashTraits<JSC::Weak<JSC::JSObject>>, WTF::HashTableTraits, WTF::ShouldValidateKey::Yes, WrapperMapTableMalloc>;
#else
using DOMObjectWrapperMap = HashMap<void*, JSC::Weak<JSC::JSObject>>;
#endif

class DOMWrapperWorld : public RefCounted<DOMWrapperWorld>, public CanMakeSingleThreadWeakPtr<DOMWrapperWorld> {
#if PLATFORM(COCOA)
    friend struct WrapperMapTableMalloc;
    friend class WrapperMutationScope;
#endif
public:
    enum class Type {
        Normal,   // Main (e.g. Page)
        User,     // User Scripts (e.g. Extensions)
        Internal, // WebKit Internal (e.g. Media Controls)
    };

    static Ref<DOMWrapperWorld> create(JSC::VM& vm, Type type = Type::Internal, const String& name = { })
    {
        return adoptRef(*new DOMWrapperWorld(vm, type, name));
    }
    WEBCORE_EXPORT ~DOMWrapperWorld();

    // Free as much memory held onto by this world as possible.
    WEBCORE_EXPORT void clearWrappers();

    void didCreateWindowProxy(WindowProxy* controller) { m_jsWindowProxies.add(controller); }
    void didDestroyWindowProxy(WindowProxy* controller) { m_jsWindowProxies.remove(controller); }

    void setAllowAutofill() { m_allowAutofill = true; }
    bool allowAutofill() const { return m_allowAutofill; }

    bool allowsJSHandleCreation() const { return m_allowsJSHandleCreation; }
    void setAllowsJSHandleCreation() { m_allowsJSHandleCreation = true; }

    void setAllowNodeSerialization() { m_allowNodeSerialization = true; }
    bool allowNodeSerialization() const { return m_allowNodeSerialization; }

    void setAllowElementUserInfo() { m_allowElementUserInfo = true; }
    bool allowElementUserInfo() const { return m_allowElementUserInfo; }

    bool canAccessAnyShadowRoot() const { return shadowRootIsAlwaysOpen() || closedShadowRootIsExposedForExtensions(); }

    void setShadowRootIsAlwaysOpen() { m_shadowRootIsAlwaysOpen = true; }
    bool shadowRootIsAlwaysOpen() const { return m_shadowRootIsAlwaysOpen; }

    void setClosedShadowRootIsExposedForExtensions() { m_closedShadowRootIsExposedForExtensions = true; }
    bool closedShadowRootIsExposedForExtensions() const { return m_closedShadowRootIsExposedForExtensions; }

    void disableLegacyOverrideBuiltInsBehavior() { m_shouldDisableLegacyOverrideBuiltInsBehavior = true; }
    bool shouldDisableLegacyOverrideBuiltInsBehavior() const { return m_shouldDisableLegacyOverrideBuiltInsBehavior; }

    void setAllowPostLegacySynchronousMessage() { m_allowPostLegacySynchronousMessage = true; }
    bool allowPostLegacySynchronousMessage() const { return m_allowPostLegacySynchronousMessage; }

    void setIsMediaControls() { m_isMediaControls = true; }
    bool isMediaControls() const { return m_isMediaControls; }

    DOMObjectWrapperMap& wrappers() LIFETIME_BOUND { return m_wrappers; }

    Type type() const { return m_type; }
    bool isNormal() const { return m_type == Type::Normal; }
    bool isNonNormal() const { return m_type != Type::Normal; }
    bool isUser() const { return m_type == Type::User; }

    const String& name() const LIFETIME_BOUND { return m_name; }

    JSC::VM& vm() const { return m_vm; }

    void addEventListener(JSEventListener&);
    void removeEventListener(JSEventListener&);

protected:
    DOMWrapperWorld(JSC::VM&, Type, const String& name);

private:
    JSC::VM& m_vm;
    HashSet<WindowProxy*> m_jsWindowProxies;
    DOMObjectWrapperMap m_wrappers;

#if PLATFORM(COCOA)
    // Diagnostic page-protection state for m_wrappers (rdar://157587352). The backing is kept
    // read-only except inside a WrapperMutationScope; WrapperMapTableMalloc reports each (re)allocated
    // backing here so the scope can re-protect the current (possibly grown/shrunk) table. The whole
    // facility relies on mmap/mprotect and is only built on Cocoa.
    void noteTableBacking(void* base, size_t size) { m_wrappersTableBase = base; m_wrappersTableSize = size; }
    void forgetTableBacking(void* base) { if (m_wrappersTableBase == base) { m_wrappersTableBase = nullptr; m_wrappersTableSize = 0; } }
    SUPPRESS_NODELETE void NODELETE setWrappersTableWritable(bool);
    void* m_wrappersTableBase { nullptr };
    size_t m_wrappersTableSize { 0 };
    unsigned m_wrappersTableWritableDepth { 0 };
#endif

    WeakHashSet<JSEventListener> m_eventListeners;

    String m_name;
    Type m_type { Type::Internal };

    bool m_allowAutofill : 1 { false };
    bool m_allowElementUserInfo : 1 { false };
    bool m_shadowRootIsAlwaysOpen : 1 { false };
    bool m_closedShadowRootIsExposedForExtensions : 1 { false };
    bool m_shouldDisableLegacyOverrideBuiltInsBehavior : 1 { false };
    bool m_allowsJSHandleCreation : 1 { false };
    bool m_allowNodeSerialization : 1 { false };
    bool m_allowPostLegacySynchronousMessage : 1 { false };
    bool m_isMediaControls : 1 { false };
};

// RAII guard bracketing a mutating operation on a world's m_wrappers. On Cocoa it makes the
// page-protected backing writable for the duration and read-only again afterwards (re-reading the
// current backing, which may have been reallocated by a rehash during the operation); it is a cheap
// branch on gGuardWrapperMaps when this process does not guard its wrapper maps. Off Cocoa the
// diagnostic does not exist and the scope is an empty no-op, so call sites can construct it
// unconditionally on every platform. rdar://157587352.
class WrapperMutationScope {
public:
#if PLATFORM(COCOA)
    explicit WrapperMutationScope(DOMWrapperWorld& world)
        : m_world(world)
    {
        if (gGuardWrapperMaps) [[unlikely]]
            enter();
    }
    ~WrapperMutationScope()
    {
        if (gGuardWrapperMaps) [[unlikely]]
            leave();
    }

    // The world whose m_wrappers backing is currently being mutated on this thread (so the backing
    // allocator can record each (re)allocated table on it), or nullptr when no scope is active.
    static DOMWrapperWorld* currentlyMutatedWorld() { return s_active ? s_active->m_world.ptr() : nullptr; }
#else
    explicit WrapperMutationScope(DOMWrapperWorld&) { }
#endif
    WrapperMutationScope(const WrapperMutationScope&) = delete;
    WrapperMutationScope& operator=(const WrapperMutationScope&) = delete;

#if PLATFORM(COCOA)
private:
    WEBCORE_EXPORT void enter();
    WEBCORE_EXPORT void leave();
    static thread_local WrapperMutationScope* s_active;
    SingleThreadWeakRef<DOMWrapperWorld> m_world;
    WrapperMutationScope* m_previous { nullptr };
#endif
};

DOMWrapperWorld& NODELETE normalWorld(JSC::VM&);
WEBCORE_EXPORT DOMWrapperWorld& mainThreadNormalWorldSingleton();

inline DOMWrapperWorld& debuggerWorldSingleton() { return mainThreadNormalWorldSingleton(); }
inline DOMWrapperWorld& pluginWorldSingleton() { return mainThreadNormalWorldSingleton(); }

DOMWrapperWorld& currentWorld(JSC::JSGlobalObject&);
DOMWrapperWorld& worldForDOMObject(JSC::JSObject&);

inline DOMWrapperWorld& currentWorld(JSC::JSGlobalObject& lexicalGlobalObject)
{
    return uncheckedDowncast<JSDOMGlobalObject>(&lexicalGlobalObject)->world();
}

inline DOMWrapperWorld& worldForDOMObject(JSC::JSObject& object)
{
    return uncheckedDowncast<JSDOMGlobalObject>(object.realm())->world();
}

} // namespace WebCore
