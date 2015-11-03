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

#include "GraphicsContext.h"
#include "JSDOMWindow.h"
#include "MachVMSPI.h"
#include "PlatformCALayer.h"
#include <QuartzCore/CALayer.h>
#include <QuartzCore/CATransaction.h>
#include <array>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <runtime/JSLock.h>
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
    GraphicsContext gc(context);
    m_overlay->draw(gc);
}

@end

namespace WebCore {

static const RGBA32 colorForJITCode    = 0xFFFF60FF;
static const RGBA32 colorForImages     = 0xFFFFFF00;
static const RGBA32 colorForLayers     = 0xFF00FFFF;
static const RGBA32 colorForGCHeap     = 0xFFA0A0FF;
static const RGBA32 colorForLibcMalloc = 0xFF00FF00;
static const RGBA32 colorForFastMalloc = 0xFFFF6060;
static const RGBA32 colorForOther      = 0xFFC0FF00;
static const RGBA32 colorForLabels     = 0xFFE0E0E0;

template<typename T, size_t size = 50>
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

    T last() const
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

struct ResourceUsageData {
    Lock lock;
    RingBuffer<float> cpuHistory;
    RingBuffer<size_t> gcHeapSizeHistory;
    RingBuffer<size_t> gcHeapCapacityHistory;
    size_t layers { 0 };
    size_t images { 0 };
    size_t jitCode { 0 };
    size_t libcMalloc { 0 };
    size_t bmalloc { 0 };
    size_t sumDirty { 0 };

    HashSet<CALayer *> overlayLayers;
    JSC::VM* vm { nullptr };
};

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

    [overlay().layer().platformLayer() addSublayer:m_layer.get()];

    [m_layer.get() setContentsScale:2.0];
    [m_layer.get() setBackgroundColor:adoptCF(CGColorCreateGenericRGB(0, 0, 0, 0.8)).get()];
    [m_layer.get() setFrame:CGRectMake(0, 0, normalWidth, normalHeight)];

    data.overlayLayers.add(m_layer.get());
}

void ResourceUsageOverlay::platformDestroy()
{
    auto& data = sharedData();
    LockHolder locker(data.lock);
    data.overlayLayers.remove(m_layer.get());
}

static void drawCpuHistory(GraphicsContext& gc, float x1, float y1, float y2, RingBuffer<float>& history)
{
    Ref<Gradient> gradient = Gradient::create(FloatPoint(0, y1), FloatPoint(0, y2));
    gradient->addColorStop(0.0, Color(255, 0, 0));
    gradient->addColorStop(0.5, Color(255, 255, 0));
    gradient->addColorStop(1.0, Color(0, 255, 0));
    gc.setStrokeGradient(WTF::move(gradient));
    gc.setStrokeThickness(1);

    int i = 0;

    history.forEach([&](float c) {
        float cpu = c / 100;
        float yScale = y2 - y1;

        Path path;
        path.moveTo(FloatPoint(x1 + i, y2));
        path.addLineTo(FloatPoint(x1 + i, y2 - (yScale * cpu)));
        gc.strokePath(path);
        i++;
    });
}

static void drawGCHistory(GraphicsContext& gc, float x1, float y1, float y2, RingBuffer<size_t>& sizeHistory, RingBuffer<size_t>& capacityHistory)
{
    size_t peak = 0;
    capacityHistory.forEach([&](size_t m) {
        if (m > peak)
            peak = m;
    });

    gc.setStrokeThickness(1);

    Ref<Gradient> capacityGradient = Gradient::create(FloatPoint(0, y1), FloatPoint(0, y2));
    capacityGradient->addColorStop(0.0, Color(0xFF, 0x00, 0x00));
    capacityGradient->addColorStop(0.5, Color(0xFF, 0x00, 0x33));
    capacityGradient->addColorStop(1.0, Color(0xCC, 0x00, 0x33));
    gc.setStrokeGradient(WTF::move(capacityGradient));

    size_t i = 0;

    capacityHistory.forEach([&](size_t m) {
        float mem = (float)m / (float)peak;
        float yScale = y2 - y1;

        Path path;
        path.moveTo(FloatPoint(x1 + i, y2));
        path.addLineTo(FloatPoint(x1 + i, y2 - (yScale * mem)));
        gc.strokePath(path);
        i++;
    });

    Ref<Gradient> sizeGradient = Gradient::create(FloatPoint(0, y1), FloatPoint(0, y2));
    sizeGradient->addColorStop(0.0, Color(0x96, 0xB1, 0xD2));
    sizeGradient->addColorStop(0.5, Color(0x47, 0x71, 0xA5));
    sizeGradient->addColorStop(1.0, Color(0x29, 0x56, 0x8F));
    gc.setStrokeGradient(WTF::move(sizeGradient));

    i = 0;

    sizeHistory.forEach([&](size_t m) {
        float mem = (float)m / (float)peak;
        float yScale = y2 - y1;

        Path path;
        path.moveTo(FloatPoint(x1 + i, y2));
        path.addLineTo(FloatPoint(x1 + i, y2 - (yScale * mem)));
        gc.strokePath(path);
        i++;
    });
}

