/*
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "RenderLayerMaskImageInfo.h"

#include "CachedSVGDocument.h"
#include "CachedSVGDocumentReference.h"
#include "RenderSVGResourceContainer.h"
#include "SVGElement.h"
#include "SVGMaskElement.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

HashMap<const RenderLayer*, std::unique_ptr<RenderLayer::MaskImageInfo>>& RenderLayer::MaskImageInfo::layerToMaskMap()
{
    static NeverDestroyed<HashMap<const RenderLayer*, std::unique_ptr<MaskImageInfo>>> layerToMaskMap;
    return layerToMaskMap;
}

RenderLayer::MaskImageInfo* RenderLayer::MaskImageInfo::getIfExists(const RenderLayer& layer)
{
    ASSERT(layer.m_hasMaskImageInfo == layerToMaskMap().contains(&layer));
    return layer.m_hasMaskImageInfo ? layerToMaskMap().get(&layer) : 0;
}

RenderLayer::MaskImageInfo& RenderLayer::MaskImageInfo::get(RenderLayer& layer)
{
    ASSERT(layer.m_hasMaskImageInfo == layerToMaskMap().contains(&layer));

    auto& info = layerToMaskMap().add(&layer, nullptr).iterator->value;
    if (!info) {
        info = std::make_unique<MaskImageInfo>(layer);
        layer.m_hasMaskImageInfo = true;
    }
    return *info;
}

void RenderLayer::MaskImageInfo::remove(RenderLayer& layer)
{
    ASSERT(layer.m_hasMaskImageInfo == layerToMaskMap().contains(&layer));

    if (layerToMaskMap().remove(&layer))
        layer.m_hasMaskImageInfo = false;
}

RenderLayer::MaskImageInfo::MaskImageInfo(RenderLayer& layer)
    : m_layer(layer)
{
    m_svgDocumentClient = std::make_unique<MaskSVGDocumentClient>(this);
    m_imageClient = std::make_unique<MaskImageClient>(this);
}

RenderLayer::MaskImageInfo::~MaskImageInfo()
{
    removeMaskImageClients();
}

void RenderLayer::MaskImageInfo::notifyFinished(CachedResource*)
{
    m_layer.renderer().repaint();
}

void RenderLayer::MaskImageInfo::imageChanged(CachedImage*, const IntRect*)
{
    m_layer.renderer().repaint();
}

void RenderLayer::MaskImageInfo::updateMaskImageClients()
{
    removeMaskImageClients();
    
    const FillLayer* maskLayer = m_layer.renderer().style().maskLayers();
    while (maskLayer) {
        const RefPtr<MaskImageOperation> maskImage = maskLayer->maskImage();
        maskImage->setRenderLayerImageClient(m_imageClient.get());
        CachedSVGDocumentReference* documentReference = maskImage->cachedSVGDocumentReference();
        CachedSVGDocument* cachedSVGDocument = documentReference ? documentReference->document() : nullptr;
        
        if (cachedSVGDocument) {
            // Reference is external; wait for notifyFinished().
            cachedSVGDocument->addClient(m_svgDocumentClient.get());
            m_externalSVGReferences.append(cachedSVGDocument);
        } else {
            // Reference is internal; add layer as a client so we can trigger
            // mask repaint on SVG attribute change.
            Element* masker = m_layer.renderer().document().getElementById(maskImage->fragment());
            if (masker && is<SVGMaskElement>(masker)) {
                downcast<SVGMaskElement>(masker)->addClientRenderLayer(&m_layer);
                m_internalSVGReferences.append(masker);
            }
        }
        
        maskLayer = maskLayer->next();
    }
}

void RenderLayer::MaskImageInfo::removeMaskImageClients()
{
    const FillLayer* maskLayer = m_layer.renderer().style().maskLayers();
    while (maskLayer) {
        if (maskLayer->maskImage())
            maskLayer->maskImage()->setRenderLayerImageClient(nullptr);

        maskLayer = maskLayer->next();
    }
    
    for (auto externalSVGReference : m_externalSVGReferences)
        externalSVGReference->removeClient(m_svgDocumentClient.get());
    m_externalSVGReferences.clear();
    
    for (auto internalSVGReference : m_internalSVGReferences)
        downcast<SVGMaskElement>(internalSVGReference.get())->removeClientRenderLayer(&m_layer);
    m_internalSVGReferences.clear();
}

} // namespace WebCore
