/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Helpers/Test.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsContextState.h>

namespace TestWebKitAPI {
using namespace WebCore;

using Purpose = GraphicsContextState::Purpose;

// GraphicsContext::save() stores the values its restore() has to put back in a
// GraphicsContextStateStack. This context does not do anything else, so the state it exposes is the
// state the stack maintains. NullGraphicsContext cannot be used, since it makes save() and restore()
// no-ops.
class TestGraphicsContext final : public GraphicsContext {
public:
    TestGraphicsContext() = default;
    explicit TestGraphicsContext(const GraphicsContextState& state)
        : GraphicsContext(IsDeferred::No, state)
    {
    }

private:
    // The state stack requires that a state change has reached the platform context before the next
    // save(), and there is no platform context here.
    void didUpdateState(GraphicsContextState& state) final { state.didApplyChanges(); }

    void drawNativeImage(const NativeImage&, const FloatRect&, const FloatRect&, ImagePaintingOptions) final { }
    void drawPattern(const NativeImage&, const FloatRect&, const FloatRect&, const AffineTransform&, const FloatPoint&, const FloatSize&, ImagePaintingOptions) final { }
    void clipToImageBuffer(ImageBuffer&, const FloatRect&) final { }

#if USE(CG)
    void applyStrokePattern() final { }
    void applyFillPattern() final { }
    bool isCALayerContext() const final { return false; }
#endif

    void drawRect(const FloatRect&, float = 1) final { }
    void drawLine(const FloatPoint&, const FloatPoint&) final { }
    void drawEllipse(const FloatRect&) final { }
    void fillPath(const Path&) final { }
    void strokePath(const Path&) final { }
    void fillRect(const FloatRect&, RequiresClipToRect) final { }
    void fillRect(const FloatRect&, Gradient&, const AffineTransform&, RequiresClipToRect) final { }
    void fillRect(const FloatRect&, const Color&) final { }
    void fillRoundedRectImpl(const FloatRoundedRect&, const Color&) final { }
    void strokeRect(const FloatRect&, float) final { }
    void clipPath(const Path&, WindRule = WindRule::EvenOdd) final { }
    void drawLinesForText(const FloatPoint&, float, std::span<const FloatSegment>, bool, bool, StrokeStyle) final { }
    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) final { }
    void drawFocusRing(const Path&, float, const Color&, float) final { }
    void drawFocusRing(const Vector<FloatRect>&, float, const Color&, float) final { }
    void setLineCap(LineCap) final { }
    void setLineDash(const DashArray&, float) final { }
    void setLineJoin(LineJoin) final { }
    void setMiterLimit(float) final { }
    void clearRect(const FloatRect&) final { }
    void resetClip() final { }
    void clip(const FloatRect&) final { }
    void clipOut(const FloatRect&) final { }
    void clipOut(const Path&) final { }
    void scale(const FloatSize&) final { }
    void rotate(float) final { }
    void translate(float, float) final { }
    void concatCTM(const AffineTransform&) final { }
    void setCTM(const AffineTransform&) final { }
    AffineTransform getCTM(IncludeDeviceScale = PossiblyIncludeDeviceScale) const final { return { }; }
};

TEST(GraphicsContextStateStackTests, RestorePutsBackAPropertyTheLevelChanged)
{
    TestGraphicsContext context;
    context.setStrokeThickness(3);
    context.setFillColor(Color::red);

    context.save();
    EXPECT_EQ(1u, context.stackSize());
    context.setFillColor(Color::green);
    context.setStrokeThickness(7);
    context.restore();

    EXPECT_EQ(0u, context.stackSize());
    EXPECT_EQ(Color::red, context.fillColor());
    EXPECT_EQ(3.f, context.strokeThickness());
}

TEST(GraphicsContextStateStackTests, RestorePutsBackAPropertyNoLevelBelowChanged)
{
    TestGraphicsContext context;
    auto initialQuality = context.imageInterpolationQuality();
    EXPECT_NE(InterpolationQuality::Low, initialQuality);

    context.save();
    context.setImageInterpolationQuality(InterpolationQuality::Low);
    context.restore();

    EXPECT_EQ(initialQuality, context.imageInterpolationQuality());
}

TEST(GraphicsContextStateStackTests, RestoreLeavesAPropertyTheLevelDidNotChange)
{
    TestGraphicsContext context;
    context.setAlpha(0.25f);

    context.save();
    context.setStrokeThickness(5);
    context.restore();

    EXPECT_EQ(0.25f, context.alpha());
}

TEST(GraphicsContextStateStackTests, ChangingAPropertyToTheValueItHasIsRestored)
{
    TestGraphicsContext context;
    context.setAlpha(0.5f);

    context.save();
    context.setAlpha(0.5f);
    context.setAlpha(0.75f);
    context.restore();

    EXPECT_EQ(0.5f, context.alpha());
}

