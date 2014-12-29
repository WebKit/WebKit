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

#ifndef MaskImageOperation_h
#define MaskImageOperation_h

#include "CachedResourceLoader.h"
#include "CachedSVGDocumentClient.h"
#include "CachedSVGDocumentReference.h"
#include "Color.h"
#include "Image.h"
#include "LayoutSize.h"
#include "Length.h"
#include "StyleImage.h"
#include <wtf/HashCountedSet.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
    
class BackgroundImageGeometry;
class CachedImageClient;
class RenderSVGResourceMasker;
class WebKitCSSResourceValue;

class MaskImageOperation final : public RefCounted<MaskImageOperation>, public CachedSVGDocumentClient {
public:
    static PassRefPtr<MaskImageOperation> create(PassRefPtr<WebKitCSSResourceValue> cssValue, const String& url, const String& fragment, bool isExternalDocument, PassRefPtr<CachedResourceLoader>);
    static PassRefPtr<MaskImageOperation> create(PassRefPtr<StyleImage> generatedImage);
    static PassRefPtr<MaskImageOperation> create();

    PassRefPtr<CSSValue> cssValue();

    virtual ~MaskImageOperation();
    
    bool operator==(const MaskImageOperation&) const;
    inline bool operator!=(const MaskImageOperation& other) const { return !operator==(other); }

    const String& url() const { return m_url; }
    const String& fragment() const { return m_fragment; }
    bool isExternalDocument() const { return m_isExternalDocument; }
    StyleImage* image() const { return m_styleImage.get(); }
    void setImage(PassRefPtr<StyleImage> image) { m_styleImage = image; }
    void setRenderLayerImageClient(CachedImageClient*);
    void addRendererImageClient(RenderElement*);
    void removeRendererImageClient(RenderElement*);
    bool isMaskLoaded() const;
    bool drawMask(RenderElement& renderer, BackgroundImageGeometry&, GraphicsContext*, CompositeOperator);
    bool isCSSValueNone() const;

    CachedSVGDocumentReference* cachedSVGDocumentReference() const { return m_cachedSVGDocumentReference.get(); }
    CachedSVGDocumentReference* ensureCachedSVGDocumentReference();
    
    virtual void notifyFinished(CachedResource*) override;

private:
    MaskImageOperation(PassRefPtr<WebKitCSSResourceValue> cssValue, const String& url, const String& fragment, bool isExternalDocument, PassRefPtr<CachedResourceLoader>);
    MaskImageOperation(PassRefPtr<StyleImage> generatedImage);
    MaskImageOperation();

    RenderSVGResourceMasker* getSVGMasker(RenderElement& renderer);

    String m_url;
    String m_fragment;
    bool m_isExternalDocument;
    std::unique_ptr<CachedSVGDocumentReference> m_cachedSVGDocumentReference;
    RefPtr<StyleImage> m_styleImage;
    CachedImageClient* m_renderLayerImageClient;
    HashCountedSet<RenderElement*> m_rendererImageClients;
    RefPtr<WebKitCSSResourceValue> m_cssMaskImageValue;
    RefPtr<CachedResourceLoader> m_cachedResourceLoader;
};

} // namespace WebCore

#endif // MaskImageOperation_h
