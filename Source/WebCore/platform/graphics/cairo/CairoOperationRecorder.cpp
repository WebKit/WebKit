/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L.
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "CairoOperationRecorder.h"

#if USE(CAIRO)
#include "CairoOperations.h"
#include "Filter.h"
#include "FilterResults.h"
#include "FloatRoundedRect.h"
#include "Gradient.h"
#include "GraphicsContextCairo.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include <type_traits>
#include <wtf/ZippedRange.h>
#include <wtf/text/TextStream.h>

#if USE(THEME_ADWAITA)
#include "Adwaita.h"
#endif

namespace WebCore {
namespace Cairo {

template<typename... Args>
struct OperationData {
    template<std::size_t I>
    auto arg() const -> std::tuple_element_t<I, const std::tuple<Args...>>& { return std::get<I>(arguments); }

    std::tuple<Args...> arguments;
};

template<> struct OperationData<> { };

template<typename T, typename... Args>
    requires std::derived_from<T, OperationData<std::decay_t<Args>...>>
auto createCommand(Args&&... arguments) -> std::unique_ptr<PaintingOperation>
{
    auto* command = new T();
    command->arguments = std::make_tuple(std::forward<Args>(arguments)...);
    return std::unique_ptr<PaintingOperation>(command);
}

template<typename T>
    requires std::derived_from<T, OperationData<>>
auto createCommand() -> std::unique_ptr<PaintingOperation>
{
    return makeUnique<T>();
}

OperationRecorder::OperationRecorder(PaintingOperations& commandList)
    : m_commandList(commandList)
{
    m_stateStack.append({ { }, { }, FloatRect::infiniteRect() });
}

void OperationRecorder::didUpdateState(GraphicsContextState& state)
{
    if (state.changes().contains(GraphicsContextState::Change::StrokeThickness)) {
        struct StrokeThicknessChange final : PaintingOperation, OperationData<float> {
            virtual ~StrokeThicknessChange() = default;

            void execute(WebCore::GraphicsContextCairo& context) override
            {
                Cairo::State::setStrokeThickness(context, arg<0>());
            }

            void dump(TextStream& ts) override
            {
                ts << indent << "StrokeThicknessChange<>\n"_s;
            }
        };

        append(createCommand<StrokeThicknessChange>(state.strokeThickness()));
    }

    if (state.changes().contains(GraphicsContextState::Change::StrokeStyle)) {
        struct StrokeStyleChange final : PaintingOperation, OperationData<StrokeStyle> {
            virtual ~StrokeStyleChange() = default;

            void execute(WebCore::GraphicsContextCairo& context) override
            {
                Cairo::State::setStrokeStyle(context, arg<0>());
            }

            void dump(TextStream& ts) override
            {
                ts << indent << "StrokeStyleChange<>\n"_s;
            }
        };

        append(createCommand<StrokeStyleChange>(state.strokeStyle()));
    }

    if (state.changes().contains(GraphicsContextState::Change::CompositeMode)) {
        struct CompositeOperationChange final : PaintingOperation, OperationData<CompositeOperator, BlendMode> {
            virtual ~CompositeOperationChange() = default;

            void execute(WebCore::GraphicsContextCairo& context) override
            {
                Cairo::State::setCompositeOperation(context, arg<0>(), arg<1>());
            }

            void dump(TextStream& ts) override
            {
                ts << indent << "CompositeOperationChange<>\n"_s;
            }
        };

        append(createCommand<CompositeOperationChange>(state.compositeMode().operation, state.compositeMode().blendMode));
    }

    if (state.changes().contains(GraphicsContextState::Change::ShouldAntialias)) {
        struct ShouldAntialiasChange final : PaintingOperation, OperationData<bool> {
            virtual ~ShouldAntialiasChange() = default;

            void execute(WebCore::GraphicsContextCairo& context) override
            {
                Cairo::State::setShouldAntialias(context, arg<0>());
            }

            void dump(TextStream& ts) override
            {
                ts << indent << "ShouldAntialiasChange<>\n"_s;
            }
        };

        append(createCommand<ShouldAntialiasChange>(state.shouldAntialias()));
    }

    state.didApplyChanges();
}

void OperationRecorder::setLineCap(LineCap lineCap)
{
    struct SetLineCap final : PaintingOperation, OperationData<LineCap> {
        virtual ~SetLineCap() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::setLineCap(context, arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "SetLineCap<>\n"_s;
        }
    };

    append(createCommand<SetLineCap>(lineCap));
}

void OperationRecorder::setLineDash(const DashArray& dashes, float dashOffset)
{
    struct SetLineDash final : PaintingOperation, OperationData<DashArray, float> {
        virtual ~SetLineDash() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::setLineDash(context, arg<0>(), arg<1>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "SetLineDash<>\n"_s;
        }
    };

    append(createCommand<SetLineDash>(dashes, dashOffset));
}

void OperationRecorder::setLineJoin(LineJoin lineJoin)
{
    struct SetLineJoin final : PaintingOperation, OperationData<LineJoin> {
        virtual ~SetLineJoin() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::setLineJoin(context, arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "SetLineJoin<>\n"_s;
        }
    };

    append(createCommand<SetLineJoin>(lineJoin));
}

void OperationRecorder::setMiterLimit(float miterLimit)
{
    struct SetMiterLimit final : PaintingOperation, OperationData<float> {
        virtual ~SetMiterLimit() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::setMiterLimit(context, arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "SetMiterLimit<>\n"_s;
        }
    };

    append(createCommand<SetMiterLimit>(miterLimit));
}

void OperationRecorder::fillRect(const FloatRect& rect, RequiresClipToRect)
{
    struct FillRect final : PaintingOperation, OperationData<FloatRect, Cairo::FillSource, Cairo::ShadowState> {
        virtual ~FillRect() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::fillRect(context, arg<0>(), arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillRect<>\n"_s;
        }
    };

    auto& state = this->state();
    append(createCommand<FillRect>(rect, Cairo::FillSource(state), Cairo::ShadowState(state)));
}

void OperationRecorder::fillRect(const FloatRect& rect, const Color& color)
{
    struct FillRect final : PaintingOperation, OperationData<FloatRect, Color, Cairo::ShadowState> {
        virtual ~FillRect() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::fillRect(context, arg<0>(), arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillRect<>\n"_s;
        }
    };

    append(createCommand<FillRect>(rect, color, Cairo::ShadowState(state())));
}

void OperationRecorder::fillRect(const FloatRect& rect, Gradient& gradient)
{
    struct FillRect final : PaintingOperation, OperationData<FloatRect, RefPtr<cairo_pattern_t>> {
        virtual ~FillRect() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            auto& platformContext = context;
            platformContext.save();
            Cairo::fillRect(platformContext, arg<0>(), arg<1>().get());
            platformContext.restore();
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillRect<>\n"_s;
        }
    };

