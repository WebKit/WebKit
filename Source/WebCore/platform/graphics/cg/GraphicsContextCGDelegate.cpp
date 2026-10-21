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

#include "config.h"
#include "GraphicsContextCGDelegate.h"

#if USE(CG)

#include "Color.h"
#include "GraphicsContext.h"
#include "Path.h"
#include "PathCG.h"
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GraphicsContextCGDelegate);

// The gstate CTM is absolute (see createCGContext()), so the target's CTM is set to it directly.
class GStateScope {
    WTF_MAKE_TZONE_ALLOCATED(GStateScope);
    WTF_MAKE_NONCOPYABLE(GStateScope);
public:
    enum class Paint : uint8_t {
        Fill = 1 << 0,
        Stroke = 1 << 1,
    };

    // Line widths and dashes stay in CG user space, without `userSpaceTransform`.
    GStateScope(GraphicsContextCGDelegate&, CGRenderingStateRef, CGGStateRef, OptionSet<Paint> = { }, const AffineTransform& userSpaceTransform = { });
    ~GStateScope();

    bool shouldDraw() const { return m_shouldDraw; }
    GraphicsContext& target() const { return m_delegate.target(); }
    const AffineTransform& ctm() const { return m_ctm; }

    static OptionSet<Paint> paintForDrawingMode(CGPathDrawingMode);
    static WindRule windRuleForDrawingMode(CGPathDrawingMode);

private:
    void initialize(CGRenderingStateRef, CGGStateRef);
    void applyClipStack(CGGStateRef);
    void applyCompositeOperation(CGGStateRef);
    void applyLineParameters(CGGStateRef);
    enum class BrushType : bool { Fill, Stroke };
    bool applyBrush(CGColorRef, BrushType);

    GraphicsContextCGDelegate& m_delegate;
    AffineTransform m_ctm;
    AffineTransform m_userSpaceTransform;
    float m_alpha { 1 };
    OptionSet<Paint> m_paint;
    bool m_shouldDraw { true };
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStateScope);

static InterpolationQuality interpolationQuality(CGInterpolationQuality quality)
{
    switch (quality) {
    case kCGInterpolationDefault:
        return InterpolationQuality::Default;
    case kCGInterpolationNone:
        return InterpolationQuality::DoNotInterpolate;
    case kCGInterpolationLow:
        return InterpolationQuality::Low;
    case kCGInterpolationMedium:
        return InterpolationQuality::Medium;
    case kCGInterpolationHigh:
        return InterpolationQuality::High;
    }
    return InterpolationQuality::Default;
}

static CompositeMode compositeMode(CGCompositeOperation operation)
{
    switch (operation) {
    case kCGCompositeClear:
        return { CompositeOperator::Clear, BlendMode::Normal };
    case kCGCompositeCopy:
        return { CompositeOperator::Copy, BlendMode::Normal };
    case kCGCompositeSover:
        return { CompositeOperator::SourceOver, BlendMode::Normal };
    case kCGCompositeSin:
        return { CompositeOperator::SourceIn, BlendMode::Normal };
    case kCGCompositeSout:
        return { CompositeOperator::SourceOut, BlendMode::Normal };
    case kCGCompositeSatop:
        return { CompositeOperator::SourceAtop, BlendMode::Normal };
    case kCGCompositeDover:
        return { CompositeOperator::DestinationOver, BlendMode::Normal };
    case kCGCompositeDin:
        return { CompositeOperator::DestinationIn, BlendMode::Normal };
    case kCGCompositeDout:
        return { CompositeOperator::DestinationOut, BlendMode::Normal };
    case kCGCompositeDatop:
        return { CompositeOperator::DestinationAtop, BlendMode::Normal };
    case kCGCompositeXor:
        return { CompositeOperator::XOR, BlendMode::Normal };
    case kCGCompositePlusd:
        return { CompositeOperator::PlusDarker, BlendMode::Normal };
    case kCGCompositePlusl:
        return { CompositeOperator::PlusLighter, BlendMode::Normal };
    case kCGCompositeMultiply:
        return { CompositeOperator::SourceOver, BlendMode::Multiply };
    case kCGCompositeScreen:
        return { CompositeOperator::SourceOver, BlendMode::Screen };
    case kCGCompositeOverlay:
        return { CompositeOperator::SourceOver, BlendMode::Overlay };
    case kCGCompositeDarken:
        return { CompositeOperator::SourceOver, BlendMode::Darken };
    case kCGCompositeLighten:
        return { CompositeOperator::SourceOver, BlendMode::Lighten };
    case kCGCompositeColorDodge:
        return { CompositeOperator::SourceOver, BlendMode::ColorDodge };
    case kCGCompositeColorBurn:
        return { CompositeOperator::SourceOver, BlendMode::ColorBurn };
    case kCGCompositeSoftLight:
        return { CompositeOperator::SourceOver, BlendMode::SoftLight };
    case kCGCompositeHardLight:
        return { CompositeOperator::SourceOver, BlendMode::HardLight };
    case kCGCompositeDifference:
        return { CompositeOperator::SourceOver, BlendMode::Difference };
    case kCGCompositeExclusion:
        return { CompositeOperator::SourceOver, BlendMode::Exclusion };
    case kCGCompositeHue:
        return { CompositeOperator::SourceOver, BlendMode::Hue };
    case kCGCompositeSaturation:
        return { CompositeOperator::SourceOver, BlendMode::Saturation };
    case kCGCompositeColor:
        return { CompositeOperator::SourceOver, BlendMode::Color };
    case kCGCompositeLuminosity:
        return { CompositeOperator::SourceOver, BlendMode::Luminosity };
    default:
        break;
    }
    return { CompositeOperator::SourceOver, BlendMode::Normal };
}

