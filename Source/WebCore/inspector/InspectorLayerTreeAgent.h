/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef InspectorLayerTreeAgent_h
#define InspectorLayerTreeAgent_h

#if ENABLE(INSPECTOR)

#include "InspectorBaseAgent.h"
#include "InspectorFrontend.h"
#include "InspectorTypeBuilder.h"
#include "RenderLayer.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class InspectorState;
class InstrumentingAgents;
class Page;

typedef String ErrorString;

class InspectorLayerTreeAgent : public InspectorBaseAgent<InspectorLayerTreeAgent>, public InspectorBackendDispatcher::LayerTreeCommandHandler {
public:
    static PassOwnPtr<InspectorLayerTreeAgent> create(InstrumentingAgents* instrumentingAgents, InspectorCompositeState* state, Page* page)
    {
        return adoptPtr(new InspectorLayerTreeAgent(instrumentingAgents, state, page));
    }
    ~InspectorLayerTreeAgent();

    virtual void setFrontend(InspectorFrontend*);
    virtual void clearFrontend();
    virtual void restore();
    void reset();

    void layerTreeDidChange();
    void renderLayerDestroyed(const RenderLayer*);

    // Called from the front-end.
    virtual void enable(ErrorString*);
    virtual void disable(ErrorString*);
    virtual void getLayerTree(ErrorString*, RefPtr<TypeBuilder::LayerTree::Layer>&);
    virtual void nodeIdForLayerId(ErrorString*, const String& layerId, int* resultNodeId);

private:
    InspectorLayerTreeAgent(InstrumentingAgents*, InspectorCompositeState*, Page*);

    // RenderLayer-related methods.
    String bind(const RenderLayer*);
    void unbind(const RenderLayer*);

    PassRefPtr<TypeBuilder::LayerTree::Layer> buildObjectForRootLayer();   
    PassRefPtr<TypeBuilder::LayerTree::Layer> buildObjectForLayer(RenderLayer*);
    PassRefPtr<TypeBuilder::LayerTree::IntRect> buildObjectForIntRect(const IntRect&);
        
    Page* m_inspectedPage;
    InspectorFrontend::LayerTree* m_frontend;

    HashMap<const RenderLayer*, String> m_documentLayerToIdMap;
    HashMap<String, const RenderLayer*> m_idToLayer;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorLayerTreeAgent_h)