    auto& state = this->state();
    append(createCommand<FillRect>(rect, gradient.createPattern(1.0, state.fillBrush().gradientSpaceTransform())));
}

void OperationRecorder::fillRect(const FloatRect& rect, Gradient& gradient, const AffineTransform& gradientSpaceTransform, RequiresClipToRect)
{
    struct FillRect final : PaintingOperation, OperationData<FloatRect, Cairo::FillSource, Cairo::ShadowState> {
        virtual ~FillRect() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::fillRect(context, arg<0>(), arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillRect<>\n"_s;
        }
    };

    auto& state = this->state();
    append(createCommand<FillRect>(rect, Cairo::FillSource(state, gradient, gradientSpaceTransform), Cairo::ShadowState(state)));
}

void OperationRecorder::fillRect(const FloatRect& rect, const Color& color, CompositeOperator compositeOperator, BlendMode blendMode)
{
    struct FillRect final : PaintingOperation, OperationData<FloatRect, Color, CompositeOperator, BlendMode, Cairo::ShadowState, CompositeOperator> {
        virtual ~FillRect() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            auto& platformContext = context;

            Cairo::State::setCompositeOperation(platformContext, arg<2>(), arg<3>());
            Cairo::fillRect(platformContext, arg<0>(), arg<1>(), arg<4>());
            Cairo::State::setCompositeOperation(platformContext, arg<5>(), BlendMode::Normal);
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillRect<>\n"_s;
        }
    };