static LineCap lineCap(CGLineCap cap)
{
    switch (cap) {
    case kCGLineCapButt:
        return LineCap::Butt;
    case kCGLineCapRound:
        return LineCap::Round;
    case kCGLineCapSquare:
        return LineCap::Square;
    }
    return LineCap::Butt;
}

static LineJoin lineJoin(CGLineJoin join)
{
    switch (join) {
    case kCGLineJoinMiter:
        return LineJoin::Miter;
    case kCGLineJoinRound:
        return LineJoin::Round;
    case kCGLineJoinBevel:
        return LineJoin::Bevel;
    }
    return LineJoin::Miter;
}

// PDF coordinates can be far outside of what converts to float and goes through IPC safely.
static RetainPtr<CGPathRef> clampPathCoordinates(CGPathRef path)
{
    static constexpr CGFloat maximumCoordinate = 1 << 24;
    CGRect bounds = CGPathGetBoundingBox(path);
    if (CGRectIsNull(bounds) || CGRectContainsRect(CGRectMake(-maximumCoordinate, -maximumCoordinate, 2 * maximumCoordinate, 2 * maximumCoordinate), bounds))
        return path;

    auto clamp = [](CGPoint point) {
        return CGPointMake(std::clamp(point.x, -maximumCoordinate, maximumCoordinate), std::clamp(point.y, -maximumCoordinate, maximumCoordinate));
    };

    RetainPtr clampedPath = adoptCF(CGPathCreateMutable());
    CGPathApplyWithBlock(path, ^(const CGPathElement* element) {
        auto points = unsafeMakeSpan(element->points, 3);
        switch (element->type) {
        case kCGPathElementMoveToPoint:
            CGPathMoveToPoint(clampedPath.get(), nullptr, clamp(points[0]).x, clamp(points[0]).y);
            break;
        case kCGPathElementAddLineToPoint:
            CGPathAddLineToPoint(clampedPath.get(), nullptr, clamp(points[0]).x, clamp(points[0]).y);
            break;
        case kCGPathElementAddQuadCurveToPoint:
            CGPathAddQuadCurveToPoint(clampedPath.get(), nullptr, clamp(points[0]).x, clamp(points[0]).y, clamp(points[1]).x, clamp(points[1]).y);
            break;
        case kCGPathElementAddCurveToPoint:
            CGPathAddCurveToPoint(clampedPath.get(), nullptr, clamp(points[0]).x, clamp(points[0]).y, clamp(points[1]).x, clamp(points[1]).y, clamp(points[2]).x, clamp(points[2]).y);
            break;
        case kCGPathElementCloseSubpath:
            CGPathCloseSubpath(clampedPath.get());
            break;
        }
    });
    return clampedPath;
}

static Path pathFromCGPath(CGPathRef cgPath, const AffineTransform& transform = { })
{
    auto clampedPath = clampPathCoordinates(cgPath);
    CGPathRef path = clampedPath.get();

    if (transform.isIdentity())
        return Path { PathCG::create(adoptCF(CGPathCreateMutableCopy(path))) };

    CGAffineTransform cgTransform = transform;
    return Path { PathCG::create(adoptCF(CGPathCreateMutableCopyByTransformingPath(path, &cgTransform))) };
}

