/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NicosiaCairoOperationRecorder.h"

#include "CairoOperations.h"
#include "FloatRoundedRect.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "NicosiaPaintingOperationReplayCairo.h"
#include <type_traits>
#include <wtf/text/TextStream.h>

#if PLATFORM(WPE) || PLATFORM(GTK)
#include "ThemeAdwaita.h"
#endif

namespace Nicosia {
using namespace WebCore;

GraphicsContextCairo& contextForReplay(PaintingOperationReplay& operationReplay)
{
    return static_cast<PaintingOperationReplayCairo&>(operationReplay).platformContext;
}

template<typename... Args>
struct OperationData {
    template<std::size_t I>
    auto arg() const -> std::tuple_element_t<I, const std::tuple<Args...>>&
    {
        return std::get<I>(arguments);
    }

    std::tuple<Args...> arguments;
};

template<> struct OperationData<> { };

template<typename T, typename... Args>
auto createCommand(Args&&... arguments) -> std::enable_if_t<std::is_base_of<OperationData<std::decay_t<Args>...>, T>::value, std::unique_ptr<PaintingOperation>>
{
    auto* command = new T();
    command->arguments = std::make_tuple(std::forward<Args>(arguments)...);
    return std::unique_ptr<PaintingOperation>(command);
}

template<typename T>
auto createCommand() -> std::enable_if_t<std::is_base_of<OperationData<>, T>::value, std::unique_ptr<PaintingOperation>>
{
    return makeUnique<T>();
}

CairoOperationRecorder::CairoOperationRecorder(PaintingOperations& commandList)
    : m_commandList(commandList)
{
    m_stateStack.append({ { }, { }, FloatRect::infiniteRect() });
}

void CairoOperationRecorder::updateState(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
{
    if (flags & GraphicsContextState::StrokeThicknessChange) {
        struct StrokeThicknessChange final : PaintingOperation, OperationData<float> {
            virtual ~StrokeThicknessChange() = default;

            void execute(PaintingOperationReplay& replayer) override
            {
                Cairo::State::setStrokeThickness(contextForReplay(replayer), arg<0>());
            }

            void dump(TextStream& ts) override
            {
                ts << indent << "StrokeThicknessChange<>\n";
            }
        };

        append(createCommand<StrokeThicknessChange>(state.strokeThickness));
    }

    if (flags & GraphicsContextState::StrokeStyleChange) {
        struct StrokeStyleChange final : PaintingOperation, OperationData<StrokeStyle> {
            virtual ~StrokeStyleChange() = default;

            void execute(PaintingOperationReplay& replayer) override
            {
                Cairo::State::setStrokeStyle(contextForReplay(replayer), arg<0>());
            }

            void dump(TextStream& ts) override
            {
                ts << indent << "StrokeStyleChange<>\n";
            }
        };

        append(createCommand<StrokeStyleChange>(state.strokeStyle));
    }

    if (flags & GraphicsContextState::CompositeOperationChange) {
        struct CompositeOperationChange final : PaintingOperation, OperationData<CompositeOperator, BlendMode> {
            virtual ~CompositeOperationChange() = default;

            void execute(PaintingOperationReplay& replayer) override
            {
                Cairo::State::setCompositeOperation(contextForReplay(replayer), arg<0>(), arg<1>());
            }

            void dump(TextStream& ts) override
            {
                ts << indent << "CompositeOperationChange<>\n";
            }
        };

        append(createCommand<CompositeOperationChange>(state.compositeOperator, state.blendMode));
    }

    if (flags & GraphicsContextState::ShouldAntialiasChange) {
        struct ShouldAntialiasChange final : PaintingOperation, OperationData<bool> {
            virtual ~ShouldAntialiasChange() = default;

            void execute(PaintingOperationReplay& replayer) override
            {
                Cairo::State::setShouldAntialias(contextForReplay(replayer), arg<0>());
            }

            void dump(TextStream& ts) override
            {
                ts << indent << "ShouldAntialiasChange<>\n";
            }
        };

        append(createCommand<ShouldAntialiasChange>(state.shouldAntialias));
    }
}

void CairoOperationRecorder::setLineCap(LineCap lineCap)
{
    struct SetLineCap final : PaintingOperation, OperationData<LineCap> {
        virtual ~SetLineCap() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::setLineCap(contextForReplay(replayer), arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "SetLineCap<>\n";
        }
    };

    append(createCommand<SetLineCap>(lineCap));
}

void CairoOperationRecorder::setLineDash(const DashArray& dashes, float dashOffset)
{
    struct SetLineDash final : PaintingOperation, OperationData<DashArray, float> {
        virtual ~SetLineDash() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::setLineDash(contextForReplay(replayer), arg<0>(), arg<1>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "SetLineDash<>\n";
        }
    };

    append(createCommand<SetLineDash>(dashes, dashOffset));
}

void CairoOperationRecorder::setLineJoin(LineJoin lineJoin)
{
    struct SetLineJoin final : PaintingOperation, OperationData<LineJoin> {
        virtual ~SetLineJoin() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::setLineJoin(contextForReplay(replayer), arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "SetLineJoin<>\n";
        }
    };

    append(createCommand<SetLineJoin>(lineJoin));
}

void CairoOperationRecorder::setMiterLimit(float miterLimit)
{
    struct SetMiterLimit final : PaintingOperation, OperationData<float> {
        virtual ~SetMiterLimit() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::setMiterLimit(contextForReplay(replayer), arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "SetMiterLimit<>\n";
        }
    };

    append(createCommand<SetMiterLimit>(miterLimit));
}

void CairoOperationRecorder::fillRect(const FloatRect& rect)
{
    struct FillRect final : PaintingOperation, OperationData<FloatRect, Cairo::FillSource, Cairo::ShadowState> {
        virtual ~FillRect() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::fillRect(contextForReplay(replayer), arg<0>(), arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillRect<>\n";
        }
    };

    auto& state = this->state();
    append(createCommand<FillRect>(rect, Cairo::FillSource(state), Cairo::ShadowState(state)));
}

void CairoOperationRecorder::fillRect(const FloatRect& rect, const Color& color)
{
    struct FillRect final : PaintingOperation, OperationData<FloatRect, Color, Cairo::ShadowState> {
        virtual ~FillRect() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::fillRect(contextForReplay(replayer), arg<0>(), arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillRect<>\n";
        }
    };

    append(createCommand<FillRect>(rect, color, Cairo::ShadowState(state())));
}

void CairoOperationRecorder::fillRect(const FloatRect& rect, Gradient& gradient)
{
    struct FillRect final : PaintingOperation, OperationData<FloatRect, RefPtr<cairo_pattern_t>> {
        virtual ~FillRect() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            auto& platformContext = contextForReplay(replayer);
            platformContext.save();
            Cairo::fillRect(platformContext, arg<0>(), arg<1>().get());
            platformContext.restore();
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillRect<>\n";
        }
    };

    auto& state = this->state();
    append(createCommand<FillRect>(rect, gradient.createPattern(1.0, state.fillGradientSpaceTransform)));
}

void CairoOperationRecorder::fillRect(const FloatRect& rect, const Color& color, CompositeOperator compositeOperator, BlendMode blendMode)
{
    struct FillRect final : PaintingOperation, OperationData<FloatRect, Color, CompositeOperator, BlendMode, Cairo::ShadowState, CompositeOperator> {
        virtual ~FillRect() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            auto& platformContext = contextForReplay(replayer);

            Cairo::State::setCompositeOperation(platformContext, arg<2>(), arg<3>());
            Cairo::fillRect(platformContext, arg<0>(), arg<1>(), arg<4>());
            Cairo::State::setCompositeOperation(platformContext, arg<5>(), BlendMode::Normal);
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillRect<>\n";
        }
    };