    auto& state = this->state();
    append(createCommand<FillRect>(rect, color, compositeOperator, blendMode, Cairo::ShadowState(state), state.compositeMode().operation));
}

void OperationRecorder::fillRoundedRect(const FloatRoundedRect& roundedRect, const Color& color, BlendMode blendMode)
{
    struct FillRoundedRect final : PaintingOperation, OperationData<FloatRoundedRect, Color, CompositeOperator, BlendMode, Cairo::ShadowState> {
        virtual ~FillRoundedRect() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            auto& platformContext = context;

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
            ts << indent << "FillRoundedRect<>\n"_s;
        }
    };

    auto& state = this->state();
    append(createCommand<FillRoundedRect>(roundedRect, color, state.compositeMode().operation, blendMode, Cairo::ShadowState(state)));
}

void OperationRecorder::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    struct FillRectWithRoundedHole final : PaintingOperation, OperationData<FloatRect, FloatRoundedRect, Cairo::ShadowState> {
        virtual ~FillRectWithRoundedHole() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::fillRectWithRoundedHole(context, arg<0>(), arg<1>(), { }, arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillRectWithRoundedHole<>\n"_s;
        }
    };

    UNUSED_PARAM(color);
    append(createCommand<FillRectWithRoundedHole>(rect, roundedHoleRect, Cairo::ShadowState(state())));
}

void OperationRecorder::fillPath(const Path& path)
{
    struct FillPath final : PaintingOperation, OperationData<Path, Cairo::FillSource, Cairo::ShadowState> {
        virtual ~FillPath() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::fillPath(context, arg<0>(), arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillPath<>\n"_s;
        }
    };

    if (path.isEmpty())
        return;

    auto& state = this->state();
    append(createCommand<FillPath>(path, Cairo::FillSource(state), Cairo::ShadowState(state)));
}

void OperationRecorder::fillEllipse(const FloatRect& rect)
{
    struct FillEllipse final : PaintingOperation, OperationData<FloatRect, Cairo::FillSource, Cairo::ShadowState> {
        virtual ~FillEllipse() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Path path;
            path.addEllipseInRect(arg<0>());
            Cairo::fillPath(context, path, arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "FillEllipse<>\n"_s;
        }
    };

    auto& state = this->state();
    append(createCommand<FillEllipse>(rect, Cairo::FillSource(state), Cairo::ShadowState(state)));
}

void OperationRecorder::strokeRect(const FloatRect& rect, float lineWidth)
{
    struct StrokeRect final : PaintingOperation, OperationData<FloatRect, float, Cairo::StrokeSource, Cairo::ShadowState> {
        virtual ~StrokeRect() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::strokeRect(context, arg<0>(), arg<1>(), arg<2>(), arg<3>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "StrokeRect<>\n"_s;
        }
    };

    auto& state = this->state();
    append(createCommand<StrokeRect>(rect, lineWidth, Cairo::StrokeSource(state), Cairo::ShadowState(state)));
}

void OperationRecorder::strokePath(const Path& path)
{
    struct StrokePath final : PaintingOperation, OperationData<Path, Cairo::StrokeSource, Cairo::ShadowState> {
        virtual ~StrokePath() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::strokePath(context, arg<0>(), arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "StrokePath<>\n"_s;
        }
    };

    if (path.isEmpty())
        return;

    auto& state = this->state();
    append(createCommand<StrokePath>(path, Cairo::StrokeSource(state), Cairo::ShadowState(state)));
}

void OperationRecorder::strokeEllipse(const FloatRect& rect)
{
    struct StrokeEllipse final : PaintingOperation, OperationData<FloatRect, Cairo::StrokeSource, Cairo::ShadowState> {
        virtual ~StrokeEllipse() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Path path;
            path.addEllipseInRect(arg<0>());
            Cairo::strokePath(context, path, arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "StrokeEllipse<>\n"_s;
        }
    };

    auto& state = this->state();
    append(createCommand<StrokeEllipse>(rect, Cairo::StrokeSource(state), Cairo::ShadowState(state)));
}

