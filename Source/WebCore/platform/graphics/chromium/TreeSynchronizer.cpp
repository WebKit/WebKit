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

PassOwnPtr<CCLayerImpl> TreeSynchronizer::synchronizeTrees(LayerChromium* layerChromiumRoot, PassOwnPtr<CCLayerImpl> oldCCLayerImplRoot)
{
    CCLayerImplMap map;
    collectExistingCCLayerImplRecursive(map, oldCCLayerImplRoot);

    return synchronizeTreeRecursive(map, layerChromiumRoot);
}

void TreeSynchronizer::collectExistingCCLayerImplRecursive(CCLayerImplMap& map, PassOwnPtr<CCLayerImpl> popCCLayerImpl)
{
    OwnPtr<CCLayerImpl> ccLayerImpl = popCCLayerImpl;

    if (!ccLayerImpl)
        return;

    Vector<OwnPtr<CCLayerImpl> >& children = ccLayerImpl->m_children;
    for (size_t i = 0; i < children.size(); ++i)
        collectExistingCCLayerImplRecursive(map, children[i].release());

    collectExistingCCLayerImplRecursive(map, ccLayerImpl->m_maskLayer.release());
    collectExistingCCLayerImplRecursive(map, ccLayerImpl->m_replicaLayer.release());

    int id = ccLayerImpl->id();
    map.set(id, ccLayerImpl.release());
}

PassOwnPtr<CCLayerImpl> TreeSynchronizer::reuseOrCreateCCLayerImpl(CCLayerImplMap& map, LayerChromium* layer)
{
    OwnPtr<CCLayerImpl> ccLayerImpl = map.take(layer->id());

    if (!ccLayerImpl)
        return layer->createCCLayerImpl();

    return ccLayerImpl.release();
}

PassOwnPtr<CCLayerImpl> TreeSynchronizer::synchronizeTreeRecursive(CCLayerImplMap& map, LayerChromium* layer)
{
    if (!layer)
        return nullptr;

    OwnPtr<CCLayerImpl> ccLayerImpl = reuseOrCreateCCLayerImpl(map, layer);

    ccLayerImpl->clearChildList();
    const Vector<RefPtr<LayerChromium> >& children = layer->children();
    for (size_t i = 0; i < children.size(); ++i)
        ccLayerImpl->addChild(synchronizeTreeRecursive(map, children[i].get()));

    ccLayerImpl->setMaskLayer(synchronizeTreeRecursive(map, layer->maskLayer()));
    ccLayerImpl->setReplicaLayer(synchronizeTreeRecursive(map, layer->replicaLayer()));

    layer->pushPropertiesTo(ccLayerImpl.get());
    return ccLayerImpl.release();
}

} // namespace WebCore
