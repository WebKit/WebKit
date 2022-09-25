/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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

#include "CSSImageGeneratorValue.h"
#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "Document.h"
#include "Page.h"
#include "StyleBuilderState.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

Ref<CSSImageSetValue> CSSImageSetValue::create()
{
    return adoptRef(*new CSSImageSetValue);
}

CSSImageSetValue::CSSImageSetValue()
    : CSSValueList(ImageSetClass, CommaSeparator)
{
}

CSSImageSetValue::~CSSImageSetValue() = default;

void CSSImageSetValue::fillImageSet()
{
    size_t length = this->length();
    for (size_t i = 0; i + 1 < length; i += 2) {
        CSSValue* imageValue = item(i);
        CSSValue* scaleFactorValue = item(i + 1);

        ASSERT(is<CSSImageValue>(imageValue) || is<CSSImageGeneratorValue>(imageValue));
        ASSERT(is<CSSPrimitiveValue>(scaleFactorValue));

        float scaleFactor = downcast<CSSPrimitiveValue>(scaleFactorValue)->floatValue(CSSUnitType::CSS_DPPX);
        m_imagesInSet.append({ imageValue, scaleFactor });
    }

    // Sort the images so that they are stored in order from lowest resolution to highest.
    std::stable_sort(m_imagesInSet.begin(), m_imagesInSet.end(), CSSImageSetValue::compareByScaleFactor);
}

ImageWithScale CSSImageSetValue::bestImageForScaleFactor()
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

CachedImage* CSSImageSetValue::cachedImage() const
{
    if (is<CSSImageValue>(m_selectedImageValue))
        return downcast<CSSImageValue>(*m_selectedImageValue).cachedImage();
    return nullptr;
}

ImageWithScale CSSImageSetValue::selectBestFitImage(const Document& document)
{
    updateDeviceScaleFactor(document);

    if (!m_accessedBestFitImage) {
        m_accessedBestFitImage = true;
        m_bestFitImage = bestImageForScaleFactor();
    }

    return m_bestFitImage;
}

void CSSImageSetValue::updateDeviceScaleFactor(const Document& document)
{
    // FIXME: In the future, we want to take much more than deviceScaleFactor into acount here.
    // All forms of scale should be included: Page::pageScaleFactor(), Frame::pageZoomFactor(),
    // and any CSS transforms. https://bugs.webkit.org/show_bug.cgi?id=81698
    float deviceScaleFactor = document.page() ? document.page()->deviceScaleFactor() : 1;
    if (deviceScaleFactor == m_deviceScaleFactor)
        return;
    m_deviceScaleFactor = deviceScaleFactor;
    m_accessedBestFitImage = false;
    m_selectedImageValue = nullptr;
}

Ref<CSSImageSetValue> CSSImageSetValue::valueWithStylesResolved(Style::BuilderState& builderState)
{
    auto result = create();
    for (size_t i = 0, length = this->length(); i + 1 < length; i += 2) {
        result->append(builderState.resolveImageStyles(*itemWithoutBoundsCheck(i)));
        result->append(*itemWithoutBoundsCheck(i + 1));
    }
    return equals(result) ? Ref { *this } : result;
}

String CSSImageSetValue::customCSSText() const
{
    StringBuilder result;
    result.append("image-set(");
    size_t length = this->length();
    for (size_t i = 0; i + 1 < length; i += 2) {
        if (i > 0)
            result.append(", ");
        result.append(item(i)->cssText(), ' ', item(i + 1)->cssText());
    }
    result.append(')');
    return result.toString();
}

bool CSSImageSetValue::customTraverseSubresources(const Function<bool(const CachedResource&)>& handler) const
{
    return CSSValueList::customTraverseSubresources(handler) || (m_selectedImageValue && m_selectedImageValue->traverseSubresources(handler));
}

} // namespace WebCore