void OperationRecorder::clearRect(const FloatRect& rect)
{
    struct ClearRect final : PaintingOperation, OperationData<FloatRect> {
        virtual ~ClearRect() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::clearRect(context, arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "ClearRect<>\n"_s;
        }
    };

    append(createCommand<ClearRect>(rect));
}

void OperationRecorder::drawGlyphs(const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& point, FontSmoothingMode fontSmoothing)
{
    struct DrawGlyphs final : PaintingOperation, OperationData<Cairo::FillSource, Cairo::StrokeSource, Cairo::ShadowState, FloatPoint, RefPtr<cairo_scaled_font_t>, float, Vector<cairo_glyph_t>, float, TextDrawingModeFlags, float, std::optional<GraphicsDropShadow>, FontSmoothingMode> {
        virtual ~DrawGlyphs() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::drawGlyphs(context, arg<0>(), arg<1>(), arg<2>(), arg<3>(), arg<4>().get(),
                arg<5>(), arg<6>(), arg<7>(), arg<8>(), arg<9>(), arg<10>(), arg<11>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawGlyphs<>\n"_s;
        }
    };

    if (!font.platformData().size())
        return;

    auto xOffset = point.x();
    Vector<cairo_glyph_t> cairoGlyphs(glyphs.size());
    {
        auto yOffset = point.y();
        for (size_t i = 0; i < glyphs.size(); ++i) {
            cairoGlyphs[i] = { glyphs[i], xOffset, yOffset };
            xOffset += advances[i].width();
        }
    }

    auto& state = this->state();
    append(createCommand<DrawGlyphs>(Cairo::FillSource(state), Cairo::StrokeSource(state),
        Cairo::ShadowState(state), point,
        RefPtr<cairo_scaled_font_t>(font.platformData().scaledFont()),
        font.syntheticBoldOffset(), WTFMove(cairoGlyphs), xOffset, state.textDrawingMode(),
        state.strokeThickness(), state.dropShadow(), fontSmoothing));
}

void OperationRecorder::drawImageBuffer(ImageBuffer& buffer, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    struct DrawImageBuffer final : PaintingOperation, OperationData<RefPtr<cairo_surface_t>, FloatRect, FloatRect, ImagePaintingOptions, float, Cairo::ShadowState> {
        virtual ~DrawImageBuffer() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::drawPlatformImage(context, arg<0>().get(), arg<1>(), arg<2>(), arg<3>(), arg<4>(), arg<5>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawImageBuffer<>\n"_s;
        }
    };

    auto nativeImage = buffer.createNativeImageReference();
    if (!nativeImage)
        return;

    auto& state = this->state();
    append(createCommand<DrawImageBuffer>(nativeImage->platformImage(), destRect, srcRect, ImagePaintingOptions(options, state.imageInterpolationQuality()), state.alpha(), Cairo::ShadowState(state)));
}

void OperationRecorder::drawFilteredImageBuffer(ImageBuffer* srcImage, const FloatRect& srcRect, Filter& filter, FilterResults& results)
{
    struct DrawFilteredImageBuffer final : PaintingOperation, OperationData<RefPtr<cairo_surface_t>, FloatRect, FloatRect, FloatSize, ImagePaintingOptions, float, Cairo::ShadowState> {
        virtual ~DrawFilteredImageBuffer() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::scale(context, { 1 / arg<3>().width(), 1 / arg<3>().height() });
            Cairo::drawPlatformImage(context, arg<0>().get(), arg<1>(), arg<2>(), arg<4>(), arg<5>(), arg<6>());
            Cairo::scale(context, arg<3>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawFilteredImageBuffer<>\n"_s;
        }
    };

    auto result = filter.apply(srcImage, srcRect, results);
    if (!result)
        return;

    auto imageBuffer = result->imageBuffer();
    if (!imageBuffer)
        return;

    auto nativeImage = imageBuffer->createNativeImageReference();
    if (!nativeImage)
        return;

    auto& state = this->state();
    append(createCommand<DrawFilteredImageBuffer>(nativeImage->platformImage(), FloatRect(result->absoluteImageRect()), FloatRect({ } , imageBuffer->logicalSize()), filter.filterScale(), ImagePaintingOptions(state.imageInterpolationQuality()), state.alpha(), Cairo::ShadowState(state)));
}