    auto& state = this->state();
    append(createCommand<FillRect>(rect, color, compositeOperator, blendMode, Cairo::ShadowState(state), state.compositeOperator));
}

void CairoOperationRecorder::fillRoundedRect(const FloatRoundedRect& roundedRect, const Color& color, BlendMode blendMode)
{
    struct FillRoundedRect final : PaintingOperation, OperationData<FloatRoundedRect, Color, CompositeOperator, BlendMode, Cairo::ShadowState> {
        virtual ~FillRoundedRect() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            auto& platformContext = contextForReplay(replayer);

            Cairo::State::setCompositeOperation(platformContext, arg<2>(), arg<3>());

            auto& rect = arg<0>();
            if (rect.isRounded())
                Cairo::fillRoundedRect(platformContext, rect, arg<1>(), arg<4>());
            else
                Cairo::fillRect(platformContext, rect.rect(), arg<1>(), arg<4>());

            Cairo::State::setCompositeOperation(platformContext, arg<2>(), BlendMode::Normal);
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillRoundedRect<>\n";
        }
    };

    auto& state = this->state();
    append(createCommand<FillRoundedRect>(roundedRect, color, state.compositeOperator, blendMode, Cairo::ShadowState(state)));
}

void CairoOperationRecorder::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    struct FillRectWithRoundedHole final : PaintingOperation, OperationData<FloatRect, FloatRoundedRect, Cairo::ShadowState> {
        virtual ~FillRectWithRoundedHole() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::fillRectWithRoundedHole(contextForReplay(replayer), arg<0>(), arg<1>(), { }, arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillRectWithRoundedHole<>\n";
        }
    };

    UNUSED_PARAM(color);
    append(createCommand<FillRectWithRoundedHole>(rect, roundedHoleRect, Cairo::ShadowState(state())));
}