GStateScope::GStateScope(GraphicsContextCGDelegate& delegate, CGRenderingStateRef rstate, CGGStateRef gstate, OptionSet<Paint> paint, const AffineTransform& userSpaceTransform)
    : m_delegate(delegate)
    , m_ctm(AffineTransform(*CGGStateGetCTM(gstate)) * userSpaceTransform)
    , m_userSpaceTransform(userSpaceTransform)
    , m_paint(paint)
{
    initialize(rstate, gstate);
}

void GStateScope::initialize(CGRenderingStateRef rstate, CGGStateRef gstate)
{
    auto& target = this->target();
    target.save();

    if (CGRectIsEmpty(CGGStateGetClipBoundingBox(gstate))) {
        m_shouldDraw = false;
        return;
    }

    applyClipStack(gstate);
    if (!m_shouldDraw)
        return;

    if (CGGStateGetSoftMask(gstate)) {
        m_delegate.logUnimplementedCallback("GStateScope (soft mask)");
        m_shouldDraw = false;
        return;
    }

    target.setCTM(m_ctm);

    m_alpha = CGGStateGetAlpha(gstate);
    applyCompositeOperation(gstate);
    target.setShouldAntialias(CGGStateGetShouldAntialias(gstate) && CGRenderingStateGetAllowsAntialiasing(rstate));
    target.setImageInterpolationQuality(interpolationQuality(CGGStateGetInterpolationQuality(gstate)));

    if (m_paint.contains(Paint::Fill) && !applyBrush(CGGStateGetFillColor(gstate), BrushType::Fill))
        m_shouldDraw = false;
    if (m_paint.contains(Paint::Stroke)) {
        if (!applyBrush(CGGStateGetStrokeColor(gstate), BrushType::Stroke))
            m_shouldDraw = false;
        applyLineParameters(gstate);
    }
    target.setAlpha(m_alpha);
}

GStateScope::~GStateScope()
{
    target().restore();
}

OptionSet<GStateScope::Paint> GStateScope::paintForDrawingMode(CGPathDrawingMode mode)
{
    switch (mode) {
    case kCGPathFill:
    case kCGPathEOFill:
        return Paint::Fill;
    case kCGPathStroke:
        return Paint::Stroke;
    case kCGPathFillStroke:
    case kCGPathEOFillStroke:
        return { Paint::Fill, Paint::Stroke };
    }
    return { };
}

WindRule GStateScope::windRuleForDrawingMode(CGPathDrawingMode mode)
{
    return mode == kCGPathEOFill || mode == kCGPathEOFillStroke ? WindRule::EvenOdd : WindRule::NonZero;
}

void GStateScope::applyClipStack(CGGStateRef gstate)
{
    CGClipStackRef clipStack = CGGStateGetClipStack(gstate);
    if (!clipStack || CGClipStackIsInfinite(clipStack))
        return;

    // The clip stack is in device space.
    auto& target = this->target();
    target.setCTM({ });

    // CG intersects all rect clips into the stack's rect, and has already made aliased ones integral.
    CGRect clipRect = CGClipStackGetRect(clipStack);
    if (!CGRectEqualToRect(clipRect, CGRectInfinite)) {
        target.setShouldAntialias(true);
        target.clip(clipRect);
    }

    size_t count = CGClipStackGetCount(clipStack);
    for (size_t i = 0; i < count; ++i) {
        CGClipRef clip = CGClipStackGetClipAtIndex(clipStack, i);
        target.setShouldAntialias(CGClipGetShouldAntialias(clip));

        switch (CGClipGetType(clip)) {
        case kCGClipTypeRect:
            if (!CGClipGetStroke(clip)) {
                target.clip(CGClipGetRect(clip));
                break;
            }
            [[fallthrough]];
        case kCGClipTypePath:
        case kCGClipTypeTextClipping: {
            // Stroke and text clips become their outline paths.
            RetainPtr path = adoptCF(CGClipCreateClipPath(clip));
            if (!path) {
                m_delegate.logUnimplementedCallback("GStateScope (clip without path)");
                break;
            }
            auto windRule = CGClipGetMode(clip) == kCGEOClip ? WindRule::EvenOdd : WindRule::NonZero;
            target.clipPath(pathFromCGPath(path.get()), windRule);
            break;
        }
        case kCGClipTypeMask:
            m_delegate.logUnimplementedCallback("GStateScope (mask clip)");
            m_shouldDraw = false;
            return;
        default:
            m_delegate.logUnimplementedCallback("GStateScope (unknown clip type)");
            break;
        }
    }
}

