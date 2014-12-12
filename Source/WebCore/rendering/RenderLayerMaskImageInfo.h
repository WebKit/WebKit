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

#ifndef RenderLayerMaskImageInfo_h
#define RenderLayerMaskImageInfo_h

#include "CachedImageClient.h"
#include "CachedResourceHandle.h"
#include "CachedSVGDocumentClient.h"
#include "MaskImageOperation.h"
#include "RenderLayer.h"
#include <memory>

namespace WebCore {

class Element;

class RenderLayer::MaskImageInfo final {
private:
    class MaskResourceClient {
    public:
        MaskResourceClient(RenderLayer::MaskImageInfo* maskImageInfo)
            : m_maskImageInfo(maskImageInfo)
        {
            ASSERT(m_maskImageInfo);
        }
    protected:
        RenderLayer::MaskImageInfo* m_maskImageInfo;
    };
    
    class MaskSVGDocumentClient : public MaskResourceClient, public CachedSVGDocumentClient {
    public:
        MaskSVGDocumentClient(RenderLayer::MaskImageInfo* maskImageInfo)
            : MaskResourceClient(maskImageInfo)
        {
        }
        
        virtual void notifyFinished(CachedResource* resource) override { m_maskImageInfo->notifyFinished(resource); }
    };
    
    class MaskImageClient : public MaskResourceClient, public CachedImageClient {
    public:
        MaskImageClient(RenderLayer::MaskImageInfo* maskImageInfo)
            : MaskResourceClient(maskImageInfo)
        {
        }
        
        virtual void imageChanged(CachedImage* image, const IntRect* rect = nullptr) override { m_maskImageInfo->imageChanged(image, rect); }
    };

public:
    static MaskImageInfo& get(RenderLayer&);
    static MaskImageInfo* getIfExists(const RenderLayer&);
    static void remove(RenderLayer&);

    explicit MaskImageInfo(RenderLayer&);
    ~MaskImageInfo();

    void updateMaskImageClients();
    void removeMaskImageClients(const RenderStyle& oldStyle);

private:
    friend void WTF::deleteOwnedPtr<MaskImageInfo>(MaskImageInfo*);

    void notifyFinished(CachedResource*);
    void imageChanged(CachedImage*, const IntRect*);

    static HashMap<const RenderLayer*, std::unique_ptr<MaskImageInfo>>& layerToMaskMap();

    RenderLayer& m_layer;

    Vector<RefPtr<Element>> m_internalSVGReferences;
    Vector<CachedResourceHandle<CachedSVGDocument>> m_externalSVGReferences;
    std::unique_ptr<MaskSVGDocumentClient> m_svgDocumentClient;
    std::unique_ptr<MaskImageClient> m_imageClient;
};

} // namespace WebCore

#endif // RenderLayerMaskImageInfo_h
