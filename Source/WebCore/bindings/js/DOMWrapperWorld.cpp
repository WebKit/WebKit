/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
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
#include "DOMWrapperWorld.h"

#include "CommonVM.h"
#include "JSEventListener.h"
#include "Logging.h"
#include "WebCoreJSClientData.h"
#include "WebCoreOpaqueRootInlines.h"
#include "WindowProxy.h"
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/HeapCellInlines.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/SlotVisitorInlines.h>
#include <JavaScriptCore/WeakInlines.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
using namespace JSC;

DOMWrapperWorld::DOMWrapperWorld(JSC::VM& vm, Type type, const String& name)
    : m_vm(vm)
    , m_name(name)
    , m_type(type)
{
    VM::ClientData* clientData = m_vm.clientData;
    ASSERT(clientData);
    downcast<JSVMClientData>(clientData)->rememberWorld(*this);
}

DOMWrapperWorld::~DOMWrapperWorld()
{
    VM::ClientData* clientData = m_vm.clientData;
    ASSERT(clientData);
    downcast<JSVMClientData>(clientData)->forgetWorld(*this);

    // These items are created lazily.
    while (!m_jsWindowProxies.isEmpty())
        protect(*m_jsWindowProxies.begin())->destroyJSWindowProxy(*this);
}

// The wrapper is not what keeps the view alive, so the view's address, which is its key, could be reused
// for another view while the wrapper is alive. Keeping a reference to the view prevents that.
struct DOMWrapperWorld::ArrayBufferViewWrapper {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(ArrayBufferViewWrapper);

    ArrayBufferViewWrapper(DOMWrapperWorld& world, JSC::ArrayBufferView& view)
        : world(world)
        , view(view)
    {
    }

    SingleThreadWeakPtr<DOMWrapperWorld> world;
    const Ref<JSC::ArrayBufferView> view;
    JSC::Weak<JSC::JSArrayBufferView> wrapper;
};

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(DOMWrapperWorld::ArrayBufferViewWrapper);

class DOMWrapperWorld::ArrayBufferViewWrapperOwner final : public JSC::WeakHandleOwner {
public:
    bool isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown>, void* context, JSC::AbstractSlotVisitor& visitor, ASCIILiteral* reason) final
    {
        if (reason) [[unlikely]]
            *reason = "ArrayBufferView is opaque root"_s;
        auto& entry = *static_cast<ArrayBufferViewWrapper*>(context);
        return containsWebCoreOpaqueRoot(visitor, WebCoreOpaqueRoot { entry.view.ptr() });
    }

    void finalize(JSC::Handle<JSC::Unknown>, void* context) final
    {
        auto& entry = *static_cast<ArrayBufferViewWrapper*>(context);
        if (RefPtr world = entry.world.get())
            world->m_arrayBufferViewWrappers.remove(entry.view.ptr());
    }
};

JSC::JSArrayBufferView* DOMWrapperWorld::arrayBufferViewWrapper(JSC::ArrayBufferView& view) const
{
    auto* entry = m_arrayBufferViewWrappers.get(&view);
    return entry ? entry->wrapper.get() : nullptr;
}

void DOMWrapperWorld::cacheArrayBufferViewWrapper(JSC::ArrayBufferView& view, JSC::JSArrayBufferView& wrapper)
{
    static NeverDestroyed<ArrayBufferViewWrapperOwner> owner;

    auto& entry = *m_arrayBufferViewWrappers.ensure(&view, [&] {
        return makeUnique<ArrayBufferViewWrapper>(*this, view);
    }).iterator->value;
    if (entry.wrapper)
        return;

    entry.wrapper = JSC::Weak<JSC::JSArrayBufferView>(&wrapper, &owner.get(), &entry);
}

void DOMWrapperWorld::clearWrappers()
{
    if (!m_eventListeners.isEmptyIgnoringNullReferences()) {
        RELEASE_LOG_ERROR(Bindings, "DOMWrapperWorld::clearWrappers() called when there were still registered event listeners");
        auto eventListeners = std::exchange(m_eventListeners, { });
        for (Ref eventListener : eventListeners)
            eventListener->invalidate();
    }

    m_wrappers.clear();
    m_arrayBufferViewWrappers.clear();

    // These items are created lazily.
    while (!m_jsWindowProxies.isEmpty())
        protect(*m_jsWindowProxies.begin())->destroyJSWindowProxy(*this);
}

void DOMWrapperWorld::addEventListener(JSEventListener& listener)
{
    m_eventListeners.add(listener);
}

void DOMWrapperWorld::removeEventListener(JSEventListener& listener)
{
    m_eventListeners.remove(listener);
}

DOMWrapperWorld& normalWorld(JSC::VM& vm)
{
    VM::ClientData* clientData = vm.clientData;
    ASSERT(clientData);
    return downcast<JSVMClientData>(clientData)->normalWorldSingleton();
}

DOMWrapperWorld& mainThreadNormalWorldSingleton()
{
    ASSERT(isMainThread());
    static NeverDestroyed<Ref<DOMWrapperWorld>> cachedNormalWorld = normalWorld(commonVM());
    return cachedNormalWorld->get();
}

} // namespace WebCore
