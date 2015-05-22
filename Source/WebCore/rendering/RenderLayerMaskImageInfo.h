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

#include "CachedResourceHandle.h"
#include "RenderLayer.h"

namespace WebCore {

class Element;

class RenderLayer::MaskImageInfo final {
public:
    static MaskImageInfo& get(RenderLayer&);
    static MaskImageInfo* getIfExists(const RenderLayer&);
    static void remove(RenderLayer&);

    explicit MaskImageInfo(RenderLayer&);
    ~MaskImageInfo();

    void updateMaskImageClients();
    void removeMaskImageClients();

private:
    static HashMap<const RenderLayer*, std::unique_ptr<MaskImageInfo>>& layerToMaskMap();

    class ImageClient;
    class SVGDocumentClient;

    RenderLayer& m_layer;

    Vector<RefPtr<Element>> m_internalSVGReferences;
    Vector<CachedResourceHandle<CachedSVGDocument>> m_externalSVGReferences;
    std::unique_ptr<SVGDocumentClient> m_svgDocumentClient;
    std::unique_ptr<ImageClient> m_imageClient;
    Vector<RefPtr<MaskImageOperation>> m_maskImageOperations;
};

} // namespace WebCore

#endif // RenderLayerMaskImageInfo_h
