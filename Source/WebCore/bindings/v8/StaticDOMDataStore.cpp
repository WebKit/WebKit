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
#include "StaticDOMDataStore.h"
#include "V8Binding.h"
#include "WebCoreMemoryInstrumentation.h"

namespace WebCore {

StaticDOMDataStore::StaticDOMDataStore()
{
    m_domNodeMap = &m_staticDomNodeMap;
    m_activeDomNodeMap = &m_staticActiveDomNodeMap;
    m_domObjectMap = &m_staticDomObjectMap;
    m_activeDomObjectMap = &m_staticActiveDomObjectMap;
    V8PerIsolateData::current()->registerDOMDataStore(this);
}

StaticDOMDataStore::~StaticDOMDataStore()
{
    V8PerIsolateData::current()->unregisterDOMDataStore(this);
}

void StaticDOMDataStore::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Binding);
    DOMDataStore::reportMemoryUsage(memoryObjectInfo);
    info.addMember(m_staticDomNodeMap);
    info.addMember(m_staticActiveDomNodeMap);
    info.addMember(m_staticDomObjectMap);
    info.addMember(m_staticActiveDomObjectMap);
}

} // namespace WebCore
