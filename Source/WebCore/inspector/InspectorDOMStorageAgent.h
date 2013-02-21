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

#ifndef InspectorDOMStorageAgent_h
#define InspectorDOMStorageAgent_h

#include "InspectorBaseAgent.h"
#include "StorageArea.h"
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Frame;
class InspectorArray;
class InspectorDOMStorageResource;
class InspectorFrontend;
class InspectorState;
class InstrumentingAgents;
class Page;
class Storage;
class StorageArea;

typedef String ErrorString;

class InspectorDOMStorageAgent : public InspectorBaseAgent<InspectorDOMStorageAgent>, public InspectorBackendDispatcher::DOMStorageCommandHandler {
public:
    static PassOwnPtr<InspectorDOMStorageAgent> create(InstrumentingAgents* instrumentingAgents, InspectorCompositeState* state)
    {
        return adoptPtr(new InspectorDOMStorageAgent(instrumentingAgents, state));
    }
    ~InspectorDOMStorageAgent();

    virtual void setFrontend(InspectorFrontend*);
    virtual void clearFrontend();
    void restore();

    void clearResources();

    // Called from the front-end.
    virtual void enable(ErrorString*);
    virtual void disable(ErrorString*);
    virtual void getDOMStorageEntries(ErrorString*, const String& storageId, RefPtr<TypeBuilder::Array<TypeBuilder::Array<String> > >& entries);
    virtual void setDOMStorageItem(ErrorString*, const String& storageId, const String& key, const String& value, bool* success);
    virtual void removeDOMStorageItem(ErrorString*, const String& storageId, const String& key, bool* success);

    // Called from the injected script.
    String storageId(Storage*);
    String storageId(SecurityOrigin*, bool isLocalStorage);

    // Called from InspectorInstrumentation
    void didUseDOMStorage(StorageArea*, bool isLocalStorage, Frame*);
    void didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*, Page*);

    virtual void reportMemoryUsage(MemoryObjectInfo*) const OVERRIDE;

private:
    InspectorDOMStorageAgent(InstrumentingAgents*, InspectorCompositeState*);

    InspectorDOMStorageResource* getDOMStorageResourceForId(const String& storageId);

    typedef HashMap<String, RefPtr<InspectorDOMStorageResource> > DOMStorageResourcesMap;
    DOMStorageResourcesMap m_resources;
    InspectorFrontend* m_frontend;
    bool m_enabled;
};

} // namespace WebCore

#endif // !defined(InspectorDOMStorageAgent_h)
