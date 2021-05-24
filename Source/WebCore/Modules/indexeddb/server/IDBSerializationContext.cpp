/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IDBSerializationContext.h"

#include "DOMWrapperWorld.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/JSObjectInlines.h>
#include <wtf/Lock.h>

namespace WebCore {

namespace IDBServer {

static Lock serializationContextMapLock;

static HashMap<Thread*, IDBSerializationContext*>& serializationContextMap() WTF_REQUIRES_LOCK(serializationContextMapLock)
{
    static NeverDestroyed<HashMap<Thread*, IDBSerializationContext*>> map;
    return map;
}

Ref<IDBSerializationContext> IDBSerializationContext::getOrCreateForCurrentThread()
{
    auto& thread = Thread::current();
    Locker locker { serializationContextMapLock };
    auto[iter, isNewEntry] = serializationContextMap().add(&thread, nullptr);
    if (isNewEntry) {
        Ref<IDBSerializationContext> protectedContext = adoptRef(*new IDBSerializationContext(thread));
        iter->value = protectedContext.ptr();
        return protectedContext;
    }

    return *iter->value;
}

IDBSerializationContext::~IDBSerializationContext()
{
    Locker locker { serializationContextMapLock };
    ASSERT(this == serializationContextMap().get(&m_thread));

    if (m_vm) {
        JSC::JSLockHolder lock(*m_vm);
        m_globalObject.clear();
        m_vm = nullptr;
    }
    serializationContextMap().remove(&m_thread);
}

void IDBSerializationContext::initializeVM()
{
    if (m_vm)
        return;

    ASSERT(!m_globalObject);
    m_vm = JSC::VM::create();
    m_vm->heap.acquireAccess();
    JSVMClientData::initNormalWorld(m_vm.get(), WorkerThreadType::Worklet);

    JSC::JSLockHolder locker(m_vm.get());
    m_globalObject.set(*m_vm, JSIDBSerializationGlobalObject::create(*m_vm, JSIDBSerializationGlobalObject::createStructure(*m_vm, JSC::jsNull()), normalWorld(*m_vm)));
}

JSC::VM& IDBSerializationContext::vm()
{
    ASSERT(&m_thread == &Thread::current());

    initializeVM();
    return *m_vm;
}

JSC::JSGlobalObject& IDBSerializationContext::globalObject()
{
    ASSERT(&m_thread == &Thread::current());

    initializeVM();
    return *m_globalObject.get();
}

IDBSerializationContext::IDBSerializationContext(Thread& thread)
    : m_thread(thread)
{
}

} // namespace IDBServer
} // namespace WebCore
