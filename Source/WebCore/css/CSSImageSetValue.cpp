/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CSSImageSetValue.h"

#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiators.h"
#include "Document.h"
#include "Page.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSImageSetValue::CSSImageSetValue(LoadedFromOpaqueSource loadedFromOpaqueSource)
    : CSSValueList(ImageSetClass, CommaSeparator)
    , m_loadedFromOpaqueSource(loadedFromOpaqueSource)
{
}

CSSImageSetValue::~CSSImageSetValue() = default;

void CSSImageSetValue::fillImageSet()
{
    size_t length = this->length();
    size_t i = 0;
    while (i < length) {
        CSSValue* imageValue = item(i);
        URL imageURL = downcast<CSSImageValue>(*imageValue).url();

        ++i;
        ASSERT_WITH_SECURITY_IMPLICATION(i < length);
        CSSValue* scaleFactorValue = item(i);
        float scaleFactor = downcast<CSSPrimitiveValue>(*scaleFactorValue).floatValue();

        ImageWithScale image;
        image.imageURL = imageURL;
        image.scaleFactor = scaleFactor;
        m_imagesInSet.append(image);
        ++i;
    }

    // Sort the images so that they are stored in order from lowest resolution to highest.
    std::sort(m_imagesInSet.begin(), m_imagesInSet.end(), CSSImageSetValue::compareByScaleFactor);
}

CSSImageSetValue::ImageWithScale CSSImageSetValue::bestImageForScaleFactor()
{
    if (!m_imagesInSet.size())
        fillImageSet();

    ImageWithScale image;
    size_t numberOfImages = m_imagesInSet.size();
    for (size_t i = 0; i < numberOfImages; ++i) {
        image = m_imagesInSet.at(i);
        if (image.scaleFactor >= m_deviceScaleFactor)
            return image;
    }
    return image;
}

std::pair<CachedImage*, float> CSSImageSetValue::loadBestFitImage(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    Document* document = loader.document();
    ASSERT(document);

    updateDeviceScaleFactor(*document);

    if (!m_accessedBestFitImage) {
        m_accessedBestFitImage = true;

        // FIXME: In the future, we want to take much more than deviceScaleFactor into acount here.
        // All forms of scale should be included: Page::pageScaleFactor(), Frame::pageZoomFactor(),
        // and any CSS transforms. https://bugs.webkit.org/show_bug.cgi?id=81698
        ImageWithScale image = bestImageForScaleFactor();

        ResourceLoaderOptions loadOptions = options;
        loadOptions.loadedFromOpaqueSource = m_loadedFromOpaqueSource;
        CachedResourceRequest request(ResourceRequest(document->completeURL(image.imageURL)), loadOptions);
        request.setInitiator(cachedResourceRequestInitiators().css);
        if (options.mode == FetchOptions::Mode::Cors)
            request.updateForAccessControl(*document);

        m_cachedImage = loader.requestImage(WTFMove(request)).value_or(nullptr);
        m_bestFitImageScaleFactor = image.scaleFactor;
    }
    return { m_cachedImage.get(), m_bestFitImageScaleFactor };
}

void CSSImageSetValue::updateDeviceScaleFactor(const Document& document)
{
    float deviceScaleFactor = document.page() ? document.page()->deviceScaleFactor() : 1;
    if (deviceScaleFactor == m_deviceScaleFactor)
        return;
    m_deviceScaleFactor = deviceScaleFactor;
    m_accessedBestFitImage = false;
    m_cachedImage = nullptr;
}

String CSSImageSetValue::customCSSText() const
{
    StringBuilder result;
    result.appendLiteral("image-set(");

    size_t length = this->length();
    size_t i = 0;
    while (i < length) {
        if (i > 0)
            result.appendLiteral(", ");

        const CSSValue* imageValue = item(i);
        result.append(imageValue->cssText());
        result.append(' ');

        ++i;
        ASSERT_WITH_SECURITY_IMPLICATION(i < length);
        const CSSValue* scaleFactorValue = item(i);
        result.append(scaleFactorValue->cssText());
        // FIXME: Eventually the scale factor should contain it's own unit http://wkb.ug/100120.
        // For now 'x' is hard-coded in the parser, so we hard-code it here too.
        result.append('x');

        ++i;
    }

    result.append(')');
    return result.toString();
}

bool CSSImageSetValue::traverseSubresources(const WTF::Function<bool (const CachedResource&)>& handler) const
{
    if (!m_cachedImage)
        return false;
    return handler(*m_cachedImage);
}

} // namespace WebCore