void GStateScope::applyCompositeOperation(CGGStateRef gstate)
{
    auto mode = compositeMode(CGGStateGetCompositeOperation(gstate));
    target().setCompositeOperation(mode.operation, mode.blendMode);
}

void GStateScope::applyLineParameters(CGGStateRef gstate)
{
    auto& target = this->target();

    double userSpaceScale = std::sqrt(std::abs(m_userSpaceTransform.a() * m_userSpaceTransform.d() - m_userSpaceTransform.b() * m_userSpaceTransform.c()));
    if (!userSpaceScale)
        userSpaceScale = 1;

    // Not CGGStateGetAdjustedLineWidth(): the device minimum width isn't baked into PDF output.
    target.setStrokeStyle(StrokeStyle::SolidStroke);
    target.setStrokeThickness(CGGStateGetLineWidth(gstate) / userSpaceScale);
    target.setLineCap(lineCap(CGGStateGetLineCap(gstate)));
    target.setLineJoin(lineJoin(CGGStateGetLineJoin(gstate)));
    target.setMiterLimit(CGGStateGetMiterLimit(gstate));

    CGFloat phase = 0;
    size_t count = 0;
    const CGFloat* pattern = nullptr;
    if (CGDashRef dash = CGGStateGetLineDash(gstate))
        pattern = CGDashGetPattern(dash, &phase, &count);
    if (!pattern)
        count = 0;
    DashArray dashArray(unsafeMakeSpan(pattern, count));
    if (userSpaceScale != 1) {
        for (auto& length : dashArray)
            length /= userSpaceScale;
    }
    target.setLineDash(WTF::move(dashArray), phase / userSpaceScale);
}

bool GStateScope::applyBrush(CGColorRef color, BrushType type)
{
    auto& target = this->target();

    if (color && CGColorGetPattern(color)) {
        m_delegate.logUnimplementedCallback("GStateScope (pattern color)");
        return false;
    }

    auto webColor = color ? Color::createAndPreserveColorSpace(color) : Color::black;
    if (type == BrushType::Fill)
        target.setFillColor(webColor);
    else
        target.setStrokeColor(webColor);
    return true;
}

RetainPtr<CGContextRef> GraphicsContextCGDelegate::createCGContext(GraphicsContext& target)
{
    // The CGContextDelegate owns the GraphicsContextCGDelegate; it is deleted in finalize().
    auto* delegateInfo = new GraphicsContextCGDelegate(target);
    RetainPtr contextDelegate = adoptCF(CGContextDelegateCreate(delegateInfo));
    if (!contextDelegate) {
        delete delegateInfo;
        return nullptr;
    }
    installCallbacks(contextDelegate.get());

    // PDFKit draws annotations as it does for printing only in PDF and PostScript contexts.
    auto contextType = target.renderingMode() == RenderingMode::PDFDocument ? kCGContextTypePostScript : kCGContextTypeUnknown;
    RetainPtr context = adoptCF(CGContextCreateWithDelegate(contextDelegate.get(), contextType, nullptr, nullptr));
    if (!context)
        return nullptr;

    CGAffineTransform targetCTM = target.getCTM();
    CGContextConcatCTM(context.get(), targetCTM);
    CGContextSetBaseCTM(context.get(), targetCTM);
    return context;
}

GraphicsContextCGDelegate::GraphicsContextCGDelegate(GraphicsContext& target)
    : m_target(target)
{
}

GraphicsContextCGDelegate& GraphicsContextCGDelegate::fromDelegate(CGContextDelegateRef delegate)
{
    return *static_cast<GraphicsContextCGDelegate*>(CGContextDelegateGetInfo(delegate));
}

