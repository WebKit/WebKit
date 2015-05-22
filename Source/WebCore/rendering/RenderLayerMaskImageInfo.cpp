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
#include "SVGMaskElement.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

class RenderLayer::MaskImageInfo::SVGDocumentClient : public CachedSVGDocumentClient {
public:
    SVGDocumentClient(RenderLayer& layer)
        : m_layer(layer)
    {
    }

private:
    virtual void notifyFinished(CachedResource*) override
    {
        m_layer.renderer().repaint();
    }

    RenderLayer& m_layer;
};

class RenderLayer::MaskImageInfo::ImageClient : public CachedImageClient {
public:
    ImageClient(RenderLayer& layer)
        : m_layer(layer)
    {
    }

private:
    virtual void imageChanged(CachedImage*, const IntRect*) override
    {
        m_layer.renderer().repaint();
    }

    RenderLayer& m_layer;
};

HashMap<const RenderLayer*, std::unique_ptr<RenderLayer::MaskImageInfo>>& RenderLayer::MaskImageInfo::layerToMaskMap()
{
    static NeverDestroyed<HashMap<const RenderLayer*, std::unique_ptr<MaskImageInfo>>> layerToMaskMap;
    return layerToMaskMap;
}

RenderLayer::MaskImageInfo* RenderLayer::MaskImageInfo::getIfExists(const RenderLayer& layer)
{
    ASSERT(layer.m_hasMaskImageInfo == layerToMaskMap().contains(&layer));
    return layer.m_hasMaskImageInfo ? layerToMaskMap().get(&layer) : nullptr;
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
    , m_svgDocumentClient(std::make_unique<SVGDocumentClient>(layer))
    , m_imageClient(std::make_unique<ImageClient>(layer))
{
}

RenderLayer::MaskImageInfo::~MaskImageInfo()
{
    removeMaskImageClients();
}

void RenderLayer::MaskImageInfo::updateMaskImageClients()
{
    removeMaskImageClients();
    
    for (auto* maskLayer = m_layer.renderer().style().maskLayers(); maskLayer; maskLayer = maskLayer->next()) {
        RefPtr<MaskImageOperation> maskImage = maskLayer->maskImage();
        maskImage->setRenderLayerImageClient(m_imageClient.get());
        m_maskImageOperations.append(maskImage);

        CachedSVGDocumentReference* documentReference = maskImage->cachedSVGDocumentReference();
        CachedSVGDocument* cachedSVGDocument = documentReference ? documentReference->document() : nullptr;
        
        if (cachedSVGDocument) {
            // Reference is external; wait for notifyFinished and then repaint.
            cachedSVGDocument->addClient(m_svgDocumentClient.get());
            m_externalSVGReferences.append(cachedSVGDocument);
        } else {
            // Reference is internal; add layer as a client so we can trigger mask repaint on SVG attribute change.
            Element* masker = m_layer.renderer().document().getElementById(maskImage->fragment());
            if (is<SVGMaskElement>(masker)) {
                downcast<SVGMaskElement>(*masker).addClientRenderLayer(&m_layer);
                m_internalSVGReferences.append(masker);
            }
        }
    }
}

void RenderLayer::MaskImageInfo::removeMaskImageClients()
{
    for (auto& maskImage : m_maskImageOperations)
        maskImage->setRenderLayerImageClient(nullptr);
    m_maskImageOperations.clear();

    for (auto& externalSVGReference : m_externalSVGReferences)
        externalSVGReference->removeClient(m_svgDocumentClient.get());
    m_externalSVGReferences.clear();
    
    for (auto& internalSVGReference : m_internalSVGReferences)
        downcast<SVGMaskElement>(internalSVGReference.get())->removeClientRenderLayer(&m_layer);
    m_internalSVGReferences.clear();
}

} // namespace WebCore