void CairoOperationRecorder::fillPath(const Path& path)
{
    struct FillPath final : PaintingOperation, OperationData<Path, Cairo::FillSource, Cairo::ShadowState> {
        virtual ~FillPath() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::fillPath(contextForReplay(replayer), arg<0>(), arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillPath<>\n";
        }
    };

    if (path.isEmpty())
        return;

    auto& state = this->state();
    append(createCommand<FillPath>(path, Cairo::FillSource(state), Cairo::ShadowState(state)));
}

void CairoOperationRecorder::fillEllipse(const FloatRect& rect)
{
    struct FillEllipse final : PaintingOperation, OperationData<FloatRect, Cairo::FillSource, Cairo::ShadowState> {
        virtual ~FillEllipse() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Path path;
            path.addEllipse(arg<0>());
            Cairo::fillPath(contextForReplay(replayer), path, arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillEllipse<>\n";
        }
    };

    auto& state = this->state();
    append(createCommand<FillEllipse>(rect, Cairo::FillSource(state), Cairo::ShadowState(state)));
}

void CairoOperationRecorder::strokeRect(const FloatRect& rect, float lineWidth)
{
    struct StrokeRect final : PaintingOperation, OperationData<FloatRect, float, Cairo::StrokeSource, Cairo::ShadowState> {
        virtual ~StrokeRect() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::strokeRect(contextForReplay(replayer), arg<0>(), arg<1>(), arg<2>(), arg<3>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "StrokeRect<>\n";
        }
    };

    auto& state = this->state();
    append(createCommand<StrokeRect>(rect, lineWidth, Cairo::StrokeSource(state), Cairo::ShadowState(state)));
}

void CairoOperationRecorder::strokePath(const Path& path)
{
    struct StrokePath final : PaintingOperation, OperationData<Path, Cairo::StrokeSource, Cairo::ShadowState> {
        virtual ~StrokePath() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::strokePath(contextForReplay(replayer), arg<0>(), arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "StrokePath<>\n";
        }
    };

    if (path.isEmpty())
        return;

    auto& state = this->state();
    append(createCommand<StrokePath>(path, Cairo::StrokeSource(state), Cairo::ShadowState(state)));
}

void CairoOperationRecorder::strokeEllipse(const FloatRect& rect)
{
    struct StrokeEllipse final : PaintingOperation, OperationData<FloatRect, Cairo::StrokeSource, Cairo::ShadowState> {
        virtual ~StrokeEllipse() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Path path;
            path.addEllipse(arg<0>());
            Cairo::strokePath(contextForReplay(replayer), path, arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "StrokeEllipse<>\n";
        }
    };

    auto& state = this->state();
    append(createCommand<StrokeEllipse>(rect, Cairo::StrokeSource(state), Cairo::ShadowState(state)));
}

