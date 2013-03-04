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
#include "V8Binding.h"
#include "WebCoreMemoryInstrumentation.h"
#include <wtf/MainThread.h>

namespace WebCore {

DOMDataStore::DOMDataStore(Type type)
    : m_type(type)
    , m_wrapperMap(v8::Isolate::GetCurrent()) // FIXME Don't call GetCurrent twice.
{
    V8PerIsolateData::current()->registerDOMDataStore(this);
}

DOMDataStore::~DOMDataStore()
{
    ASSERT(m_type != MainWorld); // We never actually destruct the main world's DOMDataStore.
    V8PerIsolateData::current()->unregisterDOMDataStore(this);
    m_wrapperMap.clear();
}

DOMDataStore* DOMDataStore::mainWorldStore()
{
    DEFINE_STATIC_LOCAL(DOMDataStore, mainWorldDOMDataStore, (MainWorld));
    ASSERT(isMainThread());
    return &mainWorldDOMDataStore;
}

DOMDataStore* DOMDataStore::current(v8::Isolate* isolate)
{
    V8PerIsolateData* data = isolate ? V8PerIsolateData::from(isolate) : V8PerIsolateData::current();
    if (UNLIKELY(!!data->domDataStore()))
        return data->domDataStore();

    if (DOMWrapperWorld::isolatedWorldsExist()) {
        DOMWrapperWorld* isolatedWorld = DOMWrapperWorld::isolatedWorld(v8::Context::GetEntered());
        if (UNLIKELY(!!isolatedWorld))
            return isolatedWorld->isolatedWorldDOMDataStore();
    }

    return mainWorldStore();
}

void DOMDataStore::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Binding);
    info.addMember(m_wrapperMap, "wrapperMap");
}

} // namespace WebCore
