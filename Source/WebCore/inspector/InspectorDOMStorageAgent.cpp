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

#if ENABLE(INSPECTOR)

#include "InspectorDOMStorageAgent.h"

#include "Database.h"
#include "DOMWindow.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "InspectorDOMStorageResource.h"
#include "InspectorFrontend.h"
#include "InspectorState.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "Storage.h"
#include "StorageArea.h"
#include "VoidCallback.h"

#include <wtf/Vector.h>

namespace WebCore {

namespace DOMStorageAgentState {
static const char domStorageAgentEnabled[] = "domStorageAgentEnabled";
};

InspectorDOMStorageAgent::InspectorDOMStorageAgent(InstrumentingAgents* instrumentingAgents, InspectorState* state)
    : InspectorBaseAgent<InspectorDOMStorageAgent>("DOMStorage", instrumentingAgents, state)
    , m_frontend(0)
    , m_enabled(false)
{
    m_instrumentingAgents->setInspectorDOMStorageAgent(this);
}

InspectorDOMStorageAgent::~InspectorDOMStorageAgent()
{
    m_instrumentingAgents->setInspectorDOMStorageAgent(0);
    m_instrumentingAgents = 0;
}

void InspectorDOMStorageAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend;
}

void InspectorDOMStorageAgent::clearFrontend()
{
    DOMStorageResourcesMap::iterator domStorageEnd = m_resources.end();
    for (DOMStorageResourcesMap::iterator it = m_resources.begin(); it != domStorageEnd; ++it)
        it->second->unbind();
    m_frontend = 0;
    disable(0);
}

void InspectorDOMStorageAgent::restore()
{
    m_enabled =  m_state->getBoolean(DOMStorageAgentState::domStorageAgentEnabled);
}

void InspectorDOMStorageAgent::enable(ErrorString*)
{
    if (m_enabled)
        return;
    m_enabled = true;
    m_state->setBoolean(DOMStorageAgentState::domStorageAgentEnabled, m_enabled);

    DOMStorageResourcesMap::iterator resourcesEnd = m_resources.end();
    for (DOMStorageResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it)
        it->second->bind(m_frontend);
}

void InspectorDOMStorageAgent::disable(ErrorString*)
{
    if (!m_enabled)
        return;
    m_enabled = false;
    m_state->setBoolean(DOMStorageAgentState::domStorageAgentEnabled, m_enabled);
}

void InspectorDOMStorageAgent::getDOMStorageEntries(ErrorString*, const String& storageId, RefPtr<TypeBuilder::Array<TypeBuilder::Array<String> > >& entries)
{
    // FIXME: consider initializing this array after 2 checks below. The checks should return error messages in this case.
    entries = TypeBuilder::Array<TypeBuilder::Array<String> >::create();

    InspectorDOMStorageResource* storageResource = getDOMStorageResourceForId(storageId);
    if (!storageResource)
        return;
    Frame* frame = storageResource->frame();
    if (!frame)
        return;
        
    StorageArea* storageArea = storageResource->storageArea();
    for (unsigned i = 0; i < storageArea->length(frame); ++i) {
        String name(storageArea->key(i, frame));
        String value(storageArea->getItem(name, frame));
        RefPtr<TypeBuilder::Array<String> > entry = TypeBuilder::Array<String>::create();
        entry->addItem(name);
        entry->addItem(value);
        entries->addItem(entry);
    }
}

void InspectorDOMStorageAgent::setDOMStorageItem(ErrorString*, const String& storageId, const String& key, const String& value, bool* success)
{
    InspectorDOMStorageResource* storageResource = getDOMStorageResourceForId(storageId);
    if (storageResource) {
        ExceptionCode exception = 0;
        storageResource->storageArea()->setItem(key, value, exception, storageResource->frame());
        *success = !exception;
    } else
        *success = false;
}

void InspectorDOMStorageAgent::removeDOMStorageItem(ErrorString*, const String& storageId, const String& key, bool* success)
{
    InspectorDOMStorageResource* storageResource = getDOMStorageResourceForId(storageId);
    if (storageResource) {
        storageResource->storageArea()->removeItem(key, storageResource->frame());
        *success = true;
    } else
        *success = false;
}

String InspectorDOMStorageAgent::storageId(Storage* storage)
{
    ASSERT(storage);
    Frame* frame = storage->frame();
    ExceptionCode ec = 0;
    bool isLocalStorage = (frame->document()->domWindow()->localStorage(ec) == storage && !ec);
    return storageId(frame->document()->securityOrigin(), isLocalStorage);
}

String InspectorDOMStorageAgent::storageId(SecurityOrigin* securityOrigin, bool isLocalStorage)
{
    ASSERT(securityOrigin);
    DOMStorageResourcesMap::iterator domStorageEnd = m_resources.end();
    for (DOMStorageResourcesMap::iterator it = m_resources.begin(); it != domStorageEnd; ++it) {
        if (it->second->isSameOriginAndType(securityOrigin, isLocalStorage))
            return it->first;
    }
    return String();
}

InspectorDOMStorageResource* InspectorDOMStorageAgent::getDOMStorageResourceForId(const String& storageId)
{
    DOMStorageResourcesMap::iterator it = m_resources.find(storageId);
    if (it == m_resources.end())
        return 0;
    return it->second.get();
}

void InspectorDOMStorageAgent::didUseDOMStorage(StorageArea* storageArea, bool isLocalStorage, Frame* frame)
{
    DOMStorageResourcesMap::iterator domStorageEnd = m_resources.end();
    for (DOMStorageResourcesMap::iterator it = m_resources.begin(); it != domStorageEnd; ++it) {
        if (it->second->isSameOriginAndType(frame->document()->securityOrigin(), isLocalStorage))
            return;
    }

    RefPtr<InspectorDOMStorageResource> resource = InspectorDOMStorageResource::create(storageArea, isLocalStorage, frame);

    m_resources.set(resource->id(), resource);

    // Resources are only bound while visible.
    if (m_enabled)
        resource->bind(m_frontend);
}

void InspectorDOMStorageAgent::didDispatchDOMStorageEvent(const String&, const String&, const String&, StorageType storageType, SecurityOrigin* securityOrigin, Page*)
{
    if (!m_frontend || !m_enabled)
        return;

    String id = storageId(securityOrigin, storageType == LocalStorage);

    if (id.isEmpty())
        return;

    m_frontend->domstorage()->domStorageUpdated(id);
}

void InspectorDOMStorageAgent::clearResources()
{
    m_resources.clear();
}

size_t InspectorDOMStorageAgent::memoryBytesUsedByStorageCache() const
{
    size_t size = 0;
    for (DOMStorageResourcesMap::const_iterator it = m_resources.begin(); it != m_resources.end(); ++it)
        size += it->second->storageArea()->memoryBytesUsedByCache();
    return size;
}


} // namespace WebCore

#endif // ENABLE(INSPECTOR)