void CairoOperationRecorder::clearRect(const FloatRect& rect)
{
    struct ClearRect final : PaintingOperation, OperationData<FloatRect> {
        virtual ~ClearRect() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::clearRect(contextForReplay(replayer), arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "ClearRect<>\n";
        }
    };

    append(createCommand<ClearRect>(rect));
}

void CairoOperationRecorder::drawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& point, FontSmoothingMode fontSmoothing)
{
    struct DrawGlyphs final : PaintingOperation, OperationData<Cairo::FillSource, Cairo::StrokeSource, Cairo::ShadowState, FloatPoint, RefPtr<cairo_scaled_font_t>, float, Vector<cairo_glyph_t>, float, TextDrawingModeFlags, float, FloatSize, Color, FontSmoothingMode> {
        virtual ~DrawGlyphs() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::drawGlyphs(contextForReplay(replayer), arg<0>(), arg<1>(), arg<2>(), arg<3>(), arg<4>().get(),
                arg<5>(), arg<6>(), arg<7>(), arg<8>(), arg<9>(), arg<10>(), arg<11>(), arg<12>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawGlyphs<>\n";
        }
    };

    if (!font.platformData().size())
        return;

    auto xOffset = point.x();
    Vector<cairo_glyph_t> cairoGlyphs(numGlyphs);
    {
        auto yOffset = point.y();
        for (size_t i = 0; i < numGlyphs; ++i) {
            cairoGlyphs[i] = { glyphs[i], xOffset, yOffset };
            xOffset += advances[i].width();
        }
    }

    auto& state = this->state();
    append(createCommand<DrawGlyphs>(Cairo::FillSource(state), Cairo::StrokeSource(state),
        Cairo::ShadowState(state), point,
        RefPtr<cairo_scaled_font_t>(font.platformData().scaledFont()),
        font.syntheticBoldOffset(), WTFMove(cairoGlyphs), xOffset, state.textDrawingMode,
        state.strokeThickness, state.shadowOffset, state.shadowColor, fontSmoothing));
}

void CairoOperationRecorder::drawImageBuffer(ImageBuffer&, const FloatRect&, const FloatRect&, const ImagePaintingOptions&)
{
    // FIXME: Not implemented.
}

void CairoOperationRecorder::drawNativeImage(NativeImage& nativeImage, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    struct DrawNativeImage final : PaintingOperation, OperationData<RefPtr<cairo_surface_t>, FloatRect, FloatRect, ImagePaintingOptions, float, Cairo::ShadowState> {
        virtual ~DrawNativeImage() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::drawPlatformImage(contextForReplay(replayer), arg<0>().get(), arg<1>(), arg<2>(), arg<3>(), arg<4>(), arg<5>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawNativeImage<>\n";
        }
    };

    UNUSED_PARAM(imageSize);
    auto& state = this->state();
    append(createCommand<DrawNativeImage>(nativeImage.platformImage(), destRect, srcRect, ImagePaintingOptions(options, state.imageInterpolationQuality), state.alpha, Cairo::ShadowState(state)));
}

void CairoOperationRecorder::drawPattern(NativeImage& nativeImage, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    struct DrawPattern final : PaintingOperation, OperationData<RefPtr<cairo_surface_t>, IntSize, FloatRect, FloatRect, AffineTransform, FloatPoint, ImagePaintingOptions> {
        virtual ~DrawPattern() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::drawPattern(contextForReplay(replayer), arg<0>().get(), arg<1>(), arg<2>(), arg<3>(), arg<4>(), arg<5>(), arg<6>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawPattern<>\n";
        }
    };

    UNUSED_PARAM(spacing);
    append(createCommand<DrawPattern>(nativeImage.platformImage(), IntSize(imageSize), destRect, tileRect, patternTransform, phase, options));
}

