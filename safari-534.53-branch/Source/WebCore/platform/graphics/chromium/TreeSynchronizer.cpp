/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "TreeSynchronizer.h"

#include "LayerChromium.h"
#include "cc/CCLayerImpl.h"
#include <wtf/RefPtr.h>

namespace WebCore {

void TreeSynchronizer::addCCLayerImplsToMapRecursive(CCLayerImplMap& map, CCLayerImpl* ccLayerImpl)
{
    map.set(ccLayerImpl->id(), ccLayerImpl);

    const Vector<RefPtr<CCLayerImpl> >& children = ccLayerImpl->children();
    for (size_t i = 0; i < children.size(); ++i)
        addCCLayerImplsToMapRecursive(map, children[i].get());

    if (CCLayerImpl* maskLayer = ccLayerImpl->maskLayer())
        addCCLayerImplsToMapRecursive(map, maskLayer);

    if (CCLayerImpl* replicaLayer = ccLayerImpl->replicaLayer())
        addCCLayerImplsToMapRecursive(map, replicaLayer);
}

PassRefPtr<CCLayerImpl> TreeSynchronizer::synchronizeTreeRecursive(LayerChromium* layer, CCLayerImplMap& map)
{
    RefPtr<CCLayerImpl> ccLayerImpl;
    CCLayerImplMap::iterator it = map.find(layer->id());
    if (it != map.end()) {
        ccLayerImpl = it->second; // We already have an entry for this, we just need to reparent it.
        ccLayerImpl->clearChildList();
    } else {
        ccLayerImpl = layer->createCCLayerImpl();
        map.set(layer->id(), ccLayerImpl); // Add it to the map so other layers referencing this one can pick it up.
    }

    const Vector<RefPtr<LayerChromium> >& children = layer->children();
    for (size_t i = 0; i < children.size(); ++i)
        ccLayerImpl->addChild(synchronizeTreeRecursive(children[i].get(), map));

    if (LayerChromium* maskLayer = layer->maskLayer())
        ccLayerImpl->setMaskLayer(synchronizeTreeRecursive(maskLayer, map));

    if (LayerChromium* replicaLayer = layer->replicaLayer())
        ccLayerImpl->setReplicaLayer(synchronizeTreeRecursive(replicaLayer, map));

    layer->setCCLayerImpl(ccLayerImpl.get());

    layer->pushPropertiesTo(ccLayerImpl.get());
    return ccLayerImpl.release();
}

PassRefPtr<CCLayerImpl> TreeSynchronizer::synchronizeTrees(LayerChromium* layerChromiumRoot, PassRefPtr<CCLayerImpl> prpOldCCLayerImplRoot)
{
    RefPtr<CCLayerImpl> oldCCLayerImplRoot = prpOldCCLayerImplRoot;
    // Build a map from layer IDs to CCLayerImpls so we can reuse layers from the old tree.
    CCLayerImplMap map;
    if (oldCCLayerImplRoot)
        addCCLayerImplsToMapRecursive(map, oldCCLayerImplRoot.get());

    // Recursively build the new layer tree.
    RefPtr<CCLayerImpl> newCCLayerImplRoot = synchronizeTreeRecursive(layerChromiumRoot, map);

    return newCCLayerImplRoot.release();
}

} // namespace WebCore


