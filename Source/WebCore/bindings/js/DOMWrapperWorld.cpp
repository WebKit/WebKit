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
#include "WindowProxy.h"
#include <JavaScriptCore/HeapCellInlines.h>
#include <JavaScriptCore/SlotVisitorInlines.h>
#include <JavaScriptCore/WeakInlines.h>
#include <wtf/MainThread.h>

#if PLATFORM(COCOA)
#include <mutex>
#include <sys/mman.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PageBlock.h>
#include <wtf/WeakRandomNumber.h>
#endif

namespace WebCore {
using namespace JSC;

#if PLATFORM(COCOA)

bool gGuardWrapperMaps = false;

// Guard ~1 in N WebContent processes (tuning knob: lower the average Speedometer cost to rate x full).
static constexpr unsigned kWrapperMapGuardSampleRate = 64;

static void initializeWrapperMapGuardingOnce()
{
    static std::once_flag flag;
    std::call_once(flag, [] {
        gGuardWrapperMaps = !(weakRandomNumber<unsigned>() % kWrapperMapGuardSampleRate);
    });
}

thread_local WrapperMutationScope* WrapperMutationScope::s_active { nullptr };

// Registry of live page-aligned m_wrappers backings, so free() knows the mmap length to unmap.
static Lock backingRegistryLock;
static HashMap<void*, size_t>& backingRegistry() WTF_REQUIRES_LOCK(backingRegistryLock)
{
    static NeverDestroyed<HashMap<void*, size_t>> registry;
    return registry;
}

void* WrapperMapTableMalloc::allocate(size_t size)
{
    size_t pageSize = WTF::pageSize();
    size_t requested = size ? size : 1;
    size_t rounded = (requested + pageSize - 1) & ~(pageSize - 1);
    void* base = mmap(nullptr, rounded, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    RELEASE_ASSERT(base != MAP_FAILED);
    {
        Locker locker { backingRegistryLock };
        backingRegistry().set(base, rounded);
    }
    // Newly allocated table starts writable; the surrounding WrapperMutationScope re-protects it.
    if (auto* world = WrapperMutationScope::currentlyMutatedWorld())
        world->noteTableBacking(base, rounded);
    return base;
}

void* WrapperMapTableMalloc::malloc(size_t size) { return gGuardWrapperMaps ? allocate(size) : WTF::fastMalloc(size); }
void* WrapperMapTableMalloc::zeroedMalloc(size_t size) { return gGuardWrapperMaps ? allocate(size) : WTF::fastZeroedMalloc(size); } // mmap(MAP_ANON) is zero-filled.

void WrapperMapTableMalloc::free(void* p)
{
    if (!p)
        return;
    // gGuardWrapperMaps is fixed for the process before the first allocation, so the allocation path
    // and this free path always agree.
    if (!gGuardWrapperMaps) {
        WTF::fastFree(p);
        return;
    }
    size_t size = 0;
    {
        Locker locker { backingRegistryLock };
        size = backingRegistry().take(p);
    }
    RELEASE_ASSERT(size);
    if (auto* world = WrapperMutationScope::currentlyMutatedWorld())
        world->forgetTableBacking(p);
    munmap(p, size);
}

void DOMWrapperWorld::setWrappersTableWritable(bool writable)
{
    if (writable) {
        if (m_wrappersTableWritableDepth++)
            return;
    } else {
        ASSERT(m_wrappersTableWritableDepth);
        if (--m_wrappersTableWritableDepth)
            return;
    }
    if (m_wrappersTableBase)
        SUPPRESS_NODELETE RELEASE_ASSERT(!mprotect(m_wrappersTableBase, m_wrappersTableSize, PROT_READ | (writable ? PROT_WRITE : 0)));
}

void WrapperMutationScope::enter()
{
    m_previous = s_active;
    s_active = this;
    m_world->setWrappersTableWritable(true);
}

void WrapperMutationScope::leave()
{
    m_world->setWrappersTableWritable(false);
    s_active = m_previous;
}

#endif // PLATFORM(COCOA)

DOMWrapperWorld::DOMWrapperWorld(JSC::VM& vm, Type type, const String& name)
    : m_vm(vm)
    , m_name(name)
    , m_type(type)
{
#if PLATFORM(COCOA)
    initializeWrapperMapGuardingOnce();
#endif
    VM::ClientData* clientData = m_vm.clientData;
    ASSERT(clientData);
    downcast<JSVMClientData>(clientData)->rememberWorld(*this);
}

DOMWrapperWorld::~DOMWrapperWorld()
{
    VM::ClientData* clientData = m_vm.clientData;
    ASSERT(clientData);
    downcast<JSVMClientData>(clientData)->forgetWorld(*this);

    // The m_wrappers member destructor (runs after this body) destroys buckets and frees the
    // table, which writes to it; make the read-only backing writable so those writes don't fault.
#if PLATFORM(COCOA)
    if (m_wrappersTableBase) {
        mprotect(m_wrappersTableBase, m_wrappersTableSize, PROT_READ | PROT_WRITE);
        m_wrappersTableWritableDepth = 0;
    }
#endif

    // These items are created lazily.
    while (!m_jsWindowProxies.isEmpty())
        protect(*m_jsWindowProxies.begin())->destroyJSWindowProxy(*this);
}

void DOMWrapperWorld::clearWrappers()
{
    if (!m_eventListeners.isEmptyIgnoringNullReferences()) {
        RELEASE_LOG_ERROR(Bindings, "DOMWrapperWorld::clearWrappers() called when there were still registered event listeners");
        auto eventListeners = std::exchange(m_eventListeners, { });
        for (Ref eventListener : eventListeners)
            eventListener->invalidate();
    }

    {
        WrapperMutationScope scope { *this };
        m_wrappers.clear();
    }

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
