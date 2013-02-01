/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(INSPECTOR)

#include "InspectorDOMStorageResource.h"

#include "DOMWindow.h"
#include "Document.h"
#include "EventNames.h"
#include "Frame.h"
#include "InspectorFrontend.h"
#include "InspectorValues.h"
#include "SecurityOrigin.h"
#include "Storage.h"
#include "StorageArea.h"
#include "StorageEvent.h"

using namespace JSC;

namespace WebCore {

int InspectorDOMStorageResource::s_nextUnusedId = 1;

InspectorDOMStorageResource::InspectorDOMStorageResource(StorageArea* storageArea, bool isLocalStorage, Frame* frame)
    : m_storageArea(storageArea)
    , m_isLocalStorage(isLocalStorage)
    , m_frame(frame)
    , m_frontend(0)
    , m_id(String::number(s_nextUnusedId++))
{
}

bool InspectorDOMStorageResource::isSameOriginAndType(SecurityOrigin* securityOrigin, bool isLocalStorage) const
{
    return m_frame->document()->securityOrigin()->equal(securityOrigin) && m_isLocalStorage == isLocalStorage;
}

void InspectorDOMStorageResource::bind(InspectorFrontend* frontend)
{
    ASSERT(!m_frontend);
    m_frontend = frontend->domstorage();

    RefPtr<TypeBuilder::DOMStorage::Entry> jsonObject = TypeBuilder::DOMStorage::Entry::create()
        .setOrigin(m_frame->document()->securityOrigin()->toRawString())
        .setIsLocalStorage(m_isLocalStorage)
        .setId(m_id);
    m_frontend->addDOMStorage(jsonObject);
}

void InspectorDOMStorageResource::unbind()
{
    if (!m_frontend)
        return;  // Already unbound.

    m_frontend = 0;
}

void InspectorDOMStorageResource::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::InspectorDOMStorageResources);
    info.addMember(m_storageArea, "storageArea");
    info.addMember(m_frame, "frame");
    info.addWeakPointer(m_frontend);
    info.addMember(m_id, "id");
    info.addPrivateBuffer(m_storageArea->memoryBytesUsedByCache(), 0, "StorageAreaCache", "cache");
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