void GraphicsContextCGDelegate::installCallbacks(CGContextDelegateRef delegate)
{
    auto set = [&](CGContextDelegateCallbackName name, auto callback) {
        CGContextDelegateSetCallback(delegate, name, reinterpret_cast<CGContextDelegateCallback>(callback));
    };

    set(deFinalize, &finalize);
    set(deGetColorTransform, &getColorTransform);
    set(deGetTransform, &getTransform);
    set(deGetBounds, &getBounds);
    set(deGetColorSpace, &getColorSpace);
    set(deGetDelegateName, &getName);
    set(deDrawLines, &drawLines);
    set(deDrawRects, &drawRects);
    set(deDrawPath, &drawPath);
    set(deDrawPathDirect, &drawPathDirect);
    set(deStrokeArc, &strokeArc);
    set(deOperation, &operation);
}

void GraphicsContextCGDelegate::logUnimplementedCallback(const char* callbackName)
{
    LOG_ERROR("GraphicsContextCGDelegate: %s is not implemented", callbackName);
}

#pragma mark - Lifecycle

void GraphicsContextCGDelegate::finalize(CGContextDelegateRef delegate)
{
    delete &fromDelegate(delegate);
}

#pragma mark - Queries

CGColorTransformRef GraphicsContextCGDelegate::getColorTransform(CGContextDelegateRef delegate, CGRenderingStateRef, CGGStateRef)
{
    // CG doesn't convert colors for delegates; this only makes CGContextCopyDeviceColorSpace() report
    // the target's color space.
    auto& graphicsContextDelegate = fromDelegate(delegate);
    if (!graphicsContextDelegate.m_colorTransform)
        graphicsContextDelegate.m_colorTransform = adoptCF(CGColorTransformCreate(graphicsContextDelegate.target().colorSpace().platformColorSpace(), nullptr));
    return graphicsContextDelegate.m_colorTransform.get();
}

CGAffineTransform GraphicsContextCGDelegate::getTransform(CGContextDelegateRef, CGRenderingStateRef, CGGStateRef)
{
    // The gstate CTM is already absolute (see createCGContext()).
    return CGAffineTransformIdentity;
}

CGRect GraphicsContextCGDelegate::getBounds(CGContextDelegateRef delegate, CGRenderingStateRef, CGGStateRef)
{
    return fromDelegate(delegate).deviceBounds();
}

CGRect GraphicsContextCGDelegate::deviceBounds() const
{
    return m_target.getCTM().mapRect(FloatRect(m_target.clipBounds()));
}

CGColorSpaceRef GraphicsContextCGDelegate::getColorSpace(CGContextDelegateRef delegate, CGRenderingStateRef, CGGStateRef)
{
    return fromDelegate(delegate).target().colorSpace().platformColorSpace();
}

const char* GraphicsContextCGDelegate::getName(CGContextDelegateRef)
{
    return "WebCore::GraphicsContextCGDelegate";
}

#pragma mark - Drawing primitives

static void drawPathWithMode(GStateScope& scope, CGPathDrawingMode mode, const Path& path)
{
    auto& target = scope.target();
    switch (mode) {
    case kCGPathFill:
    case kCGPathEOFill:
        target.setFillRule(GStateScope::windRuleForDrawingMode(mode));
        target.fillPath(path);
        return;
    case kCGPathStroke:
        target.strokePath(path);
        return;
    case kCGPathFillStroke:
    case kCGPathEOFillStroke:
        target.setFillRule(GStateScope::windRuleForDrawingMode(mode));
        target.drawPath(path);
        return;
    }
}

void GraphicsContextCGDelegate::drawLines(CGContextDelegateRef delegate, CGRenderingStateRef rstate, CGGStateRef gstate, const CGPoint points[], size_t count)
{
    if (count < 2)
        return;

    GStateScope scope(fromDelegate(delegate), rstate, gstate, GStateScope::Paint::Stroke);
    if (!scope.shouldDraw())
        return;

    auto pointsSpan = unsafeMakeSpan(points, count);
    if (count < 4) {
        scope.target().strokeLine({ FloatPoint(pointsSpan[0]), FloatPoint(pointsSpan[1]) });
        return;
    }

    Path path;
    for (size_t i = 0; i + 1 < count; i += 2) {
        path.moveTo(pointsSpan[i]);
        path.addLineTo(pointsSpan[i + 1]);
    }
    scope.target().strokePath(path);
}

