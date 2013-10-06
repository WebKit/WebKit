/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef StylePendingImage_h
#define StylePendingImage_h

#include "CSSCursorImageValue.h"
#include "CSSImageGeneratorValue.h"
#include "CSSImageValue.h"
#include "Image.h"
#include "StyleImage.h"

#if ENABLE(CSS_IMAGE_SET)
#include "CSSImageSetValue.h"
#endif

namespace WebCore {

// StylePendingImage is a placeholder StyleImage that is entered into the RenderStyle during
// style resolution, in order to avoid loading images that are not referenced by the final style.
// They should never exist in a RenderStyle after it has been returned from the style selector.

class StylePendingImage : public StyleImage {
public:
    static PassRefPtr<StylePendingImage> create(CSSValue* value) { return adoptRef(new StylePendingImage(value)); }

    CSSImageValue* cssImageValue() const { return m_value->isImageValue() ? toCSSImageValue(m_value) : nullptr; }
    CSSImageGeneratorValue* cssImageGeneratorValue() const { return m_value->isImageGeneratorValue() ? static_cast<CSSImageGeneratorValue*>(m_value) : nullptr; }
    CSSCursorImageValue* cssCursorImageValue() const { return m_value->isCursorImageValue() ? toCSSCursorImageValue(m_value) : nullptr; }
#if ENABLE(CSS_IMAGE_SET)
    CSSImageSetValue* cssImageSetValue() const { return m_value->isImageSetValue() ? toCSSImageSetValue(m_value) : nullptr; }
#endif

private:
    virtual WrappedImagePtr data() const OVERRIDE { return toCSSImageValue(m_value); }

    virtual PassRefPtr<CSSValue> cssValue() const OVERRIDE { return m_value; }
    
    virtual LayoutSize imageSize(const RenderElement*, float /*multiplier*/) const OVERRIDE { return LayoutSize(); }
    virtual bool imageHasRelativeWidth() const OVERRIDE { return false; }
    virtual bool imageHasRelativeHeight() const OVERRIDE { return false; }
    virtual void computeIntrinsicDimensions(const RenderElement*, Length& /* intrinsicWidth */ , Length& /* intrinsicHeight */, FloatSize& /* intrinsicRatio */) { }
    virtual bool usesImageContainerSize() const OVERRIDE { return false; }
    virtual void setContainerSizeForRenderer(const RenderElement*, const IntSize&, float) OVERRIDE { }
    virtual void addClient(RenderElement*) OVERRIDE { }
    virtual void removeClient(RenderElement*) OVERRIDE { }

    virtual PassRefPtr<Image> image(RenderElement*, const IntSize&) const OVERRIDE
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    virtual bool knownToBeOpaque(const RenderElement*) const OVERRIDE { return false; }
    
    StylePendingImage(CSSValue* value)
        : m_value(value)
    {
        m_isPendingImage = true;
    }

    CSSValue* m_value; // Not retained; it owns us.
};

}
#endif
