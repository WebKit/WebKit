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

#include "CCLayerImpl.h"
#include "CCScrollbarAnimationController.h"
#include "CCScrollbarLayerImpl.h"
#include "LayerChromium.h"
#include "ScrollbarLayerChromium.h"
#include <wtf/RefPtr.h>

namespace WebCore {

PassOwnPtr<CCLayerImpl> TreeSynchronizer::synchronizeTrees(LayerChromium* layerChromiumRoot, PassOwnPtr<CCLayerImpl> oldCCLayerImplRoot, CCLayerTreeHostImpl* hostImpl)
{
    OwnPtrCCLayerImplMap oldLayers;
    RawPtrCCLayerImplMap newLayers;

    collectExistingCCLayerImplRecursive(oldLayers, oldCCLayerImplRoot);

    OwnPtr<CCLayerImpl> newTree = synchronizeTreeRecursive(newLayers, oldLayers, layerChromiumRoot, hostImpl);

    updateScrollbarLayerPointersRecursive(newLayers, layerChromiumRoot);

    return newTree.release();
}

void TreeSynchronizer::collectExistingCCLayerImplRecursive(OwnPtrCCLayerImplMap& oldLayers, PassOwnPtr<CCLayerImpl> popCCLayerImpl)
{
    OwnPtr<CCLayerImpl> ccLayerImpl = popCCLayerImpl;

    if (!ccLayerImpl)
        return;

    Vector<OwnPtr<CCLayerImpl> >& children = ccLayerImpl->m_children;
    for (size_t i = 0; i < children.size(); ++i)
        collectExistingCCLayerImplRecursive(oldLayers, children[i].release());

    collectExistingCCLayerImplRecursive(oldLayers, ccLayerImpl->m_maskLayer.release());
    collectExistingCCLayerImplRecursive(oldLayers, ccLayerImpl->m_replicaLayer.release());

    int id = ccLayerImpl->id();
    oldLayers.set(id, ccLayerImpl.release());
}

PassOwnPtr<CCLayerImpl> TreeSynchronizer::reuseOrCreateCCLayerImpl(RawPtrCCLayerImplMap& newLayers, OwnPtrCCLayerImplMap& oldLayers, LayerChromium* layer)
{
    OwnPtr<CCLayerImpl> ccLayerImpl = oldLayers.take(layer->id());

    if (!ccLayerImpl)
        ccLayerImpl = layer->createCCLayerImpl();

    newLayers.set(layer->id(), ccLayerImpl.get());
    return ccLayerImpl.release();
}

PassOwnPtr<CCLayerImpl> TreeSynchronizer::synchronizeTreeRecursive(RawPtrCCLayerImplMap& newLayers, OwnPtrCCLayerImplMap& oldLayers, LayerChromium* layer, CCLayerTreeHostImpl* hostImpl)
{
    if (!layer)
        return nullptr;

    OwnPtr<CCLayerImpl> ccLayerImpl = reuseOrCreateCCLayerImpl(newLayers, oldLayers, layer);

    ccLayerImpl->clearChildList();
    const Vector<RefPtr<LayerChromium> >& children = layer->children();
    for (size_t i = 0; i < children.size(); ++i)
        ccLayerImpl->addChild(synchronizeTreeRecursive(newLayers, oldLayers, children[i].get(), hostImpl));

    ccLayerImpl->setMaskLayer(synchronizeTreeRecursive(newLayers, oldLayers, layer->maskLayer(), hostImpl));
    ccLayerImpl->setReplicaLayer(synchronizeTreeRecursive(newLayers, oldLayers, layer->replicaLayer(), hostImpl));

    layer->pushPropertiesTo(ccLayerImpl.get());
    ccLayerImpl->setLayerTreeHostImpl(hostImpl);

    // Remove all dangling pointers. The pointers will be setup later in updateScrollbarLayerPointersRecursive phase
    if (CCScrollbarAnimationController* scrollbarController = ccLayerImpl->scrollbarAnimationController()) {
        scrollbarController->setHorizontalScrollbarLayer(0);
        scrollbarController->setVerticalScrollbarLayer(0);
    }

    return ccLayerImpl.release();
}

void TreeSynchronizer::updateScrollbarLayerPointersRecursive(const RawPtrCCLayerImplMap& newLayers, LayerChromium* layer)
{
    if (!layer)
        return;

    const Vector<RefPtr<LayerChromium> >& children = layer->children();
    for (size_t i = 0; i < children.size(); ++i)
        updateScrollbarLayerPointersRecursive(newLayers, children[i].get());

    ScrollbarLayerChromium* scrollbarLayer = layer->toScrollbarLayerChromium();
    if (!scrollbarLayer)
        return;

    CCScrollbarLayerImpl* ccScrollbarLayerImpl = static_cast<CCScrollbarLayerImpl*>(newLayers.get(scrollbarLayer->id()));
    ASSERT(ccScrollbarLayerImpl);
    CCLayerImpl* ccScrollLayerImpl = newLayers.get(scrollbarLayer->scrollLayerId());
    ASSERT(ccScrollLayerImpl);

    if (ccScrollbarLayerImpl->orientation() == WebKit::WebScrollbar::Horizontal)
        ccScrollLayerImpl->setHorizontalScrollbarLayer(ccScrollbarLayerImpl);
    else
        ccScrollLayerImpl->setVerticalScrollbarLayer(ccScrollbarLayerImpl);
}

} // namespace WebCore
