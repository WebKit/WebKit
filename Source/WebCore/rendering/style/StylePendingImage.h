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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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
#include "StyleImage.h"

#if ENABLE(CSS_IMAGE_SET)
#include "CSSImageSetValue.h"
#endif

namespace WebCore {

// StylePendingImage is a placeholder StyleImage that is entered into the RenderStyle during
// style resolution, in order to avoid loading images that are not referenced by the final style.
// They should never exist in a RenderStyle after it has been returned from the style selector.

class StylePendingImage final : public StyleImage {
public:
    static Ref<StylePendingImage> create(CSSValue* value) { return adoptRef(*new StylePendingImage(value)); }

    CSSImageValue* cssImageValue() const { return is<CSSImageValue>(m_value) ? downcast<CSSImageValue>(m_value) : nullptr; }
    CSSImageGeneratorValue* cssImageGeneratorValue() const { return is<CSSImageGeneratorValue>(m_value) ? static_cast<CSSImageGeneratorValue*>(m_value) : nullptr; }
    CSSCursorImageValue* cssCursorImageValue() const { return is<CSSCursorImageValue>(m_value) ? downcast<CSSCursorImageValue>(m_value) : nullptr; }

#if ENABLE(CSS_IMAGE_SET)
    CSSImageSetValue* cssImageSetValue() const { return is<CSSImageSetValue>(m_value) ? downcast<CSSImageSetValue>(m_value) : nullptr; }
#endif

    void detachFromCSSValue() { m_value = nullptr; }

private:
    WrappedImagePtr data() const override { return const_cast<StylePendingImage*>(this); }

    PassRefPtr<CSSValue> cssValue() const override { return m_value; }
    
    FloatSize imageSize(const RenderElement*, float /*multiplier*/) const override { return FloatSize(); }
    bool imageHasRelativeWidth() const override { return false; }
    bool imageHasRelativeHeight() const override { return false; }
    void computeIntrinsicDimensions(const RenderElement*, Length& /* intrinsicWidth */ , Length& /* intrinsicHeight */, FloatSize& /* intrinsicRatio */) override { }
    bool usesImageContainerSize() const override { return false; }
    void setContainerSizeForRenderer(const RenderElement*, const FloatSize&, float) override { }
    void addClient(RenderElement*) override { }
    void removeClient(RenderElement*) override { }

    RefPtr<Image> image(RenderElement*, const FloatSize&) const override
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    bool knownToBeOpaque(const RenderElement*) const override { return false; }
    
    StylePendingImage(CSSValue* value)
        : m_value(value)
    {
        m_isPendingImage = true;
    }

    CSSValue* m_value; // Not retained; it owns us.
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(StylePendingImage, isPendingImage)

#endif // StylePendingImage_h
