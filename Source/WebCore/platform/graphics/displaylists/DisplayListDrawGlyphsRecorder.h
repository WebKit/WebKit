/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#include "AffineTransform.h"
#include "Color.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "Pattern.h"
#include "TextFlags.h"
#include <wtf/UniqueRef.h>

#if USE(CORE_TEXT)
#include <CoreGraphics/CoreGraphics.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#endif

namespace WebCore {

class FloatPoint;
class Font;
class GlyphBuffer;
class GraphicsContext;

namespace DisplayList {

class Recorder;

class DrawGlyphsRecorder {
public:
    enum class DrawGlyphsDeconstruction {
        Deconstruct,
        DontDeconstruct
    };
    explicit DrawGlyphsRecorder(Recorder&, DrawGlyphsDeconstruction);

    void drawGlyphs(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned numGlyphs, const FloatPoint& anchorPoint, FontSmoothingMode);

#if USE(CORE_TEXT) && !PLATFORM(WIN)
    void recordBeginLayer(CGRenderingStateRef, CGGStateRef, CGRect);
    void recordEndLayer(CGRenderingStateRef, CGGStateRef);
    void recordDrawGlyphs(CGRenderingStateRef, CGGStateRef, const CGAffineTransform*, const CGGlyph[], const CGPoint positions[], size_t count);
    void recordDrawImage(CGRenderingStateRef, CGGStateRef, CGRect, CGImageRef);
#endif

    DrawGlyphsDeconstruction drawGlyphsDeconstruction() const { return m_drawGlyphsDeconstruction; }

private:
#if USE(CORE_TEXT) && !PLATFORM(WIN)
    UniqueRef<GraphicsContext> createInternalContext();
#endif

    void populateInternalState(const GraphicsContextState&);
    void populateInternalContext(const GraphicsContextState&);
    void prepareInternalContext(const Font&, FontSmoothingMode);
    void concludeInternalContext();

    void updateFillColor(const Color&, Gradient* = nullptr, Pattern* = nullptr);
    void updateStrokeColor(const Color&, Gradient* = nullptr, Pattern* = nullptr);
    void updateCTM(const AffineTransform&);
    enum class ShadowsIgnoreTransforms {
        Unspecified,
        Yes,
        No
    };
    void updateShadow(const FloatSize& shadowOffset, float shadowBlur, const Color& shadowColor, ShadowsIgnoreTransforms);

#if USE(CORE_TEXT) && !PLATFORM(WIN)
    void updateShadow(CGStyleRef);
#endif

    Recorder& m_owner;
    DrawGlyphsDeconstruction m_drawGlyphsDeconstruction;

#if USE(CORE_TEXT) && !PLATFORM(WIN)
    UniqueRef<GraphicsContext> m_internalContext;
#endif

    const Font* m_originalFont { nullptr };
    FontSmoothingMode m_smoothingMode { FontSmoothingMode::AutoSmoothing };
    AffineTransform m_originalTextMatrix;

    struct State {
        struct Style {
            Color color;
            RefPtr<Gradient> gradient;
            AffineTransform gradientSpaceTransform;
            RefPtr<Pattern> pattern;
        };
        Style fillStyle;
        Style strokeStyle;

        AffineTransform ctm;

        struct ShadowState {
            FloatSize offset;
            float blur { 0 };
            Color color;
            bool ignoreTransforms { false };
        };
        ShadowState shadow;
    };
    State m_originalState;
    State m_currentState;
};

}

}
