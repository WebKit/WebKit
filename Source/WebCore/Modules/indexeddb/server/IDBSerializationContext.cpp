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

#include <JavaScriptCore/JSObjectInlines.h>
#include <pal/SessionID.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

namespace IDBServer {

static Lock serializationContextMapMutex;

static HashMap<PAL::SessionID, IDBSerializationContext*>& serializationContextMap()
{
    static NeverDestroyed<HashMap<PAL::SessionID, IDBSerializationContext*>> map;
    return map;
}

Ref<IDBSerializationContext> IDBSerializationContext::getOrCreateIDBSerializationContext(PAL::SessionID sessionID)
{
    Locker<Lock> locker(serializationContextMapMutex);
    auto[iter, isNewEntry] = serializationContextMap().add(sessionID, nullptr);
    if (isNewEntry) {
        Ref<IDBSerializationContext> protectedContext = adoptRef(*new IDBSerializationContext(sessionID));
        iter->value = protectedContext.ptr();
        return protectedContext;
    }

    return *iter->value;
}

IDBSerializationContext::~IDBSerializationContext()
{
    Locker<Lock> locker(serializationContextMapMutex);
    ASSERT(this == serializationContextMap().get(m_sessionID));

    if (m_vm) {
        JSC::JSLockHolder lock(*m_vm);
        m_globalObject.clear();
        m_vm = nullptr;
    }
    serializationContextMap().remove(m_sessionID);
}

void IDBSerializationContext::initializeVM()
{
    if (m_vm)
        return;

    ASSERT(!m_globalObject);
    m_vm = JSC::VM::create();

    JSC::JSLockHolder locker(m_vm.get());
    m_globalObject.set(*m_vm, JSC::JSGlobalObject::create(*m_vm, JSC::JSGlobalObject::createStructure(*m_vm, JSC::jsNull())));
}

JSC::VM& IDBSerializationContext::vm()
{
    initializeVM();
    return *m_vm;
}

JSC::ExecState& IDBSerializationContext::execState()
{
    initializeVM();
    return *m_globalObject.get()->globalExec();
}

IDBSerializationContext::IDBSerializationContext(PAL::SessionID sessionID)
    : m_sessionID(sessionID)
{
}

} // namespace IDBServer
} // namespace WebCore

#endif
