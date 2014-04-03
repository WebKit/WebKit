/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(CSS_FILTERS)
#include "FilterOperation.h"

#include "AnimationUtilities.h"
#include "CachedSVGDocumentReference.h"

namespace WebCore {
    
bool DefaultFilterOperation::operator==(const FilterOperation& o) const
{
    if (!isSameType(o))
        return false;
    
    return representedType() == toDefaultFilterOperation(o).representedType();
}

ReferenceFilterOperation::ReferenceFilterOperation(const String& url, const String& fragment)
    : FilterOperation(REFERENCE)
    , m_url(url)
    , m_fragment(fragment)
{
}

ReferenceFilterOperation::~ReferenceFilterOperation()
{
}
    
bool ReferenceFilterOperation::operator==(const FilterOperation& o) const
{
    if (!isSameType(o))
        return false;
    
    return m_url == toReferenceFilterOperation(o).m_url;
}
    
CachedSVGDocumentReference* ReferenceFilterOperation::getOrCreateCachedSVGDocumentReference()
{
    if (!m_cachedSVGDocumentReference)
        m_cachedSVGDocumentReference = std::make_unique<CachedSVGDocumentReference>(m_url);
    return m_cachedSVGDocumentReference.get();
}

PassRefPtr<FilterOperation> BasicColorMatrixFilterOperation::blend(const FilterOperation* from, double progress, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;
    
    if (blendToPassthrough)
        return BasicColorMatrixFilterOperation::create(WebCore::blend(m_amount, passthroughAmount(), progress), m_type);
        
    const BasicColorMatrixFilterOperation* fromOp = toBasicColorMatrixFilterOperation(from);
    double fromAmount = fromOp ? fromOp->amount() : passthroughAmount();
    return BasicColorMatrixFilterOperation::create(WebCore::blend(fromAmount, m_amount, progress), m_type);
}

inline bool BasicColorMatrixFilterOperation::operator==(const FilterOperation& o) const
{
    if (!isSameType(o))
        return false;
    const BasicColorMatrixFilterOperation& other = toBasicColorMatrixFilterOperation(o);
    return m_amount == other.m_amount;
}

double BasicColorMatrixFilterOperation::passthroughAmount() const
{
    switch (m_type) {
    case GRAYSCALE:
    case SEPIA:
    case HUE_ROTATE:
        return 0;
    case SATURATE:
        return 1;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

PassRefPtr<FilterOperation> BasicComponentTransferFilterOperation::blend(const FilterOperation* from, double progress, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;
    
    if (blendToPassthrough)
        return BasicComponentTransferFilterOperation::create(WebCore::blend(m_amount, passthroughAmount(), progress), m_type);
        
    const BasicComponentTransferFilterOperation* fromOp = toBasicComponentTransferFilterOperation(from);
    double fromAmount = fromOp ? fromOp->amount() : passthroughAmount();
    return BasicComponentTransferFilterOperation::create(WebCore::blend(fromAmount, m_amount, progress), m_type);
}

inline bool BasicComponentTransferFilterOperation::operator==(const FilterOperation& o) const
{
    if (!isSameType(o))
        return false;
    const BasicComponentTransferFilterOperation& other = toBasicComponentTransferFilterOperation(o);
    return m_amount == other.m_amount;
}

double BasicComponentTransferFilterOperation::passthroughAmount() const
{
    switch (m_type) {
    case OPACITY:
        return 1;
    case INVERT:
        return 0;
    case CONTRAST:
        return 1;
    case BRIGHTNESS:
        return 1;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}
    
bool BlurFilterOperation::operator==(const FilterOperation& o) const
{
    if (!isSameType(o))
        return false;
    
    return m_stdDeviation == toBlurFilterOperation(o).stdDeviation();
}
    
PassRefPtr<FilterOperation> BlurFilterOperation::blend(const FilterOperation* from, double progress, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;

    LengthType lengthType = m_stdDeviation.type();

    if (blendToPassthrough)
        return BlurFilterOperation::create(Length(lengthType).blend(m_stdDeviation, progress));

    const BlurFilterOperation* fromOp = toBlurFilterOperation(from);
    Length fromLength = fromOp ? fromOp->m_stdDeviation : Length(lengthType);
    return BlurFilterOperation::create(m_stdDeviation.blend(fromLength, progress));
}
    
bool DropShadowFilterOperation::operator==(const FilterOperation& o) const
{
    if (!isSameType(o))
        return false;
    const DropShadowFilterOperation& other = toDropShadowFilterOperation(o);
    return m_location == other.m_location && m_stdDeviation == other.m_stdDeviation && m_color == other.m_color;
}
    
PassRefPtr<FilterOperation> DropShadowFilterOperation::blend(const FilterOperation* from, double progress, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;

    if (blendToPassthrough)
        return DropShadowFilterOperation::create(
            WebCore::blend(m_location, IntPoint(), progress),
            WebCore::blend(m_stdDeviation, 0, progress),
            WebCore::blend(m_color, Color(Color::transparent), progress));

    const DropShadowFilterOperation* fromOp = toDropShadowFilterOperation(from);
    IntPoint fromLocation = fromOp ? fromOp->location() : IntPoint();
    int fromStdDeviation = fromOp ? fromOp->stdDeviation() : 0;
    Color fromColor = fromOp ? fromOp->color() : Color(Color::transparent);
    
    return DropShadowFilterOperation::create(
        WebCore::blend(fromLocation, m_location, progress),
        WebCore::blend(fromStdDeviation, m_stdDeviation, progress),
        WebCore::blend(fromColor, m_color, progress));
}

} // namespace WebCore

#endif // ENABLE(CSS_FILTERS)