void CairoOperationRecorder::drawRect(const FloatRect& rect, float borderThickness)
{
    struct DrawRect final : PaintingOperation, OperationData<FloatRect, float, Color, StrokeStyle, Color> {
        virtual ~DrawRect() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::drawRect(contextForReplay(replayer), arg<0>(), arg<1>(), arg<2>(), arg<3>(), arg<4>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawRect<>\n";
        }
    };

    auto& state = this->state();
    append(createCommand<DrawRect>(rect, borderThickness, state.fillColor, state.strokeStyle, state.strokeColor));
}

void CairoOperationRecorder::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    struct DrawLine final : PaintingOperation, OperationData<FloatPoint, FloatPoint, StrokeStyle, Color, float, bool> {
        virtual ~DrawLine() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::drawLine(contextForReplay(replayer), arg<0>(), arg<1>(), arg<2>(), arg<3>(), arg<4>(), arg<5>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawLine<>\n";
        }
    };

    if (strokeStyle() == NoStroke)
        return;

    auto& state = this->state();
    append(createCommand<DrawLine>(point1, point2, state.strokeStyle, state.strokeColor, state.strokeThickness, state.shouldAntialias));
}

void CairoOperationRecorder::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleUnderlines, StrokeStyle)
{
    struct DrawLinesForText final : PaintingOperation, OperationData<FloatPoint, float, DashArray, bool, bool, Color> {
        virtual ~DrawLinesForText() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::drawLinesForText(contextForReplay(replayer), arg<0>(), arg<1>(), arg<2>(), arg<3>(), arg<4>(), arg<5>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawLinesForText<>\n";
        }
    };

    if (widths.isEmpty())
        return;

    auto& state = this->state();
    append(createCommand<DrawLinesForText>(point, thickness, widths, printing, doubleUnderlines, state.strokeColor));
}

void CairoOperationRecorder::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    struct DrawDotsForDocumentMarker final : PaintingOperation, OperationData<FloatRect, DocumentMarkerLineStyle> {
        virtual ~DrawDotsForDocumentMarker() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::drawDotsForDocumentMarker(contextForReplay(replayer), arg<0>(), arg<1>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawDotsForDocumentMarker<>\n";
        }
    };

    append(createCommand<DrawDotsForDocumentMarker>(rect, style));
}

void CairoOperationRecorder::drawEllipse(const FloatRect& rect)
{
    struct DrawEllipse final : PaintingOperation, OperationData<FloatRect, Color, StrokeStyle, Color, float> {
        virtual ~DrawEllipse() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::drawEllipse(contextForReplay(replayer), arg<0>(), arg<1>(), arg<2>(), arg<3>(), arg<4>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawEllipse<>\n";
        }
    };

    auto& state = this->state();
    append(createCommand<DrawEllipse>(rect, state.fillColor, state.strokeStyle, state.strokeColor, state.strokeThickness));
}

void CairoOperationRecorder::drawFocusRing(const Path& path, float width, float offset, const Color& color)
{
#if PLATFORM(WPE) || PLATFORM(GTK)
    ThemeAdwaita::paintFocus(*this, path, color);
    UNUSED_PARAM(width);
#else
    struct DrawFocusRing final : PaintingOperation, OperationData<Path, float, Color> {
        virtual ~DrawFocusRing() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::drawFocusRing(contextForReplay(replayer), arg<0>(), arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawFocusRing<>\n";
        }
    };

    append(createCommand<DrawFocusRing>(path, width, color));
#endif
    UNUSED_PARAM(offset);
}

void CairoOperationRecorder::drawFocusRing(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
#if PLATFORM(WPE) || PLATFORM(GTK)
    ThemeAdwaita::paintFocus(*this, rects, color);
    UNUSED_PARAM(width);
#else
    struct DrawFocusRing final : PaintingOperation, OperationData<Vector<FloatRect>, float, Color> {
        virtual ~DrawFocusRing() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::drawFocusRing(contextForReplay(replayer), arg<0>(), arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawFocusRing<>\n";
        }
    };

    append(createCommand<DrawFocusRing>(rects, width, color));
#endif
    UNUSED_PARAM(offset);
}