static const float fullCircleInRadians = piFloat * 2;

static void drawSlice(GraphicsContext& context, FloatPoint center, float& angle, size_t sliceSize, size_t totalSize, Color color)
{
    Path path;
    path.moveTo(center);
    float part = (float)sliceSize / (float)totalSize;
    path.addArc(center, 30, angle, angle + part * fullCircleInRadians, false);
    context.setFillColor(color, ColorSpaceDeviceRGB);
    context.fillPath(path);
    angle += part * fullCircleInRadians;
}

static void drawPlate(GraphicsContext& context, FloatPoint center, float& angle, Color color)
{
    Path path;
    path.moveTo(center);
    path.addArc(center, 30, angle, fullCircleInRadians, false);
    context.setFillColor(color, ColorSpaceDeviceRGB);
    context.fillPath(path);
}

static void drawMemoryPie(GraphicsContext& context, float x, float y, ResourceUsageData& data)
{
    GraphicsContextStateSaver saver(context);

    context.setShouldAntialias(true);

    FloatPoint center(x - 15, y + 60);

    size_t bmallocWithDeductions = data.bmalloc - data.gcHeapCapacityHistory.last();

    float angle = 0;
    drawSlice(context, center, angle, bmallocWithDeductions, data.sumDirty, colorForFastMalloc);
    drawSlice(context, center, angle, data.libcMalloc, data.sumDirty, colorForLibcMalloc);
    drawSlice(context, center, angle, data.gcHeapCapacityHistory.last(), data.sumDirty, colorForGCHeap);
    drawSlice(context, center, angle, data.layers, data.sumDirty, colorForLayers);
    drawSlice(context, center, angle, data.images, data.sumDirty, colorForImages);
    drawSlice(context, center, angle, data.jitCode, data.sumDirty, colorForJITCode);
    drawPlate(context, center, angle, colorForOther);
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

static FontCascade& fontCascade()
{
    static NeverDestroyed<FontCascade> font;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        FontCascadeDescription fontDescription;
        fontDescription.setFamilies({ "Menlo" });
        fontDescription.setSpecifiedSize(11);
        fontDescription.setComputedSize(11);
        font.get() = FontCascade(fontDescription, 0, 0);
        font.get().update(nullptr);
    });
    return font;
}

static void showText(GraphicsContext& gc, float x, float y, Color color, const String& text)
{
    gc.setShouldSmoothFonts(false);
    gc.setTextDrawingMode(TextModeFill);
    gc.setFillColor(color, ColorSpaceDeviceRGB);
    gc.drawText(fontCascade(), TextRun(text), FloatPoint(x, y));
}

