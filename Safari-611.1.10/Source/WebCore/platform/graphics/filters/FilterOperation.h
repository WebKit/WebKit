/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "Color.h"
#include "LayoutSize.h"
#include "Length.h"
#include <wtf/EnumTraits.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/WTFString.h>

// Annoyingly, wingdi.h #defines this.
#ifdef PASSTHROUGH
#undef PASSTHROUGH
#endif

namespace WebCore {

// CSS Filters

class CachedResourceLoader;
class CachedSVGDocumentReference;
class FilterEffect;
struct ResourceLoaderOptions;
template<typename> struct SRGBA;

class FilterOperation : public ThreadSafeRefCounted<FilterOperation> {
public:
    enum OperationType {
        REFERENCE, // url(#somefilter)
        GRAYSCALE,
        SEPIA,
        SATURATE,
        HUE_ROTATE,
        INVERT,
        APPLE_INVERT_LIGHTNESS,
        OPACITY,
        BRIGHTNESS,
        CONTRAST,
        BLUR,
        DROP_SHADOW,
        PASSTHROUGH,
        DEFAULT,
        NONE
    };

    virtual ~FilterOperation() = default;

    virtual Ref<FilterOperation> clone() const = 0;

    virtual bool operator==(const FilterOperation&) const = 0;
    bool operator!=(const FilterOperation& o) const { return !(*this == o); }

    virtual RefPtr<FilterOperation> blend(const FilterOperation* /*from*/, double /*progress*/, bool /*blendToPassthrough*/ = false)
    {
        return nullptr;
    }
    
    virtual bool transformColor(SRGBA<float>&) const { return false; }
    virtual bool inverseTransformColor(SRGBA<float>&) const { return false; }

    OperationType type() const { return m_type; }

    bool isBasicColorMatrixFilterOperation() const
    {
        return m_type == GRAYSCALE || m_type == SEPIA || m_type == SATURATE || m_type == HUE_ROTATE;
    }

    bool isBasicComponentTransferFilterOperation() const
    {
        return m_type == INVERT || m_type == BRIGHTNESS || m_type == CONTRAST || m_type == OPACITY;
    }

    bool isSameType(const FilterOperation& o) const { return o.type() == m_type; }

    // True if the alpha channel of any pixel can change under this operation.
    virtual bool affectsOpacity() const { return false; }
    // True if the value of one pixel can affect the value of another pixel under this operation, such as blur.
    virtual bool movesPixels() const { return false; }
    // True if the filter should not be allowed to work on content that is not available from this security origin.
    virtual bool shouldBeRestrictedBySecurityOrigin() const { return false; }

protected:
    FilterOperation(OperationType type)
        : m_type(type)
    {
    }

    OperationType m_type;
};

class WEBCORE_EXPORT DefaultFilterOperation : public FilterOperation {
public:
    static Ref<DefaultFilterOperation> create(OperationType representedType)
    {
        return adoptRef(*new DefaultFilterOperation(representedType));
    }

    Ref<FilterOperation> clone() const override
    {
        return adoptRef(*new DefaultFilterOperation(representedType()));
    }

    OperationType representedType() const { return m_representedType; }

private:
    bool operator==(const FilterOperation&) const override;

    DefaultFilterOperation(OperationType representedType)
        : FilterOperation(DEFAULT)
        , m_representedType(representedType)
    {
    }

    OperationType m_representedType;
};

class PassthroughFilterOperation : public FilterOperation {
public:
    static Ref<PassthroughFilterOperation> create()
    {
        return adoptRef(*new PassthroughFilterOperation());
    }

    Ref<FilterOperation> clone() const override
    {
        return adoptRef(*new PassthroughFilterOperation());
    }

private:
    bool operator==(const FilterOperation& o) const override
    {
        return isSameType(o);
    }

    PassthroughFilterOperation()
        : FilterOperation(PASSTHROUGH)
    {
    }
};

class ReferenceFilterOperation : public FilterOperation {
public:
    static Ref<ReferenceFilterOperation> create(const String& url, const String& fragment)
    {
        return adoptRef(*new ReferenceFilterOperation(url, fragment));
    }
    virtual ~ReferenceFilterOperation();

