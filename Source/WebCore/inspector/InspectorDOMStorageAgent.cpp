/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "InspectorDOMStorageAgent.h"

#if ENABLE(INSPECTOR) && ENABLE(DOM_STORAGE)

#include "Database.h"
#include "DOMWindow.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "InspectorDOMStorageResource.h"
#include "InspectorFrontend.h"
#include "InspectorValues.h"
#include "Storage.h"
#include "VoidCallback.h"

#include <wtf/Vector.h>

namespace WebCore {

InspectorDOMStorageAgent::~InspectorDOMStorageAgent()
{
}

void InspectorDOMStorageAgent::getDOMStorageEntries(long storageId, RefPtr<InspectorArray>* entries)
{
    InspectorDOMStorageResource* storageResource = getDOMStorageResourceForId(storageId);
    if (storageResource) {
        storageResource->startReportingChangesToFrontend();
        Storage* domStorage = storageResource->domStorage();
        for (unsigned i = 0; i < domStorage->length(); ++i) {
            String name(domStorage->key(i));
            String value(domStorage->getItem(name));
            RefPtr<InspectorArray> entry = InspectorArray::create();
            entry->pushString(name);
            entry->pushString(value);
            (*entries)->pushArray(entry);
        }
    }
}

void InspectorDOMStorageAgent::setDOMStorageItem(long storageId, const String& key, const String& value, bool* success)
{
    InspectorDOMStorageResource* storageResource = getDOMStorageResourceForId(storageId);
    if (storageResource) {
        ExceptionCode exception = 0;
        storageResource->domStorage()->setItem(key, value, exception);
        *success = !exception;
    }
}

void InspectorDOMStorageAgent::removeDOMStorageItem(long storageId, const String& key, bool* success)
{
    InspectorDOMStorageResource* storageResource = getDOMStorageResourceForId(storageId);
    if (storageResource) {
        storageResource->domStorage()->removeItem(key);
        *success = true;
    }
}

void InspectorDOMStorageAgent::selectDOMStorage(Storage* storage)
{
    ASSERT(storage);
    if (!m_frontend)
        return;

    Frame* frame = storage->frame();
    ExceptionCode ec = 0;
    bool isLocalStorage = (frame->domWindow()->localStorage(ec) == storage && !ec);
    long storageResourceId = 0;
    DOMStorageResourcesMap::iterator domStorageEnd = m_domStorageResources->end();
    for (DOMStorageResourcesMap::iterator it = m_domStorageResources->begin(); it != domStorageEnd; ++it) {
        if (it->second->isSameHostAndType(frame, isLocalStorage)) {
            storageResourceId = it->first;
            break;
        }
    }
    if (storageResourceId)
        m_frontend->selectDOMStorage(storageResourceId);
}

InspectorDOMStorageAgent::InspectorDOMStorageAgent(DOMStorageResourcesMap* domStorageResources, InspectorFrontend* frontend)
    : m_domStorageResources(domStorageResources)
    , m_frontend(frontend)
{
}

InspectorDOMStorageResource* InspectorDOMStorageAgent::getDOMStorageResourceForId(long storageId)
{
    DOMStorageResourcesMap::iterator it = m_domStorageResources->find(storageId);
    if (it == m_domStorageResources->end())
        return 0;
    return it->second.get();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(DOM_STORE)
