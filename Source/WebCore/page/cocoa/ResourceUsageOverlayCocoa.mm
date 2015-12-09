/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "ResourceUsageOverlay.h"

#if ENABLE(RESOURCE_USAGE_OVERLAY)

#include "JSDOMWindow.h"
#include "MachVMSPI.h"
#include "PlatformCALayer.h"
#include <CoreGraphics/CGContext.h>
#include <QuartzCore/CALayer.h>
#include <QuartzCore/CATransaction.h>
#include <array>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <runtime/JSLock.h>
#include <sys/sysctl.h>
#include <thread>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

@interface WebOverlayLayer : CALayer {
    ResourceUsageOverlay* m_overlay;
}
@end

@implementation WebOverlayLayer

- (WebOverlayLayer *)initWithResourceUsageOverlay:(ResourceUsageOverlay *)overlay
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

static size_t vmPageSize()
{
    static size_t pageSize;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        size_t outputSize = sizeof(pageSize);
        int status = sysctlbyname("vm.pagesize", &pageSize, &outputSize, nullptr, 0);
        ASSERT_UNUSED(status, status != -1);
        ASSERT(pageSize);
    });
    return pageSize;
}

template<typename T, size_t size = 70>
class RingBuffer {
public:
    RingBuffer()
    {
        m_data.fill(0);
    }

    void append(T v)
    {
        m_data[m_current] = WTF::move(v);
        incrementIndex(m_current);
    }

    T& last()
    {
        unsigned index = m_current;
        decrementIndex(index);
        return m_data[index];
    }

