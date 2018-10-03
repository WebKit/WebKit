/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WI.LayerTreeManager = class LayerTreeManager extends WI.Object
{
    constructor()
    {
        super();

        if (window.LayerTreeAgent)
            LayerTreeAgent.enable();
    }

    // Public

    get supported()
    {
        return !!window.LayerTreeAgent;
    }

    layerTreeMutations(previousLayers, newLayers)
    {
        console.assert(this.supported);

        if (isEmptyObject(previousLayers))
            return {preserved: [], additions: newLayers, removals: []};

        let previousLayerIds = new Set;
        let newLayerIds = new Set;

        let preserved = [];
        let additions = [];

        for (let layer of previousLayers)
            previousLayerIds.add(layer.layerId);

        for (let layer of newLayers) {
            newLayerIds.add(layer.layerId);

            if (previousLayerIds.has(layer.layerId))
                preserved.push(layer);
            else
                additions.push(layer);
        }

        let removals = previousLayers.filter((layer) => !newLayerIds.has(layer.layerId));

        return {preserved, additions, removals};
    }

    layersForNode(node, callback)
    {
        console.assert(this.supported);

        LayerTreeAgent.layersForNode(node.id, (error, layers) => {
            callback(error ? [] : layers.map(WI.Layer.fromPayload));
        });
    }

    reasonsForCompositingLayer(layer, callback)
    {
        console.assert(this.supported);

        LayerTreeAgent.reasonsForCompositingLayer(layer.layerId, function(error, reasons) {
            callback(error ? 0 : reasons);
        });
    }

    layerTreeDidChange()
    {
        this.dispatchEventToListeners(WI.LayerTreeManager.Event.LayerTreeDidChange);
    }
};

WI.LayerTreeManager.Event = {
    LayerTreeDidChange: "layer-tree-did-change"
};
