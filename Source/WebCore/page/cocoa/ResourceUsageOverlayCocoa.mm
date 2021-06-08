/*
 * Copyright (C) 2015, 2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ResourceUsageOverlay.h"

#if ENABLE(RESOURCE_USAGE)

#import "ColorSpaceCG.h"
#import "CommonVM.h"
#import "JSDOMWindow.h"
#import "PlatformCALayer.h"
#import "ResourceUsageThread.h"
#import <CoreGraphics/CGContext.h>
#import <CoreText/CoreText.h>
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CATransaction.h>
#import <wtf/MainThread.h>
#import <wtf/MathExtras.h>
#import <wtf/MemoryFootprint.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/StringConcatenateNumbers.h>

using WebCore::ResourceUsageOverlay;

@interface WebResourceUsageOverlayLayer : CALayer {
    ResourceUsageOverlay* m_overlay;
}
@end

@implementation WebResourceUsageOverlayLayer

- (instancetype)initWithResourceUsageOverlay:(ResourceUsageOverlay *)overlay
{
    self = [super init];
    if (!self)
        return nil;
    m_overlay = overlay;
    return self;
}

- (void)drawInContext:(CGContextRef)context
{
    m_overlay->platformDraw(context);
}

@end

namespace WebCore {

template<typename T, size_t size = 70>
class RingBuffer {
public:
    RingBuffer()
    {
        m_data.fill(0);
    }

    void append(T v)
    {
        m_data[m_current] = WTFMove(v);
        incrementIndex(m_current);
    }

    T& last()
    {
        unsigned index = m_current;
        decrementIndex(index);
        return m_data[index];
    }

    void forEach(const WTF::Function<void(T)>& apply) const
    {
        unsigned i = m_current;
        for (unsigned visited = 0; visited < size; ++visited) {
            apply(m_data[i]);
            incrementIndex(i);
        }
    }

private:
    static void incrementIndex(unsigned& index)
    {
        if (++index == size)
            index = 0;
    }

    static void decrementIndex(unsigned& index)
    {
        if (index)
            --index;
        else
            index = size - 1;
    }

    std::array<T, size> m_data;
    unsigned m_current { 0 };
};

static RetainPtr<CGColorRef> createColor(float r, float g, float b, float a)
{
    CGFloat components[4] = { r, g, b, a };
    return adoptCF(CGColorCreate(sRGBColorSpaceRef(), components));
}

struct HistoricMemoryCategoryInfo {
    HistoricMemoryCategoryInfo() { } // Needed for std::array.

    HistoricMemoryCategoryInfo(unsigned category, SRGBA<uint8_t> color, String name, bool subcategory = false)
        : name(WTFMove(name))
        , color(cachedCGColor(color))
        , isSubcategory(subcategory)
        , type(category)
    {
    }

    String name;
    RetainPtr<CGColorRef> color;
    RingBuffer<size_t> dirtySize;
    RingBuffer<size_t> reclaimableSize;
    RingBuffer<size_t> externalSize;
    bool isSubcategory { false };
    unsigned type { MemoryCategory::NumberOfCategories };
};

struct HistoricResourceUsageData {
    HistoricResourceUsageData();

    RingBuffer<float> cpu;
    RingBuffer<size_t> totalDirtySize;
    RingBuffer<size_t> totalExternalSize;
    RingBuffer<size_t> gcHeapSize;
    std::array<HistoricMemoryCategoryInfo, MemoryCategory::NumberOfCategories> categories;
    MonotonicTime timeOfNextEdenCollection { MonotonicTime::nan() };
    MonotonicTime timeOfNextFullCollection { MonotonicTime::nan() };
};

HistoricResourceUsageData::HistoricResourceUsageData()
{
    // VM tag categories.
    categories[MemoryCategory::JSJIT] = HistoricMemoryCategoryInfo(MemoryCategory::JSJIT, { 255, 96, 255 }, "JS JIT");
    categories[MemoryCategory::Gigacage] = HistoricMemoryCategoryInfo(MemoryCategory::Gigacage, { 101, 79, 240 }, "Gigacage");
    categories[MemoryCategory::Images] = HistoricMemoryCategoryInfo(MemoryCategory::Images, Color::yellow, "Images");
    categories[MemoryCategory::Layers] = HistoricMemoryCategoryInfo(MemoryCategory::Layers, Color::cyan, "Layers");
    categories[MemoryCategory::LibcMalloc] = HistoricMemoryCategoryInfo(MemoryCategory::LibcMalloc, Color::green, "libc malloc");
    categories[MemoryCategory::bmalloc] = HistoricMemoryCategoryInfo(MemoryCategory::bmalloc, { 255, 96, 96 }, "bmalloc");
    categories[MemoryCategory::IsoHeap] = HistoricMemoryCategoryInfo(MemoryCategory::IsoHeap, { 128, 159, 64 }, "IsoHeap");
    categories[MemoryCategory::Other] = HistoricMemoryCategoryInfo(MemoryCategory::Other, { 192, 255, 0 }, "Other");

    // Sub categories (e.g breakdown of bmalloc tag.)
    categories[MemoryCategory::GCHeap] = HistoricMemoryCategoryInfo(MemoryCategory::GCHeap, { 160, 160, 255 }, "GC heap", true);
    categories[MemoryCategory::GCOwned] = HistoricMemoryCategoryInfo(MemoryCategory::GCOwned, { 255, 192, 96 }, "GC owned", true);

#ifndef NDEBUG
    // Ensure this aligns with ResourceUsageData's category order.
    ResourceUsageData d;
    ASSERT(categories.size() == d.categories.size());
    for (size_t i = 0; i < categories.size(); ++i)
        ASSERT(categories[i].type == d.categories[i].type);
#endif
}

static HistoricResourceUsageData& historicUsageData()
{
    static NeverDestroyed<HistoricResourceUsageData> data;
    return data;
}

static void appendDataToHistory(const ResourceUsageData& data)
{
    ASSERT(isMainThread());

    auto& historicData = historicUsageData();
    historicData.cpu.append(data.cpu);
    historicData.totalDirtySize.append(data.totalDirtySize);
    historicData.totalExternalSize.append(data.totalExternalSize);
    for (auto& category : historicData.categories) {
        category.dirtySize.append(data.categories[category.type].dirtySize);
        category.reclaimableSize.append(data.categories[category.type].reclaimableSize);
        category.externalSize.append(data.categories[category.type].externalSize);
    }
    historicData.timeOfNextEdenCollection = data.timeOfNextEdenCollection;
    historicData.timeOfNextFullCollection = data.timeOfNextFullCollection;

    // FIXME: Find a way to add this to ResourceUsageData and calculate it on the resource usage sampler thread.
    {
        JSC::VM* vm = &commonVM();
        JSC::JSLockHolder lock(vm);
        historicData.gcHeapSize.append(vm->heap.size() - vm->heap.extraMemorySize());
    }
}

void ResourceUsageOverlay::platformInitialize()
{
    m_layer = adoptNS([[WebResourceUsageOverlayLayer alloc] initWithResourceUsageOverlay:this]);

    m_containerLayer = adoptNS([[CALayer alloc] init]);
    [m_containerLayer.get() addSublayer:m_layer.get()];

    [m_containerLayer.get() setAnchorPoint:CGPointZero];
    [m_containerLayer.get() setBounds:CGRectMake(0, 0, normalWidth, normalHeight)];

    [m_layer.get() setAnchorPoint:CGPointZero];
    [m_layer.get() setContentsScale:2.0];
    [m_layer.get() setBackgroundColor:createColor(0, 0, 0, 0.8).get()];
    [m_layer.get() setBounds:CGRectMake(0, 0, normalWidth, normalHeight)];

    overlay().layer().setContentsToPlatformLayer(m_layer.get(), GraphicsLayer::ContentsLayerPurpose::None);

    ResourceUsageThread::addObserver(this, All, [this] (const ResourceUsageData& data) {
        appendDataToHistory(data);

        // FIXME: It shouldn't be necessary to update the bounds on every single thread loop iteration,
        // but something is causing them to become 0x0.
        [CATransaction begin];
        CALayer *containerLayer = [m_layer superlayer];
        CGRect rect = CGRectMake(0, 0, ResourceUsageOverlay::normalWidth, ResourceUsageOverlay::normalHeight);
        [m_layer setBounds:rect];
        [containerLayer setBounds:rect];
        [m_layer setNeedsDisplay];
        [CATransaction commit];
    });
}

void ResourceUsageOverlay::platformDestroy()
{
    ResourceUsageThread::removeObserver(this);
}

static void showText(CGContextRef context, float x, float y, CGColorRef color, const String& text)
{
    CGContextSaveGState(context);

    CGContextSetTextDrawingMode(context, kCGTextFill);
    CGContextSetFillColorWithColor(context, color);

    auto matrix = CGAffineTransformMakeScale(1, -1);
#if PLATFORM(IOS_FAMILY)
    CFStringRef fontName = CFSTR("Courier");
    CGFloat fontSize = 10;
#else
    CFStringRef fontName = CFSTR("Menlo");
    CGFloat fontSize = 11;
#endif
    auto font = adoptCF(CTFontCreateWithName(fontName, fontSize, &matrix));
    CFTypeRef keys[] = { kCTFontAttributeName, kCTForegroundColorFromContextAttributeName };
    CFTypeRef values[] = { font.get(), kCFBooleanTrue };
    auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CString cstr = text.ascii();
    auto string = adoptCF(CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, cstr.dataAsUInt8Ptr(), cstr.length(), kCFStringEncodingASCII, false, kCFAllocatorNull));
    auto attributedString = adoptCF(CFAttributedStringCreate(kCFAllocatorDefault, string.get(), attributes.get()));
    auto line = adoptCF(CTLineCreateWithAttributedString(attributedString.get()));
    CGContextSetTextPosition(context, x, y);
    CTLineDraw(line.get(), context);

    CGContextRestoreGState(context);
}

static void drawGraphLabel(CGContextRef context, float x, float y, const String& text)
{
    static NeverDestroyed<RetainPtr<CGColorRef>> black = createColor(0, 0, 0, 1);
    showText(context, x + 5, y - 3, black.get().get(), text);
    static NeverDestroyed<RetainPtr<CGColorRef>> white = createColor(1, 1, 1, 1);
    showText(context, x + 4, y - 4, white.get().get(), text);
}

static void drawCpuHistory(CGContextRef context, float x1, float y1, float y2, RingBuffer<float>& history)
{
    static NeverDestroyed<RetainPtr<CGColorRef>> cpuColor = createColor(0, 1, 0, 1);

    CGContextSetStrokeColorWithColor(context, cpuColor.get().get());
    CGContextSetLineWidth(context, 1);

    int i = 0;

    history.forEach([&](float c) {
        float cpu = c / 100;
        float yScale = y2 - y1;

        CGContextBeginPath(context);
        CGContextMoveToPoint(context, x1 + i, y2);
        CGContextAddLineToPoint(context, x1 + i, y2 - (yScale * cpu));
        CGContextStrokePath(context);
        i++;
    });

    drawGraphLabel(context, x1, y2, "CPU");
}

static void drawGCHistory(CGContextRef context, float x1, float y1, float y2, RingBuffer<size_t>& sizeHistory, RingBuffer<size_t>& capacityHistory)
{
    float yScale = y2 - y1;

    size_t peak = 0;
    capacityHistory.forEach([&](size_t m) {
        if (m > peak)
            peak = m;
    });

    CGContextSetLineWidth(context, 1);

    static NeverDestroyed<RetainPtr<CGColorRef>> capacityColor = createColor(1, 0, 0.3, 1);
    CGContextSetStrokeColorWithColor(context, capacityColor.get().get());

    size_t i = 0;

    capacityHistory.forEach([&](size_t m) {
        float mem = (float)m / (float)peak;
        CGContextBeginPath(context);
        CGContextMoveToPoint(context, x1 + i, y2);
        CGContextAddLineToPoint(context, x1 + i, y2 - (yScale * mem));
        CGContextStrokePath(context);
        i++;
    });

    static NeverDestroyed<RetainPtr<CGColorRef>> sizeColor = createColor(0.6, 0.5, 0.9, 1);
    CGContextSetStrokeColorWithColor(context, sizeColor.get().get());

    i = 0;

    sizeHistory.forEach([&](size_t m) {
        float mem = (float)m / (float)peak;
        CGContextBeginPath(context);
        CGContextMoveToPoint(context, x1 + i, y2);
        CGContextAddLineToPoint(context, x1 + i, y2 - (yScale * mem));
        CGContextStrokePath(context);
        i++;
    });

    drawGraphLabel(context, x1, y2, "GC");
}

static void drawMemHistory(CGContextRef context, float x1, float y1, float y2, HistoricResourceUsageData& data)
{
    float yScale = y2 - y1;

    size_t peak = 0;
    data.totalDirtySize.forEach([&](size_t m) {
        if (m > peak)
            peak = m;
    });

    CGContextSetLineWidth(context, 1);

    struct ColorAndSize {
        CGColorRef color;
        size_t size;
    };

    std::array<std::array<ColorAndSize, MemoryCategory::NumberOfCategories>, 70> columns;

    for (auto& category : data.categories) {
        unsigned x = 0;
        category.dirtySize.forEach([&](size_t m) {
            columns[x][category.type] = { category.color.get(), m };
            ++x;
        });
    }

    unsigned i = 0;
    for (auto& column : columns) {
        float currentY2 = y2;
        for (auto& colorAndSize : column) {
            float chunk = (float)colorAndSize.size / (float)peak;
            float nextY2 = currentY2 - (yScale * chunk);
            CGContextBeginPath(context);
            CGContextMoveToPoint(context, x1 + i, currentY2);
            CGContextAddLineToPoint(context, x1 + i, nextY2);
            CGContextSetStrokeColorWithColor(context, colorAndSize.color);
            CGContextStrokePath(context);
            currentY2 = nextY2;
        }
        ++i;
    }

    drawGraphLabel(context, x1, y2, "Mem");
}

static const float fullCircleInRadians = piFloat * 2;

static void drawSlice(CGContextRef context, FloatPoint center, float& angle, float radius, size_t sliceSize, size_t totalSize, CGColorRef color)
{
    float part = (float)sliceSize / (float)totalSize;

    CGContextBeginPath(context);
    CGContextMoveToPoint(context, center.x(), center.y());
    CGContextAddArc(context, center.x(), center.y(), radius, angle, angle + part * fullCircleInRadians, false);
    CGContextSetFillColorWithColor(context, color);
    CGContextFillPath(context);
    angle += part * fullCircleInRadians;
}

static void drawMemoryPie(CGContextRef context, FloatRect& rect, HistoricResourceUsageData& data)
{
    float radius = rect.width() / 2;
    FloatPoint center = rect.center();
    size_t totalDirty = data.totalDirtySize.last();

    float angle = 0;
    for (auto& category : data.categories)
        drawSlice(context, center, angle, radius, category.dirtySize.last(), totalDirty, category.color.get());
}

static String formatByteNumber(size_t number)
{
    if (number >= 1024 * 1048576)
        return makeString(FormattedNumber::fixedWidth(number / (1024. * 1048576), 3), " GB");
    if (number >= 1048576)
        return makeString(FormattedNumber::fixedWidth(number / 1048576., 2), " MB");
    if (number >= 1024)
        return makeString(FormattedNumber::fixedWidth(number / 1024, 1), " kB");
    return String::number(number);
}

static String gcTimerString(MonotonicTime timerFireDate, MonotonicTime now)
{
    if (std::isnan(timerFireDate))
        return "[not scheduled]"_s;
    return String::numberToStringFixedPrecision((timerFireDate - now).seconds());
}

void ResourceUsageOverlay::platformDraw(CGContextRef context)
{
    auto& data = historicUsageData();

    if (![m_layer.get() contentsAreFlipped]) {
        CGContextScaleCTM(context, 1, -1);
        CGContextTranslateCTM(context, 0, -normalHeight);
    }

    CGContextSetShouldAntialias(context, false);
    CGContextSetShouldSmoothFonts(context, false);

    CGRect viewBounds = m_overlay->bounds();
    CGContextClearRect(context, viewBounds);

    static NeverDestroyed<RetainPtr<CGColorRef>> colorForLabels = createColor(0.9, 0.9, 0.9, 1);
    showText(context, 10, 20, colorForLabels.get().get(), makeString("        CPU: ", FormattedNumber::fixedPrecision(data.cpu.last(), 6, KeepTrailingZeros)));
    showText(context, 10, 30, colorForLabels.get().get(), "  Footprint: " + formatByteNumber(memoryFootprint()));
    showText(context, 10, 40, colorForLabels.get().get(), "   External: " + formatByteNumber(data.totalExternalSize.last()));

    float y = 55;
    for (auto& category : data.categories) {
        size_t dirty = category.dirtySize.last();
        size_t reclaimable = category.reclaimableSize.last();
        size_t external = category.externalSize.last();
        
        auto label = makeString(pad(' ', 11, category.name), ": ", formatByteNumber(dirty));
        if (external)
            label = label + makeString(" + ", formatByteNumber(external));
        if (reclaimable)
            label = label + makeString(" [", formatByteNumber(reclaimable), ']');

        // FIXME: Show size/capacity of GC heap.
        showText(context, 10, y, category.color.get(), label);
        y += 10;
    }
    y -= 5;

    MonotonicTime now = MonotonicTime::now();
    showText(context, 10, y + 10, colorForLabels.get().get(), "    Eden GC: " + gcTimerString(data.timeOfNextEdenCollection, now));
    showText(context, 10, y + 20, colorForLabels.get().get(), "    Full GC: " + gcTimerString(data.timeOfNextFullCollection, now));

    drawCpuHistory(context, viewBounds.size.width - 70, 0, viewBounds.size.height, data.cpu);
    drawGCHistory(context, viewBounds.size.width - 140, 0, viewBounds.size.height, data.gcHeapSize, data.categories[MemoryCategory::GCHeap].dirtySize);
    drawMemHistory(context, viewBounds.size.width - 210, 0, viewBounds.size.height, data);

    FloatRect pieRect(viewBounds.size.width - 330, 0, 100, viewBounds.size.height);
    drawMemoryPie(context, pieRect, data);
}

}

#endif // ENABLE(RESOURCE_USAGE)