TEST(GraphicsContextStateStackTests, NestedLevelsAreRestoredInOrder)
{
    TestGraphicsContext context;
    context.setAlpha(0.1f);

    context.save();
    context.setAlpha(0.2f);
    context.save();
    context.setAlpha(0.3f);
    context.save();
    // This level changes nothing.
    EXPECT_EQ(3u, context.stackSize());

    context.restore();
    EXPECT_EQ(0.3f, context.alpha());
    context.restore();
    EXPECT_EQ(0.2f, context.alpha());
    context.restore();
    EXPECT_EQ(0.1f, context.alpha());
    EXPECT_EQ(0u, context.stackSize());
}

TEST(GraphicsContextStateStackTests, LevelsBelowAreRestoredAfterAnInnerLevelChangedTheSameProperty)
{
    TestGraphicsContext context;
    context.setAlpha(0.1f);

    context.save();
    // The level below changed the alpha, this one does not.
    context.save();
    context.setAlpha(0.9f);
    context.restore();
    EXPECT_EQ(0.1f, context.alpha());
    context.setAlpha(0.5f);
    context.restore();

    EXPECT_EQ(0.1f, context.alpha());
}

TEST(GraphicsContextStateStackTests, SuccessiveLevelsAreRestored)
{
    TestGraphicsContext context;

    for (unsigned i = 1; i <= 4; ++i) {
        context.setAlpha(1.f / i);
        context.save();
        context.setAlpha(0.f);
        context.restore();
        EXPECT_EQ(1.f / i, context.alpha());
    }
}

TEST(GraphicsContextStateStackTests, DeeplyNestedLevelsAreRestored)
{
    constexpr unsigned levels = 40;
    TestGraphicsContext context;

    for (unsigned i = 0; i < levels; ++i) {
        context.save();
        context.setStrokeThickness(i);
        context.setFillColor(i % 2 ? Color::red : Color::green);
    }
    EXPECT_EQ(levels, context.stackSize());
    for (unsigned i = levels; i--;) {
        EXPECT_EQ(static_cast<float>(i), context.strokeThickness());
        context.restore();
    }
    EXPECT_EQ(0u, context.stackSize());
    EXPECT_EQ(0.f, context.strokeThickness());
    EXPECT_EQ(Color::black, context.fillColor());
}

TEST(GraphicsContextStateStackTests, RestoreInAContextStartedFromAnExistingState)
{
    // A recording context is started from the state of the context it records for, which is that
    // state repurposed to Purpose::Initial: it has no changes left to apply.
    GraphicsContextState state;
    state.setAlpha(0.5f);
    state.didApplyChanges();

    TestGraphicsContext context { state };
    EXPECT_EQ(0.5f, context.alpha());

    context.save();
    context.setAlpha(0.25f);
    context.restore();

    EXPECT_EQ(0.5f, context.alpha());
}

TEST(GraphicsContextStateStackTests, RestorePutsBackThePurposeOfTheLevelBelow)
{
    TestGraphicsContext context;
    EXPECT_EQ(Purpose::Initial, context.state().purpose());

    context.save();
    EXPECT_EQ(Purpose::SaveRestore, context.state().purpose());
    context.save(Purpose::TransparencyLayer);
    EXPECT_EQ(Purpose::TransparencyLayer, context.state().purpose());

    context.restore(Purpose::TransparencyLayer);
    EXPECT_EQ(Purpose::SaveRestore, context.state().purpose());
    context.restore();
    EXPECT_EQ(Purpose::Initial, context.state().purpose());
}

TEST(GraphicsContextStateStackTests, RestoreOfATransparencyLayerLevelPutsBackTheResetProperties)
{
    TestGraphicsContext context;
    context.setAlpha(0.5f);
    context.setCompositeOperation(CompositeOperator::SourceIn);
    context.setDropShadow({ { 1, 2 }, 3, Color::red });

    context.save(Purpose::TransparencyLayer);
#if USE(CG)
    // CGContextBeginTransparencyLayer() resets these in the platform context.
    EXPECT_EQ(1.f, context.alpha());
    EXPECT_EQ(CompositeOperator::SourceOver, context.compositeOperation());
    EXPECT_FALSE(context.dropShadow().has_value());
#endif
    context.restore(Purpose::TransparencyLayer);

    EXPECT_EQ(0.5f, context.alpha());
    EXPECT_EQ(CompositeOperator::SourceIn, context.compositeOperation());
    ASSERT_TRUE(context.dropShadow().has_value());
    EXPECT_EQ(Color::red, context.dropShadow()->color);
}

// Three values per property, all different from each other and from the initial value, so that a
// property that is stored or restored as another one of the same type is caught.
struct EveryProperty {
    SourceBrush fillBrush;
    WindRule fillRule;
    SourceBrush strokeBrush;
    float strokeThickness;
    StrokeStyle strokeStyle;
    CompositeMode compositeMode;
    GraphicsDropShadow dropShadow;
    GraphicsDropShadow style;
    float alpha;
    InterpolationQuality imageInterpolationQuality;
    TextDrawingModeFlags textDrawingMode;
    bool shouldAntialias;
    bool shouldSmoothFonts;
    bool shouldSubpixelQuantizeFonts;
    bool shadowsIgnoreTransforms;
    bool drawLuminanceMask;
};