    void forEach(std::function<void(T)> func) const
    {
        unsigned i = m_current;
        for (unsigned visited = 0; visited < size; ++visited) {
            func(m_data[i]);
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

namespace MemoryCategory {
static const unsigned bmalloc = 0;
static const unsigned LibcMalloc = 1;
static const unsigned JSJIT = 2;
static const unsigned Images = 3;
static const unsigned GCHeap = 4;
static const unsigned GCOwned = 5;
static const unsigned Other = 6;
static const unsigned Layers = 7;
static const unsigned NumberOfCategories = 8;
}

static CGColorRef createColor(float r, float g, float b, float a)
{
    static CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGFloat components[4] = { r, g, b, a };
    return CGColorCreate(colorSpace, components);
}

struct MemoryCategoryInfo {
    MemoryCategoryInfo() { } // Needed for std::array.

    MemoryCategoryInfo(unsigned category, RGBA32 rgba, String name, bool subcategory = false)
        : name(WTF::move(name))
        , isSubcategory(subcategory)
        , type(category)
    {
        float r, g, b, a;
        Color(rgba).getRGBA(r, g, b, a);
        color = adoptCF(createColor(r, g, b, a));
    }

    String name;
    RetainPtr<CGColorRef> color;
    RingBuffer<size_t> history;
    RingBuffer<size_t> reclaimableHistory;
    bool isSubcategory { false };
    unsigned type { MemoryCategory::NumberOfCategories };
};

struct ResourceUsageData {
    ResourceUsageData();

    Lock lock;

    RingBuffer<size_t> totalDirty;
    std::array<MemoryCategoryInfo, MemoryCategory::NumberOfCategories> categories;

    RingBuffer<float> cpuHistory;
    RingBuffer<size_t> gcHeapSizeHistory;

    HashSet<CALayer *> overlayLayers;
    JSC::VM* vm { nullptr };
};

ResourceUsageData::ResourceUsageData()
{
    // VM tag categories.
    categories[MemoryCategory::JSJIT] = MemoryCategoryInfo(MemoryCategory::JSJIT, 0xFFFF60FF, "JS JIT");
    categories[MemoryCategory::Images] = MemoryCategoryInfo(MemoryCategory::Images, 0xFFFFFF00, "Images");
    categories[MemoryCategory::Layers] = MemoryCategoryInfo(MemoryCategory::Layers, 0xFF00FFFF, "Layers");
    categories[MemoryCategory::LibcMalloc] = MemoryCategoryInfo(MemoryCategory::LibcMalloc, 0xFF00FF00, "libc malloc");
    categories[MemoryCategory::bmalloc] = MemoryCategoryInfo(MemoryCategory::bmalloc, 0xFFFF6060, "bmalloc");
    categories[MemoryCategory::Other] = MemoryCategoryInfo(MemoryCategory::Other, 0xFFC0FF00, "Other");

    // Sub categories (e.g breakdown of bmalloc tag.)
    categories[MemoryCategory::GCHeap] = MemoryCategoryInfo(MemoryCategory::GCHeap, 0xFFA0A0FF, "GC heap", true);
    categories[MemoryCategory::GCOwned] = MemoryCategoryInfo(MemoryCategory::GCOwned, 0xFFFFC060, "GC owned", true);
}

static ResourceUsageData& sharedData()
{
    static NeverDestroyed<ResourceUsageData> data;
    return data;
}

void runSamplerThread(void*);

void ResourceUsageOverlay::platformInitialize()
{
    auto& data = sharedData();
    LockHolder locker(data.lock);

    // FIXME: The sampler thread will never stop once started.
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        data.vm = &JSDOMWindow::commonVM();
        createThread(runSamplerThread, nullptr, "ResourceUsageOverlay Sampler");
    });

    m_layer = adoptNS([[WebOverlayLayer alloc] initWithResourceUsageOverlay:this]);

    m_containerLayer = adoptNS([[CALayer alloc] init]);
    [m_containerLayer.get() addSublayer:m_layer.get()];

    [m_containerLayer.get() setAnchorPoint:CGPointZero];
    [m_containerLayer.get() setBounds:CGRectMake(0, 0, normalWidth, normalHeight)];

    [m_layer.get() setAnchorPoint:CGPointZero];
    [m_layer.get() setContentsScale:2.0];
    [m_layer.get() setBackgroundColor:adoptCF(createColor(0, 0, 0, 0.8)).get()];
    [m_layer.get() setBounds:CGRectMake(0, 0, normalWidth, normalHeight)];

    overlay().layer().setContentsToPlatformLayer(m_layer.get(), GraphicsLayer::NoContentsLayer);

    data.overlayLayers.add(m_layer.get());
}

void ResourceUsageOverlay::platformDestroy()
{
    auto& data = sharedData();
    LockHolder locker(data.lock);
    data.overlayLayers.remove(m_layer.get());
}

static void showText(CGContextRef context, float x, float y, CGColorRef color, const String& text)
{
    CGContextSaveGState(context);

    CGContextSetTextDrawingMode(context, kCGTextFill);
    CGContextSetFillColorWithColor(context, color);

    CGContextSetTextMatrix(context, CGAffineTransformMakeScale(1, -1));

    // FIXME: Don't use deprecated APIs.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#if PLATFORM(IOS)
    CGContextSelectFont(context, "Courier", 10, kCGEncodingMacRoman);
#else
    CGContextSelectFont(context, "Menlo", 11, kCGEncodingMacRoman);
#endif

    CString cstr = text.ascii();
    CGContextShowTextAtPoint(context, x, y, cstr.data(), cstr.length());
#pragma clang diagnostic pop

    CGContextRestoreGState(context);
}

static void drawGraphLabel(CGContextRef context, float x, float y, const String& text)
{
    static CGColorRef black = createColor(0, 0, 0, 1);
    showText(context, x + 5, y - 3, black, text);
    static CGColorRef white = createColor(1, 1, 1, 1);
    showText(context, x + 4, y - 4, white, text);
}

static void drawCpuHistory(CGContextRef context, float x1, float y1, float y2, RingBuffer<float>& history)
{
    static CGColorRef cpuColor = createColor(0, 1, 0, 1);

    CGContextSetStrokeColorWithColor(context, cpuColor);
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

    static CGColorRef capacityColor = createColor(1, 0, 0.3, 1);
    CGContextSetStrokeColorWithColor(context, capacityColor);

    size_t i = 0;

    capacityHistory.forEach([&](size_t m) {
        float mem = (float)m / (float)peak;
        CGContextBeginPath(context);
        CGContextMoveToPoint(context, x1 + i, y2);
        CGContextAddLineToPoint(context, x1 + i, y2 - (yScale * mem));
        CGContextStrokePath(context);
        i++;
    });

    static CGColorRef sizeColor = createColor(0.6, 0.5, 0.9, 1);
    CGContextSetStrokeColorWithColor(context, sizeColor);

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

static void drawMemHistory(CGContextRef context, float x1, float y1, float y2, ResourceUsageData& data)
{
    float yScale = y2 - y1;

    size_t peak = 0;
    data.totalDirty.forEach([&](size_t m) {
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
        category.history.forEach([&](size_t m) {
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

static void drawMemoryPie(CGContextRef context, FloatRect& rect, ResourceUsageData& data)
{
    float radius = rect.width() / 2;
    FloatPoint center = rect.center();
    size_t totalDirty = data.totalDirty.last();

    float angle = 0;
    for (auto& category : data.categories)
        drawSlice(context, center, angle, radius, category.history.last(), totalDirty, category.color.get());
}

static String formatByteNumber(size_t number)
{
    if (number >= 1024 * 1048576)
        return String::format("%.3f GB", static_cast<double>(number) / 1024 * 1048576);
    if (number >= 1048576)
        return String::format("%.2f MB", static_cast<double>(number) / 1048576);
    if (number >= 1024)
        return String::format("%.1f kB", static_cast<double>(number) / 1024);
    return String::format("%lu", number);
}

void ResourceUsageOverlay::platformDraw(CGContextRef context)
{
    auto& data = sharedData();
    LockHolder locker(data.lock);

    if (![m_layer.get() contentsAreFlipped]) {
        CGContextScaleCTM(context, 1, -1);
        CGContextTranslateCTM(context, 0, -normalHeight);
    }

    CGContextSetShouldAntialias(context, false);
    CGContextSetShouldSmoothFonts(context, false);

    CGRect viewBounds = m_overlay->bounds();
    CGContextClearRect(context, viewBounds);

    static CGColorRef colorForLabels = createColor(0.9, 0.9, 0.9, 1);
    showText(context, 10, 20, colorForLabels, String::format("        CPU: %g", data.cpuHistory.last()));
    showText(context, 10, 30, colorForLabels, "  Footprint: " + formatByteNumber(data.totalDirty.last()));

    float y = 50;
    for (auto& category : data.categories) {
        String label = String::format("% 11s: %s", category.name.ascii().data(), formatByteNumber(category.history.last()).ascii().data());
        size_t reclaimable = category.reclaimableHistory.last();
        if (reclaimable)
            label = label + String::format(" [%s]", formatByteNumber(reclaimable).ascii().data());

        // FIXME: Show size/capacity of GC heap.
        showText(context, 10, y, category.color.get(), label);
        y += 10;
    }

    drawCpuHistory(context, viewBounds.size.width - 70, 0, viewBounds.size.height, data.cpuHistory);
    drawGCHistory(context, viewBounds.size.width - 140, 0, viewBounds.size.height, data.gcHeapSizeHistory, data.categories[MemoryCategory::GCHeap].history);
    drawMemHistory(context, viewBounds.size.width - 210, 0, viewBounds.size.height, data);

    FloatRect pieRect(viewBounds.size.width - 330, 0, 100, viewBounds.size.height);
    drawMemoryPie(context, pieRect, data);
}

struct TagInfo {
    TagInfo() { }
    size_t dirty { 0 };
    size_t reclaimable { 0 };
};

static std::array<TagInfo, 256> pagesPerVMTag()
{
    std::array<TagInfo, 256> tags;
    task_t task = mach_task_self();
    mach_vm_size_t size;
    uint32_t depth = 0;
    struct vm_region_submap_info_64 info = { };
    mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
    for (mach_vm_address_t addr = 0; ; addr += size) {
        int purgeableState;
        if (mach_vm_purgable_control(task, addr, VM_PURGABLE_GET_STATE, &purgeableState) != KERN_SUCCESS)
            purgeableState = VM_PURGABLE_DENY;

        kern_return_t kr = mach_vm_region_recurse(task, &addr, &size, &depth, (vm_region_info_t)&info, &count);
        if (kr != KERN_SUCCESS)
            break;

        if (purgeableState == VM_PURGABLE_VOLATILE) {
            tags[info.user_tag].reclaimable += info.pages_resident;
            continue;
        }

        if (purgeableState == VM_PURGABLE_EMPTY) {
            tags[info.user_tag].reclaimable += size / vmPageSize();
            continue;
        }

        bool anonymous = !info.external_pager;
        if (anonymous) {
            tags[info.user_tag].dirty += info.pages_resident - info.pages_reusable;
            tags[info.user_tag].reclaimable += info.pages_reusable;
        } else
            tags[info.user_tag].dirty += info.pages_dirtied;
    }

    return tags;
}

static float cpuUsage()
{
    thread_array_t threadList;
    mach_msg_type_number_t threadCount;
    kern_return_t kr = task_threads(mach_task_self(), &threadList, &threadCount);
    if (kr != KERN_SUCCESS)
        return -1;

    float usage = 0;

    for (mach_msg_type_number_t i = 0; i < threadCount; ++i) {
        thread_info_data_t threadInfo;
        thread_basic_info_t threadBasicInfo;

        mach_msg_type_number_t threadInfoCount = THREAD_INFO_MAX;
        kr = thread_info(threadList[i], THREAD_BASIC_INFO, static_cast<thread_info_t>(threadInfo), &threadInfoCount);
        if (kr != KERN_SUCCESS)
            return -1;

        threadBasicInfo = reinterpret_cast<thread_basic_info_t>(threadInfo);

        if (!(threadBasicInfo->flags & TH_FLAGS_IDLE))
            usage += threadBasicInfo->cpu_usage / static_cast<float>(TH_USAGE_SCALE) * 100.0;
    }

    kr = vm_deallocate(mach_task_self(), (vm_offset_t)threadList, threadCount * sizeof(thread_t));
    ASSERT(kr == KERN_SUCCESS);

    return usage;
}

static unsigned categoryForVMTag(unsigned tag)
{
    switch (tag) {
    case VM_MEMORY_IOKIT:
    case VM_MEMORY_LAYERKIT:
        return MemoryCategory::Layers;
    case VM_MEMORY_IMAGEIO:
    case VM_MEMORY_CGIMAGE:
        return MemoryCategory::Images;
    case VM_MEMORY_JAVASCRIPT_JIT_EXECUTABLE_ALLOCATOR:
        return MemoryCategory::JSJIT;
    case VM_MEMORY_MALLOC:
    case VM_MEMORY_MALLOC_HUGE:
    case VM_MEMORY_MALLOC_LARGE:
    case VM_MEMORY_MALLOC_SMALL:
    case VM_MEMORY_MALLOC_TINY:
    case VM_MEMORY_MALLOC_NANO:
        return MemoryCategory::LibcMalloc;
    case VM_MEMORY_TCMALLOC:
        return MemoryCategory::bmalloc;
    default:
        return MemoryCategory::Other;
    }
};

NO_RETURN void runSamplerThread(void*)
{
    auto& data = sharedData();
    while (1) {
        float cpu = cpuUsage();
        auto tags = pagesPerVMTag();
        Vector<CALayer *, 8> layers;

        {
            LockHolder locker(data.lock);
            data.cpuHistory.append(cpu);

            std::array<TagInfo, MemoryCategory::NumberOfCategories> pagesPerCategory;

            size_t totalDirtyPages = 0;
            for (unsigned i = 0; i < 256; ++i) {
                pagesPerCategory[categoryForVMTag(i)].dirty += tags[i].dirty;
                pagesPerCategory[categoryForVMTag(i)].reclaimable += tags[i].reclaimable;
                totalDirtyPages += tags[i].dirty;
            }

            for (auto& category : data.categories) {
                if (category.isSubcategory) // Only do automatic tallying for top-level categories.
                    continue;
                category.history.append(pagesPerCategory[category.type].dirty * vmPageSize());
                category.reclaimableHistory.append(pagesPerCategory[category.type].reclaimable * vmPageSize());
            }
            data.totalDirty.append(totalDirtyPages * vmPageSize());

            copyToVector(data.overlayLayers, layers);

            size_t currentGCHeapCapacity = data.vm->heap.blockBytesAllocated();
            size_t currentGCOwned = data.vm->heap.extraMemorySize();

            data.categories[MemoryCategory::GCHeap].history.append(currentGCHeapCapacity);
            data.categories[MemoryCategory::GCOwned].history.append(currentGCOwned);

            // Subtract known subchunks from the bmalloc bucket.
            // FIXME: Handle running with bmalloc disabled.
            data.categories[MemoryCategory::bmalloc].history.last() -= currentGCHeapCapacity + currentGCOwned;
        }

        [CATransaction begin];
        for (CALayer *layer : layers) {
            // FIXME: It shouldn't be necessary to update the bounds on every single thread loop iteration,
            // but something is causing them to become 0x0.
            CALayer *containerLayer = [layer superlayer];
            CGRect rect = CGRectMake(0, 0, ResourceUsageOverlay::normalWidth, ResourceUsageOverlay::normalHeight);
            [layer setBounds:rect];
            [containerLayer setBounds:rect];
            [layer setNeedsDisplay];
        }
        [CATransaction commit];

        // FIXME: Find a way to get the size of the current GC heap size safely from the sampler thread.
        callOnMainThread([] {
            auto& data = sharedData();
            JSC::JSLockHolder lock(data.vm);
            size_t gcHeapSize = data.vm->heap.size() - data.vm->heap.extraMemorySize();

            LockHolder locker(data.lock);
            data.gcHeapSizeHistory.append(gcHeapSize);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

}

#endif
