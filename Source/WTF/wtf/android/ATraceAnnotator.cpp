/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <wtf/android/ATraceAnnotator.h>

#if USE(ATRACE_CAPTURE)

#include <algorithm>
#include <android/trace.h>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <span>
#include <string_view>
#include <wtf/SystemTracing.h>

namespace WTF {

// Every TracePointCode, so a slice name can be derived from the enumerator itself. rawCodeName()
// has no default case, so a code added to the enum but not here is a -Wswitch warning rather than
// a trace point that silently goes missing.
#define WTF_TRACE_POINT_CODE_LIST(M) \
    M(VMEntryScopeStart) \
    M(VMEntryScopeEnd) \
    M(WebAssemblyCompileStart) \
    M(WebAssemblyCompileEnd) \
    M(WebAssemblyExecuteStart) \
    M(WebAssemblyExecuteEnd) \
    M(DumpJITMemoryStart) \
    M(DumpJITMemoryStop) \
    M(FromJSStart) \
    M(FromJSStop) \
    M(IncrementalSweepStart) \
    M(IncrementalSweepEnd) \
    M(MainResourceLoadDidStartProvisional) \
    M(MainResourceLoadDidEnd) \
    M(SubresourceLoadWillStart) \
    M(SubresourceLoadDidEnd) \
    M(FetchCookiesStart) \
    M(FetchCookiesEnd) \
    M(StyleRecalcStart) \
    M(StyleRecalcEnd) \
    M(RenderTreeBuildStart) \
    M(RenderTreeBuildEnd) \
    M(PerformLayoutStart) \
    M(PerformLayoutEnd) \
    M(PaintLayerStart) \
    M(PaintLayerEnd) \
    M(AsyncImageDecodeStart) \
    M(AsyncImageDecodeEnd) \
    M(RAFCallbackStart) \
    M(RAFCallbackEnd) \
    M(MemoryPressureHandlerStart) \
    M(MemoryPressureHandlerEnd) \
    M(UpdateTouchRegionsStart) \
    M(UpdateTouchRegionsEnd) \
    M(DisplayListRecordStart) \
    M(DisplayListRecordEnd) \
    M(DisplayRefreshDispatchingToMainThread) \
    M(ComputeEventRegionsStart) \
    M(ComputeEventRegionsEnd) \
    M(ScheduleRenderingUpdate) \
    M(TriggerRenderingUpdate) \
    M(RenderingUpdateStart) \
    M(RenderingUpdateEnd) \
    M(CompositingUpdateStart) \
    M(CompositingUpdateEnd) \
    M(DispatchTouchEventsStart) \
    M(DispatchTouchEventsEnd) \
    M(ParseHTMLStart) \
    M(ParseHTMLEnd) \
    M(DisplayListReplayStart) \
    M(DisplayListReplayEnd) \
    M(ScrollingThreadRenderUpdateSyncStart) \
    M(ScrollingThreadRenderUpdateSyncEnd) \
    M(ScrollingThreadDisplayDidRefreshStart) \
    M(ScrollingThreadDisplayDidRefreshEnd) \
    M(ScrollingTreeDisplayDidRefresh) \
    M(RenderTreeLayoutStart) \
    M(RenderTreeLayoutEnd) \
    M(PerformOpportunisticallyScheduledTasksStart) \
    M(PerformOpportunisticallyScheduledTasksEnd) \
    M(WebXRLayerStartFrameStart) \
    M(WebXRLayerStartFrameEnd) \
    M(WebXRLayerEndFrameStart) \
    M(WebXRLayerEndFrameEnd) \
    M(WebXRSessionFrameCallbacksStart) \
    M(WebXRSessionFrameCallbacksEnd) \
    M(ProgrammaticScroll) \
    M(FixedContainerEdgeSamplingStart) \
    M(FixedContainerEdgeSamplingEnd) \
    M(ThreadTimersStart) \
    M(ThreadTimersEnd) \
    M(TimerFiredStart) \
    M(TimerFiredEnd) \
    M(CoreImageRenderStart) \
    M(CoreImageRenderEnd) \
    M(WebHTMLViewPaintStart) \
    M(WebHTMLViewPaintEnd) \
    M(BackingStoreFlushStart) \
    M(BackingStoreFlushEnd) \
    M(BuildTransactionStart) \
    M(BuildTransactionEnd) \
    M(SyncMessageStart) \
    M(SyncMessageEnd) \
    M(SyncTouchEventStart) \
    M(SyncTouchEventEnd) \
    M(InitializeWebProcessStart) \
    M(InitializeWebProcessEnd) \
    M(RenderingUpdateRunLoopObserverStart) \
    M(RenderingUpdateRunLoopObserverEnd) \
    M(LayerTreeFreezeStart) \
    M(LayerTreeFreezeEnd) \
    M(FlushRemoteImageBufferStart) \
    M(FlushRemoteImageBufferEnd) \
    M(CreateInjectedBundleStart) \
    M(CreateInjectedBundleEnd) \
    M(PaintSnapshotStart) \
    M(PaintSnapshotEnd) \
    M(RenderServerSnapshotStart) \
    M(RenderServerSnapshotEnd) \
    M(TakeSnapshotStart) \
    M(TakeSnapshotEnd) \
    M(SyntheticMomentumStart) \
    M(SyntheticMomentumEnd) \
    M(SyntheticMomentumEvent) \
    M(RemoteLayerTreeScheduleRenderingUpdate) \
    M(DisplayLinkUpdate) \
    M(ProcessInitializeStart) \
    M(ProcessInitializeEnd) \
    M(CommitLayerTreeStart) \
    M(CommitLayerTreeEnd) \
    M(ProcessLaunchStart) \
    M(ProcessLaunchEnd) \
    M(InitializeSandboxStart) \
    M(InitializeSandboxEnd) \
    M(WebXRCPFrameWaitStart) \
    M(WebXRCPFrameWaitEnd) \
    M(WebXRCPFrameStartSubmissionStart) \
    M(WebXRCPFrameStartSubmissionEnd) \
    M(WebXRCPFrameEndSubmissionStart) \
    M(WebXRCPFrameEndSubmissionEnd) \
    M(TextExtractionStart) \
    M(TextExtractionEnd) \
    M(RemoteLayerTreeAnimationsUpdateStart) \
    M(RemoteLayerTreeAnimationsUpdateEnd) \
    M(WakeUpAndApplyDisplayListStart) \
    M(WakeUpAndApplyDisplayListEnd) \
    M(LayerTreeHostRenderingUpdateStart) \
    M(LayerTreeHostRenderingUpdateEnd) \
    M(WaitForCompositionCompletionStart) \
    M(WaitForCompositionCompletionEnd) \
    M(RenderLayerTreeStart) \
    M(RenderLayerTreeEnd) \
    M(UpdateLayerContentBuffersStart) \
    M(UpdateLayerContentBuffersEnd)

namespace {

enum class SliceKind : uint8_t { Begin, End, Instant };

} // namespace

static const char* rawCodeName(TracePointCode code)
{
#define WTF_TRACE_POINT_CODE_NAME(name) case name: return #name;

    switch (code) {
        WTF_TRACE_POINT_CODE_LIST(WTF_TRACE_POINT_CODE_NAME)
    case WTFRange:
    case JavaScriptRange:
    case WebCoreRange:
    case WebKitRange:
    case WebKit2Range:
    case UIProcessRange:
    case GPUProcessRange:
    case GTKWPEPortRange:
        return nullptr;
    }

#undef WTF_TRACE_POINT_CODE_NAME
    return nullptr;
}

// The enumerators are named <Span><Start|Begin|End|Stop>, so the span name and whether the
// code opens or closes it both fall out of the suffix.
static SliceKind sliceKindAndName(std::string_view rawName, std::span<char> buffer)
{
    auto store = [&](std::string_view name) {
        size_t bytes = std::min(name.length(), buffer.size() - 1);
        for (size_t i = 0; i < bytes; ++i)
            buffer[i] = name[i];
        buffer[bytes] = '\0';
    };

    auto withoutSuffix = [&](size_t suffixLength) {
        return rawName.substr(0, rawName.length() - suffixLength);
    };

    if (rawName.ends_with("Start") || rawName.ends_with("Begin")) {
        store(withoutSuffix(5));
        return SliceKind::Begin;
    }
    if (rawName.ends_with("End")) {
        store(withoutSuffix(3));
        return SliceKind::End;
    }
    if (rawName.ends_with("Stop")) {
        store(withoutSuffix(4));
        return SliceKind::End;
    }
    store(rawName);
    return SliceKind::Instant;
}

// Thread-scoped slices nest, so every begin needs its end on the same thread.
static void emitThreadSlice(SliceKind kind, const char* name)
{
    switch (kind) {
    case SliceKind::Begin:
        ATrace_beginSection(name);
        break;
    case SliceKind::End:
        ATrace_endSection();
        break;
    case SliceKind::Instant:
        // ATrace has no instant event, so a zero-length slice stands in for one.
        ATrace_beginSection(name);
        ATrace_endSection();
        break;
    }
}

// Process-scoped slices pair by name and cookie, so they can end on another thread than the one
// they began on.
static void emitAsyncSlice(SliceKind kind, const char* name, int32_t cookie)
{
    if (kind == SliceKind::Begin)
        ATrace_beginAsyncSection(name, cookie);
    else
        ATrace_endAsyncSection(name, cookie);
}

// These carry the same data at their begin and at their end, which tells apart what they trace,
// and may end on another thread, so pair them by that data, as SysprofAnnotator does. Without data
// there is nothing to pair by, so they become a thread-scoped slice. Perfetto keeps only the first
// of repeated begins with the same name and cookie, so a load that begins again on a redirect is
// still timed from its first begin.
static void emitSliceByData(SliceKind kind, const char* name, uint64_t data)
{
    if (!data) {
        emitThreadSlice(kind, name);
        return;
    }
    emitAsyncSlice(kind, name, ATraceAnnotator::cookieForValue(static_cast<uintptr_t>(data)));
}

bool ATraceAnnotator::isEnabled()
{
    return ATrace_isEnabled();
}

void ATraceAnnotator::beginAsync(const char* name, int32_t cookie)
{
    if (!isEnabled())
        return;
    ATrace_beginAsyncSection(name, cookie);
}

void ATraceAnnotator::endAsync(const char* name, int32_t cookie)
{
    if (!isEnabled())
        return;
    ATrace_endAsyncSection(name, cookie);
}

void ATraceAnnotator::instant(const char* format, ...)
{
    if (!isEnabled())
        return;

    char buffer[256];
    va_list args;
    va_start(args, format);
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    vsnprintf(buffer, sizeof(buffer), format, args);
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    va_end(args);

    ATrace_beginSection(buffer);
    ATrace_endSection();
}

void ATraceAnnotator::counter(const char* name, int64_t value)
{
    if (!isEnabled())
        return;
    ATrace_setCounter(name, value);
}

void ATraceAnnotator::tracePoint(unsigned code, uint64_t data1)
{
    if (!isEnabled())
        return;

    auto traceCode = static_cast<TracePointCode>(code);
    const char* rawName = rawCodeName(traceCode);
    if (!rawName)
        return;

    std::array<char, 128> name;
    auto kind = sliceKindAndName(rawName, std::span { name });

    switch (traceCode) {
    // The begin and the end of a load are named differently, so the suffix cannot give them the
    // one name they need to pair.
    case MainResourceLoadDidStartProvisional:
        emitSliceByData(SliceKind::Begin, "MainResourceLoad", data1);
        break;
    case MainResourceLoadDidEnd:
        emitSliceByData(SliceKind::End, "MainResourceLoad", data1);
        break;
    case SubresourceLoadWillStart:
        emitSliceByData(SliceKind::Begin, "SubresourceLoad", data1);
        break;
    case SubresourceLoadDidEnd:
        emitSliceByData(SliceKind::End, "SubresourceLoad", data1);
        break;
    case FlushRemoteImageBufferStart:
    case FlushRemoteImageBufferEnd:
    case ProcessLaunchStart:
    case ProcessLaunchEnd:
    case FromJSStart:
    case FromJSStop:
        emitSliceByData(kind, name.data(), data1);
        break;
    // ThreadedScrollingTree begins this one on the main thread and ends it on the scrolling
    // thread. Only one is ever in flight per process, so a fixed cookie is enough.
    case ScrollingThreadRenderUpdateSyncStart:
    case ScrollingThreadRenderUpdateSyncEnd:
        emitAsyncSlice(kind, name.data(), 0);
        break;
    default:
        emitThreadSlice(kind, name.data());
        break;
    }
}

} // namespace WTF

#endif // USE(ATRACE_CAPTURE)
