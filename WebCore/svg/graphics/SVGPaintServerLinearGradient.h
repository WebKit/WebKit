/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGPaintServerLinearGradient_h
#define SVGPaintServerLinearGradient_h

#if ENABLE(SVG)

#include "FloatPoint.h"
#include "SVGPaintServerGradient.h"

namespace WebCore {

    class SVGPaintServerLinearGradient : public SVGPaintServerGradient {
    public:
        static PassRefPtr<SVGPaintServerLinearGradient> create(const SVGGradientElement* owner) { return adoptRef(new SVGPaintServerLinearGradient(owner)); }
        virtual ~SVGPaintServerLinearGradient();

        virtual SVGPaintServerType type() const { return LinearGradientPaintServer; }

        FloatPoint gradientStart() const;
        void setGradientStart(const FloatPoint&);

        FloatPoint gradientEnd() const;
        void setGradientEnd(const FloatPoint&);

        virtual TextStream& externalRepresentation(TextStream&) const;

    private:
        SVGPaintServerLinearGradient(const SVGGradientElement* owner);

        FloatPoint m_start;
        FloatPoint m_end;
    };

} // namespace WebCore

#endif

#endif // SVGPaintServerLinearGradient_h