void CairoOperationRecorder::save()
{
    struct Save final : PaintingOperation, OperationData<> {
        virtual ~Save() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            contextForReplay(replayer).save();
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "Save<>\n";
        }
    };

    GraphicsContext::save();

    append(createCommand<Save>());

    m_stateStack.append(m_stateStack.last());
}

void CairoOperationRecorder::restore()
{
    struct Restore final : PaintingOperation, OperationData<> {
        virtual ~Restore() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            contextForReplay(replayer).restore();
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "Restore<>\n";
        }
    };

    if (!stackSize())
        return;

    GraphicsContext::restore();

    if (m_stateStack.isEmpty())
        return;

    append(createCommand<Restore>());

    m_stateStack.removeLast();
    if (m_stateStack.isEmpty())
        m_stateStack.clear();
}

void CairoOperationRecorder::translate(float x, float y)
{
    struct Translate final : PaintingOperation, OperationData<float, float> {
        virtual ~Translate() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::translate(contextForReplay(replayer), arg<0>(), arg<1>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "Translate<>\n";
        }
    };

    append(createCommand<Translate>(x, y));

    {
        auto& state = m_stateStack.last();
        state.ctm.translate(x, y);

        AffineTransform t;
        t.translate(-x, -y);
        state.ctmInverse = t * state.ctmInverse;
    }
}

void CairoOperationRecorder::rotate(float angleInRadians)
{
    struct Rotate final : PaintingOperation, OperationData<float> {
        virtual ~Rotate() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::rotate(contextForReplay(replayer), arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "Rotate<>\n";
        }
    };

    append(createCommand<Rotate>(angleInRadians));

    {
        auto& state = m_stateStack.last();
        state.ctm.rotate(angleInRadians);

        AffineTransform t;
        t.rotate(angleInRadians);
        state.ctmInverse = t * state.ctmInverse;
    }
}

void CairoOperationRecorder::scale(const FloatSize& size)
{
    struct Scale final : PaintingOperation, OperationData<FloatSize> {
        virtual ~Scale() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::scale(contextForReplay(replayer), arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "Scale<>\n";
        }
    };

    append(createCommand<Scale>(size));

    {
        auto& state = m_stateStack.last();
        state.ctm.scale(size.width(), size.height());

        AffineTransform t;
        t.scale(1 / size.width(), 1 / size.height());
        state.ctmInverse = t * state.ctmInverse;
    }
}

void CairoOperationRecorder::concatCTM(const AffineTransform& transform)
{
    struct ConcatCTM final : PaintingOperation, OperationData<AffineTransform> {
        virtual ~ConcatCTM() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::concatCTM(contextForReplay(replayer), arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "ConcatCTM<>\n";
        }
    };

    auto inverse = transform.inverse();
    if (!inverse)
        return;

    append(createCommand<ConcatCTM>(transform));

    auto& state = m_stateStack.last();
    state.ctm *= transform;
    state.ctmInverse = inverse.value() * state.ctmInverse;
}

void CairoOperationRecorder::setCTM(const AffineTransform& transform)
{
    struct SetCTM final : PaintingOperation, OperationData<AffineTransform> {
        virtual ~SetCTM() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::State::setCTM(contextForReplay(replayer), arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "SetCTM<>\n";
        }
    };

    auto inverse = transform.inverse();
    if (!inverse)
        return;

    append(createCommand<SetCTM>(transform));

    auto& state = m_stateStack.last();
    state.ctm = transform;
    state.ctmInverse = inverse.value();
}

AffineTransform CairoOperationRecorder::getCTM(GraphicsContext::IncludeDeviceScale) const
{
    return m_stateStack.last().ctm;
}

void CairoOperationRecorder::beginTransparencyLayer(float opacity)
{
    struct BeginTransparencyLayer final : PaintingOperation, OperationData<float> {
        virtual ~BeginTransparencyLayer() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::beginTransparencyLayer(contextForReplay(replayer), arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "BeginTransparencyLayer<>\n";
        }
    };

    GraphicsContext::beginTransparencyLayer(opacity);

    append(createCommand<BeginTransparencyLayer>(opacity));
}

