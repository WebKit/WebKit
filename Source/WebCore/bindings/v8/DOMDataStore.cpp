/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DOMDataStore.h"

#include "DOMWrapperMap.h"
#include "IntrusiveDOMWrapperMap.h"
#include "V8Binding.h"
#include "WebCoreMemoryInstrumentation.h"
#include <wtf/MainThread.h>

namespace WebCore {

DOMDataStore::DOMDataStore(Type type)
    : m_type(type)
{
    if (type == MainWorld) {
        m_domNodeMap = adoptPtr(new DOMNodeWrapperMap);
        m_activeDomNodeMap = adoptPtr(new ActiveDOMNodeWrapperMap);
    } else {
        ASSERT(type == IsolatedWorld || type == Worker);
        // FIXME: In principle, we shouldn't need to create these
        // wrapper maps for workers because there are no Nodes on
        // worker threads.
        m_domNodeMap = adoptPtr(new DOMWrapperHashMap<Node>);
        m_activeDomNodeMap = adoptPtr(new DOMWrapperHashMap<Node>);
    }
    m_domObjectMap = adoptPtr(new DOMWrapperHashMap<void>);
    m_activeDomObjectMap = adoptPtr(new DOMWrapperHashMap<void>);

    V8PerIsolateData::current()->registerDOMDataStore(this);
}

DOMDataStore::~DOMDataStore()
{
    ASSERT(m_type != MainWorld); // We never actually destruct the main world's DOMDataStore.

    V8PerIsolateData::current()->unregisterDOMDataStore(this);

    if (m_type == IsolatedWorld) {
        m_domNodeMap->clear();
        m_activeDomNodeMap->clear();
    }
    m_domObjectMap->clear();
    m_activeDomObjectMap->clear();
}

DOMDataStore* DOMDataStore::current(v8::Isolate* isolate)
{
    DEFINE_STATIC_LOCAL(DOMDataStore, defaultStore, (MainWorld));
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    if (UNLIKELY(!!data->domDataStore()))
        return data->domDataStore();
    V8DOMWindowShell* context = V8DOMWindowShell::getEntered();
    if (UNLIKELY(!!context))
        return context->world()->domDataStore();
    return &defaultStore;
}

void DOMDataStore::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Binding);
    info.addMember(m_domNodeMap);
    info.addMember(m_activeDomNodeMap);
    info.addMember(m_domObjectMap);
    info.addMember(m_activeDomObjectMap);
}

} // namespace WebCore