void OperationRecorder::drawNativeImage(NativeImage& nativeImage, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    struct DrawNativeImage final : PaintingOperation, OperationData<RefPtr<cairo_surface_t>, FloatRect, FloatRect, ImagePaintingOptions, float, Cairo::ShadowState> {
        virtual ~DrawNativeImage() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::drawPlatformImage(context, arg<0>().get(), arg<1>(), arg<2>(), arg<3>(), arg<4>(), arg<5>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawNativeImage<>\n"_s;
        }
    };

    auto& state = this->state();
    append(createCommand<DrawNativeImage>(nativeImage.platformImage(), destRect, srcRect, ImagePaintingOptions(options, state.imageInterpolationQuality()), state.alpha(), Cairo::ShadowState(state)));
}

void OperationRecorder::drawPattern(NativeImage& nativeImage, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    struct DrawPattern final : PaintingOperation, OperationData<RefPtr<cairo_surface_t>, IntSize, FloatRect, FloatRect, AffineTransform, FloatPoint, FloatSize, ImagePaintingOptions> {
        virtual ~DrawPattern() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::drawPattern(context, arg<0>().get(), arg<1>(), arg<2>(), arg<3>(), arg<4>(), arg<5>(), arg<6>(), arg<7>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawPattern<>\n"_s;
        }
    };

    UNUSED_PARAM(spacing);
    append(createCommand<DrawPattern>(nativeImage.platformImage(), nativeImage.size(), destRect, tileRect, patternTransform, phase, spacing, options));
}

void OperationRecorder::drawRect(const FloatRect& rect, float borderThickness)
{
    struct DrawRect final : PaintingOperation, OperationData<FloatRect, float, Color, StrokeStyle, Color> {
        virtual ~DrawRect() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::drawRect(context, arg<0>(), arg<1>(), arg<2>(), arg<3>(), arg<4>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawRect<>\n"_s;
        }
    };

    auto& state = this->state();
    append(createCommand<DrawRect>(rect, borderThickness, state.fillBrush().color(), state.strokeStyle(), state.strokeBrush().color()));
}

void OperationRecorder::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    struct DrawLine final : PaintingOperation, OperationData<FloatPoint, FloatPoint, StrokeStyle, Color, float, bool> {
        virtual ~DrawLine() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::drawLine(context, arg<0>(), arg<1>(), arg<2>(), arg<3>(), arg<4>(), arg<5>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawLine<>\n"_s;
        }
    };

    if (strokeStyle() == StrokeStyle::NoStroke)
        return;

    auto& state = this->state();
    append(createCommand<DrawLine>(point1, point2, state.strokeStyle(), state.strokeBrush().color(), state.strokeThickness(), state.shouldAntialias()));
}

void OperationRecorder::drawLinesForText(const FloatPoint& point, float thickness, std::span<const FloatSegment> lineSegments, bool printing, bool doubleUnderlines, StrokeStyle)
{
    struct DrawLinesForText final : PaintingOperation, OperationData<FloatPoint, float, Vector<FloatSegment>, bool, bool, Color> {
        virtual ~DrawLinesForText() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::drawLinesForText(context, arg<0>(), arg<1>(), arg<2>(), arg<3>(), arg<4>(), arg<5>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawLinesForText<>\n"_s;
        }
    };

    if (lineSegments.empty())
        return;

    auto& state = this->state();
    Vector<FloatSegment> segmentVector { WTFMove(lineSegments) };
    append(createCommand<DrawLinesForText>(point, thickness, WTFMove(segmentVector), printing, doubleUnderlines, state.strokeBrush().color()));
}

void OperationRecorder::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    struct DrawDotsForDocumentMarker final : PaintingOperation, OperationData<FloatRect, DocumentMarkerLineStyle> {
        virtual ~DrawDotsForDocumentMarker() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::drawDotsForDocumentMarker(context, arg<0>(), arg<1>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawDotsForDocumentMarker<>\n"_s;
        }
    };

    append(createCommand<DrawDotsForDocumentMarker>(rect, style));
}