    Ref<FilterOperation> clone() const final
    {
        // Reference filters cannot be cloned.
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool affectsOpacity() const override { return true; }
    bool movesPixels() const override { return true; }
    // FIXME: This only needs to return true for graphs that include ConvolveMatrix, DisplacementMap, Morphology and possibly Lighting.
    // https://bugs.webkit.org/show_bug.cgi?id=171753
    bool shouldBeRestrictedBySecurityOrigin() const override { return true; }

    const String& url() const { return m_url; }
    const String& fragment() const { return m_fragment; }

    void loadExternalDocumentIfNeeded(CachedResourceLoader&, const ResourceLoaderOptions&);

    CachedSVGDocumentReference* cachedSVGDocumentReference() const { return m_cachedSVGDocumentReference.get(); }

private:
    ReferenceFilterOperation(const String& url, const String& fragment);

    bool operator==(const FilterOperation&) const override;

    String m_url;
    String m_fragment;
    std::unique_ptr<CachedSVGDocumentReference> m_cachedSVGDocumentReference;
};

// GRAYSCALE, SEPIA, SATURATE and HUE_ROTATE are variations on a basic color matrix effect.
// For HUE_ROTATE, the angle of rotation is stored in m_amount.
class WEBCORE_EXPORT BasicColorMatrixFilterOperation : public FilterOperation {
public:
    static Ref<BasicColorMatrixFilterOperation> create(double amount, OperationType type)
    {
        return adoptRef(*new BasicColorMatrixFilterOperation(amount, type));
    }

    Ref<FilterOperation> clone() const override
    {
        return adoptRef(*new BasicColorMatrixFilterOperation(amount(), type()));
    }

    double amount() const { return m_amount; }

    RefPtr<FilterOperation> blend(const FilterOperation* from, double progress, bool blendToPassthrough = false) override;

private:
    bool operator==(const FilterOperation&) const override;

    double passthroughAmount() const;

    BasicColorMatrixFilterOperation(double amount, OperationType type)
        : FilterOperation(type)
        , m_amount(amount)
    {
    }

    bool transformColor(SRGBA<float>&) const override;

    double m_amount;
};

// INVERT, BRIGHTNESS, CONTRAST and OPACITY are variations on a basic component transfer effect.
class WEBCORE_EXPORT BasicComponentTransferFilterOperation : public FilterOperation {
public:
    static Ref<BasicComponentTransferFilterOperation> create(double amount, OperationType type)
    {
        return adoptRef(*new BasicComponentTransferFilterOperation(amount, type));
    }

    Ref<FilterOperation> clone() const override
    {
        return adoptRef(*new BasicComponentTransferFilterOperation(amount(), type()));
    }

    double amount() const { return m_amount; }

    bool affectsOpacity() const override { return m_type == OPACITY; }

    RefPtr<FilterOperation> blend(const FilterOperation* from, double progress, bool blendToPassthrough = false) override;

private:
    bool operator==(const FilterOperation&) const override;

    double passthroughAmount() const;

    BasicComponentTransferFilterOperation(double amount, OperationType type)
        : FilterOperation(type)
        , m_amount(amount)
    {
    }

    bool transformColor(SRGBA<float>&) const override;

    double m_amount;
};

class WEBCORE_EXPORT InvertLightnessFilterOperation : public FilterOperation {
public:
    static Ref<InvertLightnessFilterOperation> create()
    {
        return adoptRef(*new InvertLightnessFilterOperation());
    }

    Ref<FilterOperation> clone() const final
    {
        return adoptRef(*new InvertLightnessFilterOperation());
    }

    RefPtr<FilterOperation> blend(const FilterOperation* from, double progress, bool blendToPassthrough = false) override;

private:
    bool operator==(const FilterOperation&) const final;

    InvertLightnessFilterOperation()
        : FilterOperation(APPLE_INVERT_LIGHTNESS)
    {
    }

    bool transformColor(SRGBA<float>&) const final;
    bool inverseTransformColor(SRGBA<float>&) const final;
};

class WEBCORE_EXPORT BlurFilterOperation : public FilterOperation {
public:
    static Ref<BlurFilterOperation> create(Length stdDeviation)
    {
        return adoptRef(*new BlurFilterOperation(WTFMove(stdDeviation)));
    }

    Ref<FilterOperation> clone() const override
    {
        return adoptRef(*new BlurFilterOperation(stdDeviation()));
    }

    const Length& stdDeviation() const { return m_stdDeviation; }

    bool affectsOpacity() const override { return true; }
    bool movesPixels() const override { return true; }

