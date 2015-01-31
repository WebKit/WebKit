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
#include "MaskImageOperation.h"

#include "CachedImage.h"
#include "CachedSVGDocument.h"
#include "RenderBoxModelObject.h"
#include "RenderSVGResourceMasker.h"
#include "SVGDocument.h"
#include "SVGMaskElement.h"
#include "SVGSVGElement.h"
#include "StyleCachedImage.h"
#include "SubresourceLoader.h"
#include "WebKitCSSResourceValue.h"

namespace WebCore {

PassRefPtr<MaskImageOperation> MaskImageOperation::create(PassRefPtr<WebKitCSSResourceValue> cssValue, const String& url, const String& fragment, bool isExternalDocument, PassRefPtr<CachedResourceLoader> cachedResourceLoader)
{
    return adoptRef(new MaskImageOperation(cssValue, url, fragment, isExternalDocument, cachedResourceLoader));
}

PassRefPtr<MaskImageOperation> MaskImageOperation::create(PassRefPtr<StyleImage> generatedImage)
{
    return adoptRef(new MaskImageOperation(generatedImage));
}

PassRefPtr<MaskImageOperation> MaskImageOperation::create()
{
    return adoptRef(new MaskImageOperation());
}

MaskImageOperation::MaskImageOperation(PassRefPtr<WebKitCSSResourceValue> cssValue, const String& url, const String& fragment, bool isExternalDocument, PassRefPtr<CachedResourceLoader> cachedResourceLoader)
    : m_url(url)
    , m_fragment(fragment)
    , m_isExternalDocument(isExternalDocument)
    , m_renderLayerImageClient(nullptr)
    , m_cssMaskImageValue(cssValue)
    , m_cachedResourceLoader(cachedResourceLoader)
{
    ASSERT(m_cssMaskImageValue.get());
}

MaskImageOperation::MaskImageOperation(PassRefPtr<StyleImage> generatedImage)
    : m_isExternalDocument(false)
    , m_styleImage(generatedImage)
    , m_renderLayerImageClient(nullptr)
{
}

MaskImageOperation::MaskImageOperation()
    : m_isExternalDocument(false)
    , m_renderLayerImageClient(nullptr)
{
    m_cssMaskImageValue = WebKitCSSResourceValue::create(CSSPrimitiveValue::createIdentifier(CSSValueNone));
}

MaskImageOperation::~MaskImageOperation()
{
    setRenderLayerImageClient(nullptr);
}

bool MaskImageOperation::operator==(const MaskImageOperation& other) const
{
    if (m_url.length())
        return (m_url == other.m_url && m_fragment == other.m_fragment && m_isExternalDocument == other.m_isExternalDocument);

    return m_styleImage.get() == other.m_styleImage.get();
}

bool MaskImageOperation::isCSSValueNone() const
{
    if (image())
        return false;

    ASSERT(m_cssMaskImageValue.get());
    return m_cssMaskImageValue->isCSSValueNone();
}

PassRefPtr<CSSValue> MaskImageOperation::cssValue()
{
    if (image())
        return image()->cssValue();
    
    if (isCSSValueNone())
        return m_cssMaskImageValue->innerValue();

    ASSERT(m_cssMaskImageValue.get());
    return m_cssMaskImageValue.get();
}

bool MaskImageOperation::isMaskLoaded() const
{
    if (!m_isExternalDocument)
        return true;

    if (image())
        return (image()->cachedImage() && image()->cachedImage()->image());
    
    if (m_cachedSVGDocumentReference.get()) {
        if (CachedSVGDocument* cachedSVGDocument = m_cachedSVGDocumentReference->document())
            return (cachedSVGDocument->document() != nullptr);
    }
    
    return false;
}

void MaskImageOperation::setRenderLayerImageClient(CachedImageClient* client)
{
    if (m_renderLayerImageClient == client)
        return;

    if (CachedImage* cachedImage = (image() ? image()->cachedImage() : nullptr)) {
        if (m_renderLayerImageClient)
            cachedImage->removeClient(m_renderLayerImageClient);
        
        if (client)
            cachedImage->addClient(client);
    }

    m_renderLayerImageClient = client;
}

void MaskImageOperation::addRendererImageClient(RenderElement* client)
{
    ASSERT(client);

    if (m_styleImage.get())
        m_styleImage->addClient(client);
    
    m_rendererImageClients.add(client);
}

void MaskImageOperation::removeRendererImageClient(RenderElement* client)
{
    ASSERT(client && m_rendererImageClients.contains(client));
    
    if (m_styleImage.get())
        m_styleImage->removeClient(client);
    
    m_rendererImageClients.remove(client);
}

CachedSVGDocumentReference* MaskImageOperation::ensureCachedSVGDocumentReference()
{
    // If we ended up loading the data into an Image, then the SVG document was not valid.
    if (image())
        return nullptr;

    if (!m_cachedSVGDocumentReference.get())
        m_cachedSVGDocumentReference = std::make_unique<CachedSVGDocumentReference>(m_url, this, false);
    return m_cachedSVGDocumentReference.get();
}

void MaskImageOperation::notifyFinished(CachedResource* resource)
{
    // The only one notifying us should be the SVG document we hold.
    CachedSVGDocument* cachedSVGDocument = ensureCachedSVGDocumentReference()->document();
    if ((CachedResource*)cachedSVGDocument != resource || !resource) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    // Check if we find a valid masking element in this SVG document.
    SVGDocument* svgDocument = cachedSVGDocument->document();
    bool validMaskFound = false;
    if (svgDocument && svgDocument->rootElement()) {
        // Are we looking for a specific element in the SVG document?
        if (fragment().length()) {
            if (Element* maskingElement = svgDocument->rootElement()->getElementById(fragment())) {
                if (is<SVGMaskElement>(maskingElement))
                    validMaskFound = true;
            }
        }
    }
    
    // If no valid mask was found, this is not a valid SVG document or it specified an invalid fragment identifier.
    // Fallback to the normal way of loading the document in an Image object.
    if (!validMaskFound) {
        // Get the resource loader, acquire the resource buffer and load it into an image.
        ASSERT(cachedSVGDocument->loader());
        if (SubresourceLoader* loader = cachedSVGDocument->loader()) {
            if (SharedBuffer* dataBuffer = loader->resourceData()) {
                m_styleImage = StyleCachedImage::create(new CachedImage(cachedSVGDocument->resourceRequest(), cachedSVGDocument->sessionID()));
                if (m_renderLayerImageClient)
                    m_styleImage->cachedImage()->addClient(m_renderLayerImageClient);
                for (auto itClient : m_rendererImageClients)
                    m_styleImage->addClient(itClient.key);

                m_styleImage->cachedImage()->setResponse(cachedSVGDocument->response());
                m_styleImage->cachedImage()->finishLoading(dataBuffer);

                // Let the cached resource loader of the document which requested this mask keep a handle to this
                // cached image to ensure it only gets deleted when it should.
                if (m_cachedResourceLoader.get())
                    m_cachedResourceLoader->addCachedResource(*m_styleImage->cachedImage());
            }
            
            // Destroy the current SVG document as its no longer needed
            m_cachedSVGDocumentReference = nullptr;
        }
    }
}

bool MaskImageOperation::drawMask(RenderElement& renderer, BackgroundImageGeometry& geometry, GraphicsContext* context, CompositeOperator compositeOp)
{
    // This method only handles custom masks.
    if (image())
        return false;

    if (RenderSVGResourceMasker* svgMasker = getSVGMasker(renderer)) {
        svgMasker->drawMaskForRenderer(renderer, geometry, context, compositeOp);
        return true;
    }

    return false;
}

RenderSVGResourceMasker* MaskImageOperation::getSVGMasker(RenderElement& renderer)
{
    if (image())
        return nullptr;

    // Identify the element referenced by the fragmentId.
    CachedSVGDocumentReference* svgDocumentReference = cachedSVGDocumentReference();
    Element* elementForMasking = nullptr;
    RenderObject* svgResourceForMasking = nullptr;
    if (svgDocumentReference && svgDocumentReference->document()) {
        SVGDocument* svgDocument = svgDocumentReference->document()->document();
        if (svgDocument && svgDocument->rootElement())
            elementForMasking = svgDocument->rootElement()->getElementById(fragment());
    } else
        elementForMasking = renderer.document().getElementById(fragment());

    if (elementForMasking)
        svgResourceForMasking = elementForMasking->renderer();

    // Check if it's the correct type
    if (svgResourceForMasking && svgResourceForMasking->isSVGResourceContainer() && downcast<RenderSVGResourceContainer>(svgResourceForMasking)->resourceType() == MaskerResourceType)
        return static_cast<RenderSVGResourceMasker*>(svgResourceForMasking);

    return nullptr;
}

} // namespace WebCore