static EveryProperty firstValues()
{
    return {
        SourceBrush { Color::red }, WindRule::EvenOdd, SourceBrush { Color::green }, 2,
        StrokeStyle::DottedStroke, { CompositeOperator::SourceIn, BlendMode::Multiply },
        { { 1, 2 }, 3, Color::blue }, { { 4, 5 }, 6, Color::cyan }, 0.25f,
        InterpolationQuality::Low, TextDrawingMode::Stroke, false, false, false, true, true
    };
}

static EveryProperty secondValues()
{
    return {
        SourceBrush { Color::magenta }, WindRule::NonZero, SourceBrush { Color::yellow }, 8,
        StrokeStyle::DashedStroke, { CompositeOperator::DestinationOver, BlendMode::Screen },
        { { 7, 8 }, 9, Color::white }, { { 10, 11 }, 12, Color::gray }, 0.75f,
        InterpolationQuality::High, TextDrawingMode::Fill, true, true, true, false, false
    };
}

static EveryProperty thirdValues()
{
    return {
        SourceBrush { Color::gold }, WindRule::EvenOdd, SourceBrush { Color::purple }, 16,
        StrokeStyle::DoubleStroke, { CompositeOperator::DestinationIn, BlendMode::Darken },
        { { 13, 14 }, 15, Color::orange }, { { 16, 17 }, 18, Color::lightGray }, 0.5f,
        InterpolationQuality::Medium, TextDrawingMode::Stroke, false, false, false, true, true
    };
}

static void setEveryProperty(GraphicsContext& context, const EveryProperty& values)
{
    context.setFillBrush(values.fillBrush);
    context.setFillRule(values.fillRule);
    context.setStrokeBrush(values.strokeBrush);
    context.setStrokeThickness(values.strokeThickness);
    context.setStrokeStyle(values.strokeStyle);
    context.setCompositeMode(values.compositeMode);
    context.setDropShadow(values.dropShadow);
    context.setStyle(GraphicsStyle { values.style });
    context.setAlpha(values.alpha);
    context.setImageInterpolationQuality(values.imageInterpolationQuality);
    context.setTextDrawingMode(values.textDrawingMode);
    context.setShouldAntialias(values.shouldAntialias);
    context.setShouldSmoothFonts(values.shouldSmoothFonts);
    context.setShouldSubpixelQuantizeFonts(values.shouldSubpixelQuantizeFonts);
    context.setShadowsIgnoreTransforms(values.shadowsIgnoreTransforms);
    context.setDrawLuminanceMask(values.drawLuminanceMask);
}

static void expectEveryProperty(const GraphicsContext& context, const EveryProperty& values)
{
    EXPECT_EQ(values.fillBrush.color(), context.fillColor());
    EXPECT_EQ(values.fillRule, context.fillRule());
    EXPECT_EQ(values.strokeBrush.color(), context.strokeColor());
    EXPECT_EQ(values.strokeThickness, context.strokeThickness());
    EXPECT_EQ(values.strokeStyle, context.strokeStyle());
    EXPECT_EQ(values.compositeMode.operation, context.compositeOperation());
    EXPECT_EQ(values.compositeMode.blendMode, context.blendMode());
    ASSERT_TRUE(context.dropShadow().has_value());
    EXPECT_EQ(values.dropShadow, *context.dropShadow());
    ASSERT_TRUE(context.style().has_value());
    ASSERT_TRUE(std::holds_alternative<GraphicsDropShadow>(*context.style()));
    EXPECT_EQ(values.style, std::get<GraphicsDropShadow>(*context.style()));
    EXPECT_EQ(values.alpha, context.alpha());
    EXPECT_EQ(values.imageInterpolationQuality, context.imageInterpolationQuality());
    EXPECT_EQ(values.textDrawingMode, context.textDrawingMode());
    EXPECT_EQ(values.shouldAntialias, context.shouldAntialias());
    EXPECT_EQ(values.shouldSmoothFonts, context.shouldSmoothFonts());
    EXPECT_EQ(values.shouldSubpixelQuantizeFonts, context.shouldSubpixelQuantizeFonts());
    EXPECT_EQ(values.shadowsIgnoreTransforms, context.shadowsIgnoreTransforms());
    EXPECT_EQ(values.drawLuminanceMask, context.drawLuminanceMask());
}

TEST(GraphicsContextStateStackTests, RestorePutsBackEveryProperty)
{
    TestGraphicsContext context;

    // The first level restores from the values the first save() stored for every property, the
    // second one from the values its own save() pushed.
    setEveryProperty(context, firstValues());
    context.save();
    setEveryProperty(context, secondValues());
    context.save();
    setEveryProperty(context, thirdValues());

    expectEveryProperty(context, thirdValues());
    context.restore();
    expectEveryProperty(context, secondValues());
    context.restore();
    expectEveryProperty(context, firstValues());
}

}
