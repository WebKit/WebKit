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

namespace WebCore {

    struct GraphicsContextState {
        GraphicsContextState()
            : strokeThickness(0)
            , shadowBlur(0)
#if PLATFORM(CAIRO)
            , globalAlpha(1)
#endif
            , textDrawingMode(TextModeFill)
            , strokeColor(Color::black)
            , fillColor(Color::black)
            , strokeStyle(SolidStroke)
            , fillRule(RULE_NONZERO)
            , strokeColorSpace(ColorSpaceDeviceRGB)
            , fillColorSpace(ColorSpaceDeviceRGB)
            , shouldAntialias(true)
            , paintingDisabled(false)
            , shadowsIgnoreTransforms(false)
        {
        }

        RefPtr<Gradient> strokeGradient;
        RefPtr<Pattern> strokePattern;
        
        RefPtr<Gradient> fillGradient;
        RefPtr<Pattern> fillPattern;

        FloatSize shadowOffset;

        float strokeThickness;
        float shadowBlur;

#if PLATFORM(CAIRO)
        float globalAlpha;
#endif
        TextDrawingModeFlags textDrawingMode;

        Color strokeColor;
        Color fillColor;
        Color shadowColor;

        StrokeStyle strokeStyle;
        WindRule fillRule;

        ColorSpace strokeColorSpace;
        ColorSpace fillColorSpace;

        bool shouldAntialias;
        bool paintingDisabled;
        bool shadowsIgnoreTransforms;
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
