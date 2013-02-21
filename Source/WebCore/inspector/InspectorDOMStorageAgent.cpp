/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
#include "ExceptionCodeDescription.h"
#include "Frame.h"
#include "InspectorFrontend.h"
#include "InspectorPageAgent.h"
#include "InspectorState.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "PageGroup.h"
#include "SecurityOrigin.h"
#include "Storage.h"
#include "StorageArea.h"
#include "StorageNamespace.h"
#include "VoidCallback.h"
#include "WebCoreMemoryInstrumentation.h"

#include <wtf/MemoryInstrumentationHashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace DOMStorageAgentState {
static const char domStorageAgentEnabled[] = "domStorageAgentEnabled";
};

static bool hadException(ExceptionCode ec, ErrorString* errorString)
{
    switch (ec) {
    case 0:
        return false;
    case SECURITY_ERR:
        *errorString = "Security error";
        return true;
    default:
        *errorString = "Unknown DOM storage error";
        return true;
    }
}

InspectorDOMStorageAgent::InspectorDOMStorageAgent(InstrumentingAgents* instrumentingAgents, InspectorPageAgent* pageAgent, InspectorCompositeState* state)
    : InspectorBaseAgent<InspectorDOMStorageAgent>("DOMStorage", instrumentingAgents, state)
    , m_pageAgent(pageAgent)
    , m_frontend(0)
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
    m_frontend = 0;
    disable(0);
}

bool InspectorDOMStorageAgent::isEnabled() const
{
    return m_state->getBoolean(DOMStorageAgentState::domStorageAgentEnabled);
}

void InspectorDOMStorageAgent::enable(ErrorString*)
{
    m_state->setBoolean(DOMStorageAgentState::domStorageAgentEnabled, true);
}

void InspectorDOMStorageAgent::disable(ErrorString*)
{
    m_state->setBoolean(DOMStorageAgentState::domStorageAgentEnabled, false);
}

void InspectorDOMStorageAgent::getDOMStorageItems(ErrorString* errorString, const RefPtr<InspectorObject>& storageId, RefPtr<TypeBuilder::Array<TypeBuilder::Array<String> > >& items)
{
    Frame* frame;
    RefPtr<StorageArea> storageArea = findStorageArea(errorString, storageId, frame);
    if (!storageArea)
        return;

    RefPtr<TypeBuilder::Array<TypeBuilder::Array<String> > > storageItems = TypeBuilder::Array<TypeBuilder::Array<String> >::create();

    ExceptionCode ec = 0;
    for (unsigned i = 0; i < storageArea->length(ec, frame); ++i) {
        String name(storageArea->key(i, ec, frame));
        if (hadException(ec, errorString))
            return;
        String value(storageArea->getItem(name, ec, frame));
        if (hadException(ec, errorString))
            return;
        RefPtr<TypeBuilder::Array<String> > entry = TypeBuilder::Array<String>::create();
        entry->addItem(name);
        entry->addItem(value);
        storageItems->addItem(entry);
    }
    items = storageItems.release();
}

static String toErrorString(const ExceptionCode& ec)
{
    if (ec) {
        ExceptionCodeDescription description(ec);
        return description.name;
    }
    return "";
}

void InspectorDOMStorageAgent::setDOMStorageItem(ErrorString* errorString, const RefPtr<InspectorObject>& storageId, const String& key, const String& value)
{
    Frame* frame;
    RefPtr<StorageArea> storageArea = findStorageArea(0, storageId, frame);
    if (!storageArea) {
        *errorString = "Storage not found";
        return;
    }

    ExceptionCode exception = 0;
    storageArea->setItem(key, value, exception, frame);
    *errorString = toErrorString(exception);
}

void InspectorDOMStorageAgent::removeDOMStorageItem(ErrorString* errorString, const RefPtr<InspectorObject>& storageId, const String& key)
{
    Frame* frame;
    RefPtr<StorageArea> storageArea = findStorageArea(0, storageId, frame);
    if (!storageArea) {
        *errorString = "Storage not found";
        return;
    }

    ExceptionCode exception = 0;
    storageArea->removeItem(key, exception, frame);
    *errorString = toErrorString(exception);
}

String InspectorDOMStorageAgent::storageId(Storage* storage)
{
    ASSERT(storage);
    Document* document = storage->frame()->document();
    ASSERT(document);
    DOMWindow* window = document->domWindow();
    ASSERT(window);
    RefPtr<SecurityOrigin> securityOrigin = document->securityOrigin();
    bool isLocalStorage = window->optionalLocalStorage() == storage;
    return storageId(securityOrigin.get(), isLocalStorage)->toJSONString();
}

PassRefPtr<TypeBuilder::DOMStorage::StorageId> InspectorDOMStorageAgent::storageId(SecurityOrigin* securityOrigin, bool isLocalStorage)
{
    return TypeBuilder::DOMStorage::StorageId::create()
        .setSecurityOrigin(securityOrigin->toRawString())
        .setIsLocalStorage(isLocalStorage).release();
}

void InspectorDOMStorageAgent::didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType storageType, SecurityOrigin* securityOrigin, Page*)
{
    if (!m_frontend || !isEnabled())
        return;

    RefPtr<TypeBuilder::DOMStorage::StorageId> id = storageId(securityOrigin, storageType == LocalStorage);

    if (key.isNull())
        m_frontend->domstorage()->domStorageItemsCleared(id);
    else if (newValue.isNull())
        m_frontend->domstorage()->domStorageItemRemoved(id, key);
    else if (oldValue.isNull())
        m_frontend->domstorage()->domStorageItemAdded(id, key, newValue);
    else
        m_frontend->domstorage()->domStorageItemUpdated(id, key, oldValue, newValue);
}

PassRefPtr<StorageArea> InspectorDOMStorageAgent::findStorageArea(ErrorString* errorString, const RefPtr<InspectorObject>& storageId, Frame*& targetFrame)
{
    String securityOrigin;
    bool isLocalStorage = false;
    bool success = storageId->getString("securityOrigin", &securityOrigin);
    if (success)
        success = storageId->getBoolean("isLocalStorage", &isLocalStorage);
    if (!success) {
        if (errorString)
            *errorString = "Invalid storageId format";
        return 0;
    }

    Frame* frame = m_pageAgent->findFrameWithSecurityOrigin(securityOrigin);
    if (!frame) {
        if (errorString)
            *errorString = "Frame not found for the given security origin";
        return 0;
    }
    targetFrame = frame;

    Page* page = m_pageAgent->page();
    if (isLocalStorage)
        return page->group().localStorage()->storageArea(frame->document()->securityOrigin());
    return page->sessionStorage()->storageArea(frame->document()->securityOrigin());
}

void InspectorDOMStorageAgent::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::InspectorDOMStorageAgent);
    InspectorBaseAgent<InspectorDOMStorageAgent>::reportMemoryUsage(memoryObjectInfo);
    info.addWeakPointer(m_frontend);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