void CairoOperationRecorder::endTransparencyLayer()
{
    struct EndTransparencyLayer final : PaintingOperation, OperationData<> {
        virtual ~EndTransparencyLayer() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::endTransparencyLayer(contextForReplay(replayer));
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "EndTransparencyLayer<>\n";
        }
    };

    GraphicsContext::endTransparencyLayer();

    append(createCommand<EndTransparencyLayer>());
}

void CairoOperationRecorder::clip(const FloatRect& rect)
{
    struct Clip final : PaintingOperation, OperationData<FloatRect> {
        virtual ~Clip() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::clip(contextForReplay(replayer), arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "Clip<>\n";
        }
    };

    append(createCommand<Clip>(rect));

    {
        auto& state = m_stateStack.last();
        state.clipBounds.intersect(state.ctm.mapRect(rect));
    }
}

void CairoOperationRecorder::clipOut(const FloatRect& rect)
{
    struct ClipOut final : PaintingOperation, OperationData<FloatRect> {
        virtual ~ClipOut() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::clipOut(contextForReplay(replayer), arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "ClipOut<>\n";
        }
    };

    append(createCommand<ClipOut>(rect));
}

void CairoOperationRecorder::clipOut(const Path& path)
{
    struct ClipOut final : PaintingOperation, OperationData<Path> {
        virtual ~ClipOut() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::clipOut(contextForReplay(replayer), arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "ClipOut<>\n";
        }
    };

    append(createCommand<ClipOut>(path));
}

void CairoOperationRecorder::clipPath(const Path& path, WindRule clipRule)
{
    struct ClipPath final : PaintingOperation, OperationData<Path, WindRule> {
        virtual ~ClipPath() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::clipPath(contextForReplay(replayer), arg<0>(), arg<1>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "ClipPath<>\n";
        }
    };

    append(createCommand<ClipPath>(path, clipRule));

    {
        auto& state = m_stateStack.last();
        state.clipBounds.intersect(state.ctm.mapRect(path.fastBoundingRect()));
    }
}

IntRect CairoOperationRecorder::clipBounds() const
{
    auto& state = m_stateStack.last();
    return enclosingIntRect(state.ctmInverse.mapRect(state.clipBounds));
}

void CairoOperationRecorder::clipToImageBuffer(ImageBuffer& buffer, const FloatRect& destRect)
{
    struct ClipToImageBuffer final: PaintingOperation, OperationData<RefPtr<cairo_surface_t>, FloatRect> {
        virtual ~ClipToImageBuffer() = default;

        void execute(PaintingOperationReplay& replayer) override
        {
            Cairo::clipToImageBuffer(contextForReplay(replayer), arg<0>().get(), arg<1>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "ClipToImageBuffer<>\n";
        }
    };

    RefPtr<Image> image = buffer.copyImage(DontCopyBackingStore);
    if (!image)
        return;

    if (auto nativeImage = image->nativeImageForCurrentFrame())
        append(createCommand<ClipToImageBuffer>(nativeImage->platformImage(), destRect));
}

void CairoOperationRecorder::applyDeviceScaleFactor(float)
{
}

FloatRect CairoOperationRecorder::roundToDevicePixels(const FloatRect& rect, GraphicsContext::RoundingMode)
{
    return rect;
}

void CairoOperationRecorder::append(std::unique_ptr<PaintingOperation>&& command)
{
    m_commandList.append(WTFMove(command));
}

GraphicsContext::ClipToDrawingCommandsResult CairoOperationRecorder::clipToDrawingCommands(const FloatRect&, const DestinationColorSpace&, Function<void(GraphicsContext&)>&&)
{
    // FIXME: Not implemented.
    return ClipToDrawingCommandsResult::Success;
}

#if ENABLE(VIDEO)
void CairoOperationRecorder::paintFrameForMedia(MediaPlayer& player, const FloatRect& destination)
{
    // FIXME: Not implemented.
    GraphicsContext::paintFrameForMedia(player, destination);
}
#endif

} // namespace Nicosia
