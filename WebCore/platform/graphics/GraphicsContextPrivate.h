/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef GraphicsContextPrivate_h
#define GraphicsContextPrivate_h

#include "Gradient.h"
#include "GraphicsContext.h"
#include "Pattern.h"
#include "TransformationMatrix.h"

namespace WebCore {

    struct GraphicsContextState {
        GraphicsContextState()
            : textDrawingMode(cTextFill)
            , strokeStyle(SolidStroke)
            , strokeThickness(0)
            , strokeColor(Color::black)
            , strokeColorSpace(ColorSpaceDeviceRGB)
            , fillRule(RULE_NONZERO)
            , fillColor(Color::black)
            , fillColorSpace(ColorSpaceDeviceRGB)
            , shouldAntialias(true)
            , paintingDisabled(false)
            , shadowBlur(0)
            , shadowsIgnoreTransforms(false)
#if PLATFORM(CAIRO)
            , globalAlpha(1)
#endif
        {
        }

        int textDrawingMode;
        
        StrokeStyle strokeStyle;
        float strokeThickness;
        Color strokeColor;
        ColorSpace strokeColorSpace;
        RefPtr<Gradient> strokeGradient;
        RefPtr<Pattern> strokePattern;
        
        WindRule fillRule;
        Color fillColor;
        ColorSpace fillColorSpace;
        RefPtr<Gradient> fillGradient;
        RefPtr<Pattern> fillPattern;

        bool shouldAntialias;

        bool paintingDisabled;
        
        FloatSize shadowOffset;
        float shadowBlur;
        Color shadowColor;

        bool shadowsIgnoreTransforms;
#if PLATFORM(CAIRO)
        float globalAlpha;
#elif PLATFORM(QT)
        TransformationMatrix pathTransform;
#endif
    };

    class GraphicsContextPrivate : public Noncopyable {
    public:
        GraphicsContextPrivate()
            : m_updatingControlTints(false)
        {
        }

        GraphicsContextState state;
        Vector<GraphicsContextState> stack;
        bool m_updatingControlTints;
    };

} // namespace WebCore

#endif // GraphicsContextPrivate_h