void OperationRecorder::drawEllipse(const FloatRect& rect)
{
    struct DrawEllipse final : PaintingOperation, OperationData<FloatRect, Color, StrokeStyle, Color, float> {
        virtual ~DrawEllipse() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::drawEllipse(context, arg<0>(), arg<1>(), arg<2>(), arg<3>(), arg<4>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawEllipse<>\n"_s;
        }
    };

    auto& state = this->state();
    append(createCommand<DrawEllipse>(rect, state.fillBrush().color(), state.strokeStyle(), state.strokeBrush().color(), state.strokeThickness()));
}

void OperationRecorder::drawFocusRing(const Path& path, float outlineWidth, const Color& color)
{
#if USE(THEME_ADWAITA)
    Adwaita::paintFocus(*this, path, color);
    UNUSED_PARAM(outlineWidth);
#else
    struct DrawFocusRing final : PaintingOperation, OperationData<Path, float, Color> {
        virtual ~DrawFocusRing() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::drawFocusRing(context, arg<0>(), arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawFocusRing<>\n"_s;
        }
    };

    append(createCommand<DrawFocusRing>(path, outlineWidth, color));
#endif
}

void OperationRecorder::drawFocusRing(const Vector<FloatRect>& rects, float outlineOffset, float outlineWidth, const Color& color)
{
#if USE(THEME_ADWAITA)
    Adwaita::paintFocus(*this, rects, color);
    UNUSED_PARAM(outlineWidth);
#else
    struct DrawFocusRing final : PaintingOperation, OperationData<Vector<FloatRect>, float, Color> {
        virtual ~DrawFocusRing() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::drawFocusRing(context, arg<0>(), arg<1>(), arg<2>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "DrawFocusRing<>\n"_s;
        }
    };

    append(createCommand<DrawFocusRing>(rects, outlineWidth, color));
#endif
    UNUSED_PARAM(outlineOffset);
}

void OperationRecorder::save(GraphicsContextState::Purpose purpose)
{
    struct Save final : PaintingOperation, OperationData<GraphicsContextState::Purpose> {
        virtual ~Save() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            context.save(arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "Save<>\n"_s;
        }
    };

    GraphicsContext::save(purpose);

    append(createCommand<Save>(purpose));

    m_stateStack.append(m_stateStack.last());
}

void OperationRecorder::restore(GraphicsContextState::Purpose purpose)
{
    struct Restore final : PaintingOperation, OperationData<GraphicsContextState::Purpose> {
        virtual ~Restore() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            context.restore(arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "Restore<>\n"_s;
        }
    };

    if (!stackSize())
        return;

    GraphicsContext::restore(purpose);

    if (m_stateStack.isEmpty())
        return;

    append(createCommand<Restore>(purpose));

    m_stateStack.removeLast();
    if (m_stateStack.isEmpty())
        m_stateStack.clear();
}

