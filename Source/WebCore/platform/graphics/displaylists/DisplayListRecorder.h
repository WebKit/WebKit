/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/Image.h> // For Image::TileRule.
#include <WebCore/TextFlags.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>

#if USE(CORE_TEXT)
#include <WebCore/DrawGlyphsRecorder.h>
#endif

namespace WebCore {

class FloatPoint;
class FloatRect;
class Font;
class GlyphBuffer;
class Gradient;
class Image;
class SourceImage;
class VideoFrame;

struct ImagePaintingOptions;

namespace DisplayList {

class Recorder : public GraphicsContext {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(Recorder, WEBCORE_EXPORT);
    WTF_MAKE_NONCOPYABLE(Recorder);
public:
    enum class DrawGlyphsMode {
        Normal,
        // Deconstruct different text layers into separate DrawGlyphs commands. Allows for doing the deconstruct work once during recording instead of
        // multiple times during multiple playbacks.
        Deconstruct,
#if USE(SKIA)
        TextBlob
#endif
    };

    Recorder(const GraphicsContextState& state, const FloatRect& initialClip, const AffineTransform& transform, const DestinationColorSpace& colorSpace, DrawGlyphsMode drawGlyphsMode = DrawGlyphsMode::Normal)
        : Recorder(IsDeferred::Yes, state, initialClip, transform, colorSpace, drawGlyphsMode)
    {
    }
    WEBCORE_EXPORT virtual ~Recorder();

    WEBCORE_EXPORT void appendDisplayList(const DisplayList&);

protected:
    WEBCORE_EXPORT Recorder(IsDeferred, const GraphicsContextState&, const FloatRect& initialClip, const AffineTransform&, const DestinationColorSpace&, DrawGlyphsMode);

    struct ContextState {
        GraphicsContextState state;
        AffineTransform ctm;
        FloatRect clipBounds;
        std::optional<GraphicsContextState> lastDrawingState { std::nullopt };

        ContextState cloneForTransparencyLayer() const
        {
            auto stateClone = state.clone(GraphicsContextState::Purpose::TransparencyLayer);
            std::optional<GraphicsContextState> lastDrawingStateClone;
            if (lastDrawingStateClone)
                lastDrawingStateClone = lastDrawingState->clone(GraphicsContextState::Purpose::TransparencyLayer);
            return ContextState { WTFMove(stateClone), ctm, clipBounds, WTFMove(lastDrawingStateClone) };
        }

        void translate(float x, float y);
        void rotate(float angleInRadians);
        void scale(const FloatSize&);
        void concatCTM(const AffineTransform&);
        void setCTM(const AffineTransform&);
    };

    const Vector<ContextState, 4>& stateStack() const { return m_stateStack; }

    const ContextState& currentState() const;
    ContextState& currentState();

protected:
    WEBCORE_EXPORT void updateStateForSave(GraphicsContextState::Purpose);
    [[nodiscard]] WEBCORE_EXPORT bool updateStateForRestore(GraphicsContextState::Purpose);
    [[nodiscard]] WEBCORE_EXPORT bool updateStateForTranslate(float x, float y);
    [[nodiscard]] WEBCORE_EXPORT bool updateStateForRotate(float angleInRadians);
    [[nodiscard]] WEBCORE_EXPORT bool updateStateForScale(const FloatSize&);
    [[nodiscard]] WEBCORE_EXPORT bool updateStateForConcatCTM(const AffineTransform&);
    WEBCORE_EXPORT void updateStateForSetCTM(const AffineTransform&);
    WEBCORE_EXPORT void updateStateForBeginTransparencyLayer(float opacity);
    WEBCORE_EXPORT void updateStateForBeginTransparencyLayer(CompositeOperator, BlendMode);
    [[nodiscard]] WEBCORE_EXPORT bool updateStateForEndTransparencyLayer();
    WEBCORE_EXPORT void updateStateForResetClip();
    WEBCORE_EXPORT void updateStateForClip(const FloatRect&);
    WEBCORE_EXPORT void updateStateForClipRoundedRect(const FloatRoundedRect&);
    WEBCORE_EXPORT void updateStateForClipPath(const Path&);
    WEBCORE_EXPORT void updateStateForClipOut(const FloatRect&);
    WEBCORE_EXPORT void updateStateForClipOut(const Path&);
    WEBCORE_EXPORT void updateStateForClipOutRoundedRect(const FloatRoundedRect&);
    WEBCORE_EXPORT void updateStateForClipToImageBuffer(const FloatRect&);
    WEBCORE_EXPORT void updateStateForApplyDeviceScaleFactor(float);
    WEBCORE_EXPORT bool decomposeDrawGlyphsIfNeeded(const Font&, std::span<const GlyphBufferGlyph>, std::span<const GlyphBufferAdvance>, const FloatPoint& anchorPoint, FontSmoothingMode);
    FloatRect initialClip() const { return m_initialClip; }
    DrawGlyphsMode drawGlyphsMode() const { return m_drawGlyphsMode; }

    const DestinationColorSpace& colorSpace() const final { return m_colorSpace; }

private:
    bool hasPlatformContext() const final { return false; }
    PlatformGraphicsContext* platformContext() const final { ASSERT_NOT_REACHED(); return nullptr; }

#if USE(CG)
    bool isCALayerContext() const final { return false; }
#endif

    void fillRoundedRectImpl(const FloatRoundedRect&, const Color&) final { ASSERT_NOT_REACHED(); }

    WEBCORE_EXPORT const GraphicsContextState& state() const final;

    WEBCORE_EXPORT void didUpdateState(GraphicsContextState&) final;
    WEBCORE_EXPORT void didUpdateSingleState(GraphicsContextState&, GraphicsContextState::ChangeIndex) final;
    WEBCORE_EXPORT void drawConsumingImageBuffer(RefPtr<ImageBuffer>, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions) final;
    WEBCORE_EXPORT AffineTransform getCTM(GraphicsContext::IncludeDeviceScale = PossiblyIncludeDeviceScale) const final;
    WEBCORE_EXPORT IntRect clipBounds() const final;

    virtual void appendStateChangeItemIfNecessary() = 0;

    const AffineTransform& ctm() const;

    Vector<ContextState, 4> m_stateStack;
    DestinationColorSpace m_colorSpace;
    const FloatRect m_initialClip;
    const DrawGlyphsMode m_drawGlyphsMode { DrawGlyphsMode::Normal };
#if USE(CORE_TEXT)
    std::unique_ptr<DrawGlyphsRecorder> m_drawGlyphsRecorder;
    float m_initialScale { 1 };
#endif
};

inline const Recorder::ContextState& Recorder::currentState() const
{
    ASSERT(m_stateStack.size());
    return m_stateStack.last();
}

inline Recorder::ContextState& Recorder::currentState()
{
    ASSERT(m_stateStack.size());
    return m_stateStack.last();
}

} // namespace DisplayList
} // namespace WebCore
