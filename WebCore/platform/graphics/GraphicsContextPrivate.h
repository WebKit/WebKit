/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "TransformationMatrix.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "Pattern.h"

namespace WebCore {

    struct GraphicsContextState {
        GraphicsContextState()
            : textDrawingMode(cTextFill)
            , strokeStyle(SolidStroke)
            , strokeThickness(0)
            , strokeType(SolidColorType)
            , strokeColor(Color::black)
            , fillRule(RULE_NONZERO)
            , fillType(SolidColorType)
            , fillColor(Color::black)
            , shouldAntialias(true)
            , paintingDisabled(false)
            , shadowBlur(0)
            , shadowsIgnoreTransforms(false)
#if PLATFORM(CAIRO)
            , globalAlpha(1.0f)
#endif
        {
        }

        int textDrawingMode;
        
        StrokeStyle strokeStyle;
        float strokeThickness;
        FillOrStrokeType strokeType;
        Color strokeColor;
        RefPtr<Gradient> strokeGradient;
        RefPtr<Pattern> strokePattern;
        
        WindRule fillRule;
        FillOrStrokeType fillType;
        Color fillColor;
        RefPtr<Gradient> fillGradient;
        RefPtr<Pattern> fillPattern;

        bool shouldAntialias;

        bool paintingDisabled;
        
        IntSize shadowSize;
        unsigned shadowBlur;
        Color shadowColor;

        bool shadowsIgnoreTransforms;
#if PLATFORM(CAIRO)
        float globalAlpha;
#elif PLATFORM(QT)
        TransformationMatrix pathTransform;
#endif
    };

    class GraphicsContextPrivate {
    public:
        GraphicsContextPrivate()
            : m_focusRingWidth(0)
            , m_focusRingOffset(0)
            , m_updatingControlTints(false)
        {
        }

        GraphicsContextState state;
        Vector<GraphicsContextState> stack;
        Vector<IntRect> m_focusRingRects;
        int m_focusRingWidth;
        int m_focusRingOffset;
        bool m_updatingControlTints;
    };

} // namespace WebCore

#endif // GraphicsContextPrivate_h