void ResourceUsageOverlay::draw(GraphicsContext& context)
{
    auto& data = sharedData();
    LockHolder locker(data.lock);

    size_t gcHeapSize = data.gcHeapSizeHistory.last();
    size_t gcHeapCapacity = data.gcHeapCapacityHistory.last();

    context.setShouldAntialias(false);
    context.setShouldSmoothFonts(false);

    context.clearRect(m_overlay->bounds());
    CGRect viewBounds = m_overlay->bounds();

    size_t bmallocWithDeductions = data.bmalloc - gcHeapCapacity;
    size_t footprintWithDeductions = data.sumDirty - data.bmalloc - data.layers - data.images - data.libcMalloc - data.jitCode;

    showText(context, 10,  20, colorForLabels, String::format("        CPU: %g", data.cpuHistory.last()));
    showText(context, 10,  30, colorForLabels,     "  Footprint: " + formatByteNumber(data.sumDirty));

    showText(context, 10,  50, colorForFastMalloc, "    bmalloc: " + formatByteNumber(bmallocWithDeductions));
    showText(context, 10,  60, colorForLibcMalloc, "libc malloc: " + formatByteNumber(data.libcMalloc));
    showText(context, 10,  70, colorForImages,     "     Images: " + formatByteNumber(data.images));
    showText(context, 10,  80, colorForLayers,     "     Layers: " + formatByteNumber(data.layers));
    showText(context, 10,  90, colorForJITCode,    "     JS JIT: " + formatByteNumber(data.jitCode));
    showText(context, 10, 100, colorForGCHeap,     "    GC heap: " + formatByteNumber(gcHeapSize) + " (" + formatByteNumber(gcHeapCapacity) + ")");
    showText(context, 10, 110, colorForOther,      "      Other: " + formatByteNumber(footprintWithDeductions));

    drawCpuHistory(context, m_overlay->frame().width() - 50, 0, viewBounds.size.height, data.cpuHistory);
    drawGCHistory(context, m_overlay->frame().width() - 100, 0, viewBounds.size.height, data.gcHeapSizeHistory, data.gcHeapCapacityHistory);
    drawMemoryPie(context, m_overlay->frame().width() - 150, 0, data);
}

static std::array<size_t, 256> dirtyPagesPerVMTag()
{
    std::array<size_t, 256> dirty;
    dirty.fill(0);
    task_t task = mach_task_self();
    mach_vm_size_t size;
    uint32_t depth = 0;
    struct vm_region_submap_info_64 info = { };
    mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
    for (mach_vm_address_t addr = 0; ; addr += size) {
        kern_return_t kr = mach_vm_region_recurse(task, &addr, &size, &depth, (vm_region_info_t)&info, &count);
        if (kr != KERN_SUCCESS)
            break;
        dirty[info.user_tag] += info.pages_dirtied;
    }
    return dirty;
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

NO_RETURN void runSamplerThread(void*)
{
    static size_t vmPageSize = getpagesize();
    auto& data = sharedData();
    while (1) {
        float cpu = cpuUsage();
        auto dirtyPages = dirtyPagesPerVMTag();
        Vector<CALayer *, 8> layers;

        {
            LockHolder locker(data.lock);
            data.cpuHistory.append(cpu);
            data.layers = (dirtyPages[VM_MEMORY_IOKIT] + dirtyPages[VM_MEMORY_LAYERKIT]) * vmPageSize;
            data.images = (dirtyPages[VM_MEMORY_IMAGEIO] + dirtyPages[VM_MEMORY_CGIMAGE]) * vmPageSize;
            data.jitCode = dirtyPages[VM_MEMORY_JAVASCRIPT_JIT_EXECUTABLE_ALLOCATOR] * vmPageSize;
            data.libcMalloc = vmPageSize *
                (dirtyPages[VM_MEMORY_MALLOC]
                + dirtyPages[VM_MEMORY_MALLOC_HUGE]
                + dirtyPages[VM_MEMORY_MALLOC_LARGE]
                + dirtyPages[VM_MEMORY_MALLOC_SMALL]
                + dirtyPages[VM_MEMORY_MALLOC_TINY]
                + dirtyPages[VM_MEMORY_MALLOC_NANO]);
            data.bmalloc = vmPageSize * dirtyPages[VM_MEMORY_TCMALLOC];

            data.sumDirty = 0;
            for (auto dirty : dirtyPages)
                data.sumDirty += dirty;
            data.sumDirty *= vmPageSize;

            copyToVector(data.overlayLayers, layers);

            data.gcHeapCapacityHistory.append(data.vm->heap.blockBytesAllocated());
        }

        [CATransaction begin];
        for (CALayer *layer : layers)
            [layer setNeedsDisplay];
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