CGError GraphicsContextCGDelegate::drawRects(CGContextDelegateRef delegate, CGRenderingStateRef rstate, CGGStateRef gstate, CGPathDrawingMode mode, const CGRect rects[], size_t count)
{
    if (!count)
        return kCGErrorSuccess;

    GStateScope scope(fromDelegate(delegate), rstate, gstate, GStateScope::paintForDrawingMode(mode));
    if (!scope.shouldDraw())
        return kCGErrorSuccess;

    auto& target = scope.target();
    auto rectsSpan = unsafeMakeSpan(rects, count);
    if (count == 1) {
        FloatRect rect = CGRectStandardize(rectsSpan[0]);
        switch (mode) {
        case kCGPathFill:
        case kCGPathEOFill:
            target.fillRect(rect);
            return kCGErrorSuccess;
        case kCGPathStroke:
            target.strokeRect(rect, target.strokeThickness());
            return kCGErrorSuccess;
        case kCGPathFillStroke:
        case kCGPathEOFillStroke:
            break;
        }
    }

    Path path;
    for (auto& rect : rectsSpan)
        path.addRect(CGRectStandardize(rect));
    drawPathWithMode(scope, mode, path);
    return kCGErrorSuccess;
}

// The path is in device space; map it back so that line widths and dashes apply in user space.
CGError GraphicsContextCGDelegate::drawPath(CGContextDelegateRef delegate, CGRenderingStateRef rstate, CGGStateRef gstate, CGPathDrawingMode mode, CGPathRef path)
{
    GStateScope scope(fromDelegate(delegate), rstate, gstate, GStateScope::paintForDrawingMode(mode));
    if (!scope.shouldDraw())
        return kCGErrorSuccess;

    auto inverseCTM = scope.ctm().inverse();
    if (!inverseCTM)
        return kCGErrorSuccess;

    drawPathWithMode(scope, mode, pathFromCGPath(path, *inverseCTM));
    return kCGErrorSuccess;
}

// Unlike drawPath, the path given to drawPathDirect is in user space.
CGError GraphicsContextCGDelegate::drawPathDirect(CGContextDelegateRef delegate, CGRenderingStateRef rstate, CGGStateRef gstate, CGPathDrawingMode mode, CGPathRef path, const CGRect*)
{
    GStateScope scope(fromDelegate(delegate), rstate, gstate, GStateScope::paintForDrawingMode(mode));
    if (!scope.shouldDraw())
        return kCGErrorSuccess;

    drawPathWithMode(scope, mode, pathFromCGPath(path));
    return kCGErrorSuccess;
}

CGError GraphicsContextCGDelegate::strokeArc(CGContextDelegateRef delegate, CGRenderingStateRef rstate, CGGStateRef gstate, CGFloat x, CGFloat y, CGFloat radius, CGFloat startAngle, CGFloat endAngle, bool clockwise)
{
    GStateScope scope(fromDelegate(delegate), rstate, gstate, GStateScope::Paint::Stroke);
    if (!scope.shouldDraw())
        return kCGErrorSuccess;

    // CG's `clockwise` is relative to a y-up coordinate system; see addToCGPath(CGMutablePathRef, const PathArc&).
    auto direction = clockwise ? RotationDirection::Counterclockwise : RotationDirection::Clockwise;
    scope.target().strokeArc({ FloatPoint(x, y), static_cast<float>(radius), static_cast<float>(startAngle), static_cast<float>(endAngle), direction });
    return kCGErrorSuccess;
}

#pragma mark - Operations

CGError GraphicsContextCGDelegate::operation(CGContextDelegateRef delegate, CGRenderingStateRef, CGGStateRef, CFStringRef action, CFDictionaryRef)
{
    if (!action)
        return kCGErrorNotImplemented;

    // Like bitmap contexts, clear and erase ignore the gstate.
    bool clear = CFEqual(action, kCGContextClear);
    if (clear || CFEqual(action, kCGContextErase)) {
        auto& target = fromDelegate(delegate).target();
        auto bounds = getBounds(delegate, nullptr, nullptr);
        GraphicsContextStateSaver stateSaver(target);
        target.setCTM({ });
        if (clear)
            target.clearRect(bounds);
        else
            target.fillRect(bounds, Color::white, CompositeOperator::Copy);
        return kCGErrorSuccess;
    }

    if (CFEqual(action, kCGContextFlush) || CFEqual(action, kCGContextSynchronize) || CFEqual(action, kCGContextSynchronizeAttributes) || CFEqual(action, kCGContextWait))
        return kCGErrorSuccess;

    // For kCGContextDisplayList, this makes CG replay the display list through the drawing callbacks.
    return kCGErrorNotImplemented;
}

} // namespace WebCore

#endif // USE(CG)
