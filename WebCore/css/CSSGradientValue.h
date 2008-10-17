/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#ifndef CSSGradientValue_h
#define CSSGradientValue_h

#include "CSSImageGeneratorValue.h"
#include "CSSPrimitiveValue.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class FloatPoint;
class Gradient;

enum CSSGradientType { CSSLinearGradient, CSSRadialGradient };

struct CSSGradientColorStop {
    CSSGradientColorStop()
        : m_stop(0)
    {
    }
    
    float m_stop;
    RefPtr<CSSPrimitiveValue> m_color;
};

class CSSGradientValue : public CSSImageGeneratorValue {
public:
    static PassRefPtr<CSSGradientValue> create()
    {
        return adoptRef(new CSSGradientValue);
    }

    virtual String cssText() const;

    virtual Image* image(RenderObject*, const IntSize&);

    CSSGradientType type() const { return m_type; }
    void setType(CSSGradientType type) { m_type = type; }
    
    void setFirstX(PassRefPtr<CSSPrimitiveValue> val) { m_firstX = val; }
    void setFirstY(PassRefPtr<CSSPrimitiveValue> val) { m_firstY = val; }
    void setSecondX(PassRefPtr<CSSPrimitiveValue> val) { m_secondX = val; }
    void setSecondY(PassRefPtr<CSSPrimitiveValue> val) { m_secondY = val; }
    
    void setFirstRadius(PassRefPtr<CSSPrimitiveValue> val) { m_firstRadius = val; }
    void setSecondRadius(PassRefPtr<CSSPrimitiveValue> val) { m_secondRadius = val; }

    void addStop(const CSSGradientColorStop& stop) { m_stops.append(stop); }

    void sortStopsIfNeeded();

private:
    CSSGradientValue()
        : m_type(CSSLinearGradient)
        , m_stopsSorted(false)
    {
    }
    
    // Create the gradient for a given size.
    PassRefPtr<Gradient> createGradient(RenderObject*, const IntSize&);
    
    // Resolve points/radii to front end values.
    FloatPoint resolvePoint(CSSPrimitiveValue*, CSSPrimitiveValue*, const IntSize&, float zoomFactor);
    float resolveRadius(CSSPrimitiveValue*, float zoomFactor);
    
    // Type
    CSSGradientType m_type;

    // Points
    RefPtr<CSSPrimitiveValue> m_firstX;
    RefPtr<CSSPrimitiveValue> m_firstY;
    
    RefPtr<CSSPrimitiveValue> m_secondX;
    RefPtr<CSSPrimitiveValue> m_secondY;
    
    // Radii (for radial gradients only)
    RefPtr<CSSPrimitiveValue> m_firstRadius;
    RefPtr<CSSPrimitiveValue> m_secondRadius;
    
    // Stops
    Vector<CSSGradientColorStop> m_stops;
    bool m_stopsSorted;
};

} // namespace WebCore

#endif // CSSGradientValue_h