    RefPtr<FilterOperation> blend(const FilterOperation* from, double progress, bool blendToPassthrough = false) override;

private:
    bool operator==(const FilterOperation&) const override;

    BlurFilterOperation(Length stdDeviation)
        : FilterOperation(BLUR)
        , m_stdDeviation(WTFMove(stdDeviation))
    {
    }

    Length m_stdDeviation;
};

class WEBCORE_EXPORT DropShadowFilterOperation : public FilterOperation {
public:
    static Ref<DropShadowFilterOperation> create(const IntPoint& location, int stdDeviation, const Color& color)
    {
        return adoptRef(*new DropShadowFilterOperation(location, stdDeviation, color));
    }

    Ref<FilterOperation> clone() const override
    {
        return adoptRef(*new DropShadowFilterOperation(location(), stdDeviation(), color()));
    }

    int x() const { return m_location.x(); }
    int y() const { return m_location.y(); }
    IntPoint location() const { return m_location; }
    int stdDeviation() const { return m_stdDeviation; }
    const Color& color() const { return m_color; }

    bool affectsOpacity() const override { return true; }
    bool movesPixels() const override { return true; }

    RefPtr<FilterOperation> blend(const FilterOperation* from, double progress, bool blendToPassthrough = false) override;

private:
    bool operator==(const FilterOperation&) const override;

    DropShadowFilterOperation(const IntPoint& location, int stdDeviation, const Color& color)
        : FilterOperation(DROP_SHADOW)
        , m_location(location)
        , m_stdDeviation(stdDeviation)
        , m_color(color)
    {
    }

    IntPoint m_location; // FIXME: should location be in Lengths?
    int m_stdDeviation;
    Color m_color;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const FilterOperation&);

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_FILTEROPERATION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::FilterOperation& operation) { return operation.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_FILTEROPERATION(DefaultFilterOperation, type() == WebCore::FilterOperation::DEFAULT)
SPECIALIZE_TYPE_TRAITS_FILTEROPERATION(PassthroughFilterOperation, type() == WebCore::FilterOperation::PASSTHROUGH)
SPECIALIZE_TYPE_TRAITS_FILTEROPERATION(ReferenceFilterOperation, type() == WebCore::FilterOperation::REFERENCE)
SPECIALIZE_TYPE_TRAITS_FILTEROPERATION(BasicColorMatrixFilterOperation, isBasicColorMatrixFilterOperation())
SPECIALIZE_TYPE_TRAITS_FILTEROPERATION(BasicComponentTransferFilterOperation, isBasicComponentTransferFilterOperation())
SPECIALIZE_TYPE_TRAITS_FILTEROPERATION(InvertLightnessFilterOperation, type() == WebCore::FilterOperation::APPLE_INVERT_LIGHTNESS)
SPECIALIZE_TYPE_TRAITS_FILTEROPERATION(BlurFilterOperation, type() == WebCore::FilterOperation::BLUR)
SPECIALIZE_TYPE_TRAITS_FILTEROPERATION(DropShadowFilterOperation, type() == WebCore::FilterOperation::DROP_SHADOW)

namespace WTF {

template<> struct EnumTraits<WebCore::FilterOperation::OperationType> {
    using values = EnumValues<
        WebCore::FilterOperation::OperationType,
        WebCore::FilterOperation::OperationType::REFERENCE,
        WebCore::FilterOperation::OperationType::GRAYSCALE,
        WebCore::FilterOperation::OperationType::SEPIA,
        WebCore::FilterOperation::OperationType::SATURATE,
        WebCore::FilterOperation::OperationType::HUE_ROTATE,
        WebCore::FilterOperation::OperationType::INVERT,
        WebCore::FilterOperation::OperationType::APPLE_INVERT_LIGHTNESS,
        WebCore::FilterOperation::OperationType::OPACITY,
        WebCore::FilterOperation::OperationType::BRIGHTNESS,
        WebCore::FilterOperation::OperationType::CONTRAST,
        WebCore::FilterOperation::OperationType::BLUR,
        WebCore::FilterOperation::OperationType::DROP_SHADOW,
        WebCore::FilterOperation::OperationType::PASSTHROUGH,
        WebCore::FilterOperation::OperationType::DEFAULT,
        WebCore::FilterOperation::OperationType::NONE
    >;
};

} // namespace WTF