void OperationRecorder::translate(float x, float y)
{
    struct Translate final : PaintingOperation, OperationData<float, float> {
        virtual ~Translate() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::translate(context, arg<0>(), arg<1>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "Translate<>\n"_s;
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

void OperationRecorder::rotate(float angleInRadians)
{
    struct Rotate final : PaintingOperation, OperationData<float> {
        virtual ~Rotate() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::rotate(context, arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "Rotate<>\n"_s;
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

void OperationRecorder::scale(const FloatSize& size)
{
    struct Scale final : PaintingOperation, OperationData<FloatSize> {
        virtual ~Scale() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::scale(context, arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "Scale<>\n"_s;
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

void OperationRecorder::concatCTM(const AffineTransform& transform)
{
    struct ConcatCTM final : PaintingOperation, OperationData<AffineTransform> {
        virtual ~ConcatCTM() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::concatCTM(context, arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "ConcatCTM<>\n"_s;
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

void OperationRecorder::setCTM(const AffineTransform& transform)
{
    struct SetCTM final : PaintingOperation, OperationData<AffineTransform> {
        virtual ~SetCTM() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::State::setCTM(context, arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "SetCTM<>\n"_s;
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

AffineTransform OperationRecorder::getCTM(GraphicsContext::IncludeDeviceScale) const
{
    return m_stateStack.last().ctm;
}

void OperationRecorder::beginTransparencyLayer(float opacity)
{
    struct BeginTransparencyLayer final : PaintingOperation, OperationData<float> {
        virtual ~BeginTransparencyLayer() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::beginTransparencyLayer(context, arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "BeginTransparencyLayer<>\n"_s;
        }
    };

    GraphicsContext::beginTransparencyLayer(opacity);

    append(createCommand<BeginTransparencyLayer>(opacity));
}

void OperationRecorder::beginTransparencyLayer(CompositeOperator, BlendMode)
{
    beginTransparencyLayer(1);
}

void OperationRecorder::endTransparencyLayer()
{
    struct EndTransparencyLayer final : PaintingOperation, OperationData<> {
        virtual ~EndTransparencyLayer() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::endTransparencyLayer(context);
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "EndTransparencyLayer<>\n"_s;
        }
    };

    GraphicsContext::endTransparencyLayer();

    append(createCommand<EndTransparencyLayer>());
}

void OperationRecorder::resetClip()
{
    ASSERT_NOT_REACHED("resetClip is not supported on Cairo");
}

void OperationRecorder::clip(const FloatRect& rect)
{
    struct Clip final : PaintingOperation, OperationData<FloatRect> {
        virtual ~Clip() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::clip(context, arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "Clip<>\n"_s;
        }
    };

    append(createCommand<Clip>(rect));

    {
        auto& state = m_stateStack.last();
        state.clipBounds.intersect(state.ctm.mapRect(rect));
    }
}

void OperationRecorder::clipOut(const FloatRect& rect)
{
    struct ClipOut final : PaintingOperation, OperationData<FloatRect> {
        virtual ~ClipOut() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::clipOut(context, arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "ClipOut<>\n"_s;
        }
    };

    append(createCommand<ClipOut>(rect));
}

void OperationRecorder::clipOut(const Path& path)
{
    struct ClipOut final : PaintingOperation, OperationData<Path> {
        virtual ~ClipOut() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::clipOut(context, arg<0>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "ClipOut<>\n"_s;
        }
    };

    append(createCommand<ClipOut>(path));
}

void OperationRecorder::clipPath(const Path& path, WindRule clipRule)
{
    struct ClipPath final : PaintingOperation, OperationData<Path, WindRule> {
        virtual ~ClipPath() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::clipPath(context, arg<0>(), arg<1>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "ClipPath<>\n"_s;
        }
    };

    append(createCommand<ClipPath>(path, clipRule));

    {
        auto& state = m_stateStack.last();
        state.clipBounds.intersect(state.ctm.mapRect(path.fastBoundingRect()));
    }
}

IntRect OperationRecorder::clipBounds() const
{
    auto& state = m_stateStack.last();
    return enclosingIntRect(state.ctmInverse.mapRect(state.clipBounds));
}

void OperationRecorder::clipToImageBuffer(ImageBuffer& buffer, const FloatRect& destRect)
{
    struct ClipToImageBuffer final: PaintingOperation, OperationData<RefPtr<cairo_surface_t>, FloatRect> {
        virtual ~ClipToImageBuffer() = default;

        void execute(WebCore::GraphicsContextCairo& context) override
        {
            Cairo::clipToImageBuffer(context, arg<0>().get(), arg<1>());
        }

        void dump(TextStream& ts) override
        {
            ts << indent << "ClipToImageBuffer<>\n"_s;
        }
    };

    if (auto nativeImage = buffer.createNativeImageReference())
        append(createCommand<ClipToImageBuffer>(nativeImage->platformImage(), destRect));
}

void OperationRecorder::applyDeviceScaleFactor(float)
{
}

void OperationRecorder::append(std::unique_ptr<PaintingOperation>&& command)
{
    m_commandList.append(WTFMove(command));
}

#if ENABLE(VIDEO)
void OperationRecorder::drawVideoFrame(const VideoFrame& frame, const FloatRect& destination, ImageOrientation orientation, bool shouldDiscardAlpha)
{
    // FIXME: Not implemented.
    GraphicsContext::drawVideoFrame(frame, destination, orientation, shouldDiscardAlpha);
}
#endif

} // namespace Cairo
} // namespace WebCore

#endif // USE(CAIRO)
