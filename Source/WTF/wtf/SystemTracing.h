/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <wtf/FastMalloc.h>
#include <wtf/Platform.h>

#if USE(APPLE_INTERNAL_SDK)
#include <sys/kdebug_private.h>
#define HAVE_KDEBUG_H 1
#endif

#if OS(DARWIN)
#include <cinttypes>
#include <sys/types.h>
#endif

// No namespaces because this file has to be includable from C and Objective-C.

// Reserved kdebug codes. Do not change these.
#define DBG_APPS_WEBKIT_MISC 0xFF
#define WEBKIT_COMPONENT 47

// Trace point codes can be up to 14 bits (0-16383).
// When adding or changing these codes, update Source/WebKit/Resources/Signposts/SystemTracePoints.plist to match.
enum TracePointCode {
    WTFRange = 0,

    JavaScriptRange = 2500,
    VMEntryScopeStart,
    VMEntryScopeEnd,
    WebAssemblyCompileStart,
    WebAssemblyCompileEnd,
    WebAssemblyExecuteStart,
    WebAssemblyExecuteEnd,
    DumpJITMemoryStart,
    DumpJITMemoryStop,
    FromJSStart,
    FromJSStop,
    IncrementalSweepStart,
    IncrementalSweepEnd,

    WebCoreRange = 5000,
    MainResourceLoadDidStartProvisional,
    MainResourceLoadDidEnd,
    SubresourceLoadWillStart,
    SubresourceLoadDidEnd,
    FetchCookiesStart,
    FetchCookiesEnd,
    StyleRecalcStart,
    StyleRecalcEnd,
    RenderTreeBuildStart,
    RenderTreeBuildEnd,
    PerformLayoutStart,
    PerformLayoutEnd,
    PaintLayerStart,
    PaintLayerEnd,
    AsyncImageDecodeStart,
    AsyncImageDecodeEnd,
    RAFCallbackStart,
    RAFCallbackEnd,
    MemoryPressureHandlerStart,
    MemoryPressureHandlerEnd,
    UpdateTouchRegionsStart,
    UpdateTouchRegionsEnd,
    DisplayListRecordStart,
    DisplayListRecordEnd,
    DisplayRefreshDispatchingToMainThread,
    ComputeEventRegionsStart,
    ComputeEventRegionsEnd,
    ScheduleRenderingUpdate,
    TriggerRenderingUpdate,
    RenderingUpdateStart,
    RenderingUpdateEnd,
    CompositingUpdateStart,
    CompositingUpdateEnd,
    DispatchTouchEventsStart,
    DispatchTouchEventsEnd,
    ParseHTMLStart,
    ParseHTMLEnd,
    DisplayListReplayStart,
    DisplayListReplayEnd,
    ScrollingThreadRenderUpdateSyncStart,
    ScrollingThreadRenderUpdateSyncEnd,
    ScrollingThreadDisplayDidRefreshStart,
    ScrollingThreadDisplayDidRefreshEnd,
    ScrollingTreeDisplayDidRefresh,
    RenderTreeLayoutStart,
    RenderTreeLayoutEnd,
    PerformOpportunisticallyScheduledTasksStart,
    PerformOpportunisticallyScheduledTasksEnd,
    WebXRLayerStartFrameStart,
    WebXRLayerStartFrameEnd,
    WebXRLayerEndFrameStart,
    WebXRLayerEndFrameEnd,
    WebXRSessionFrameCallbacksStart,
    WebXRSessionFrameCallbacksEnd,
    ProgrammaticScroll,
    FixedContainerEdgeSamplingStart,
    FixedContainerEdgeSamplingEnd,
    ThreadTimersStart,
    ThreadTimersEnd,
    TimerFiredStart,
    TimerFiredEnd,
    CoreImageRenderStart,
    CoreImageRenderEnd,

    WebKitRange = 10000,
    WebHTMLViewPaintStart,
    WebHTMLViewPaintEnd,

    WebKit2Range = 12000,
    BackingStoreFlushStart,
    BackingStoreFlushEnd,
    BuildTransactionStart,
    BuildTransactionEnd,
    SyncMessageStart,
    SyncMessageEnd,
    SyncTouchEventStart,
    SyncTouchEventEnd,
    InitializeWebProcessStart,
    InitializeWebProcessEnd,
    RenderingUpdateRunLoopObserverStart,
    RenderingUpdateRunLoopObserverEnd,
    LayerTreeFreezeStart,
    LayerTreeFreezeEnd,
    FlushRemoteImageBufferStart,
    FlushRemoteImageBufferEnd,
    CreateInjectedBundleStart,
    CreateInjectedBundleEnd,
    PaintSnapshotStart,
    PaintSnapshotEnd,
    RenderServerSnapshotStart,
    RenderServerSnapshotEnd,
    TakeSnapshotStart,
    TakeSnapshotEnd,
    SyntheticMomentumStart,
    SyntheticMomentumEnd,
    SyntheticMomentumEvent,
    RemoteLayerTreeScheduleRenderingUpdate,
    DisplayLinkUpdate,
    ProcessInitializeStart,
    ProcessInitializeEnd,

    UIProcessRange = 14000,
    CommitLayerTreeStart,
    CommitLayerTreeEnd,
    ProcessLaunchStart,
    ProcessLaunchEnd,
    InitializeSandboxStart,
    InitializeSandboxEnd,
    WebXRCPFrameWaitStart,
    WebXRCPFrameWaitEnd,
    WebXRCPFrameStartSubmissionStart,
    WebXRCPFrameStartSubmissionEnd,
    WebXRCPFrameEndSubmissionStart,
    WebXRCPFrameEndSubmissionEnd,
    TextExtractionStart,
    TextExtractionEnd,

    GPUProcessRange = 16000,
    WakeUpAndApplyDisplayListStart,
    WakeUpAndApplyDisplayListEnd,

#if PLATFORM(GTK) || PLATFORM(WPE)
    GTKWPEPortRange = 20000,

    LayerTreeHostRenderingUpdateStart,
    LayerTreeHostRenderingUpdateEnd,
    WaitForCompositionCompletionStart,
    WaitForCompositionCompletionEnd,
    RenderLayerTreeStart,
    RenderLayerTreeEnd,
    UpdateLayerContentBuffersStart,
    UpdateLayerContentBuffersEnd,
#endif

};

#ifdef __cplusplus

// This has to be included after the TracePointCode enum.
#if USE(SYSPROF_CAPTURE)
#include <wtf/glib/SysprofAnnotator.h>
#endif

namespace WTF {

inline void tracePoint(TracePointCode code, uint64_t data1 = 0, uint64_t data2 = 0, uint64_t data3 = 0, uint64_t data4 = 0)
{
#if HAVE(KDEBUG_H)
    kdebug_trace(ARIADNEDBG_CODE(WEBKIT_COMPONENT, code), data1, data2, data3, data4);
#elif USE(SYSPROF_CAPTURE)
    if (auto* annotator = SysprofAnnotator::singletonIfCreated())
        annotator->tracePoint(code);
    UNUSED_PARAM(data1);
    UNUSED_PARAM(data2);
    UNUSED_PARAM(data3);
    UNUSED_PARAM(data4);
#else
    UNUSED_PARAM(code);
    UNUSED_PARAM(data1);
    UNUSED_PARAM(data2);
    UNUSED_PARAM(data3);
    UNUSED_PARAM(data4);
#endif
}

class TraceScope {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(TraceScope);
public:

    TraceScope(TracePointCode entryCode, TracePointCode exitCode, uint64_t data1 = 0, uint64_t data2 = 0, uint64_t data3 = 0, uint64_t data4 = 0)
        : m_exitCode(exitCode)
    {
        tracePoint(entryCode, data1, data2, data3, data4);
    }

    ~TraceScope()
    {
        tracePoint(m_exitCode);
    }

private:
    TracePointCode m_exitCode;
};

} // namespace WTF

using WTF::TraceScope;
using WTF::tracePoint;

#endif // __cplusplus

#if HAVE(OS_SIGNPOST)

#include <os/signpost.h>
#include <wtf/Seconds.h>

WTF_EXTERN_C_BEGIN

WTF_EXPORT_PRIVATE extern bool WTFSignpostIndirectLoggingEnabled;

WTF_EXPORT_PRIVATE os_log_t WTFSignpostLogHandle();
WTF_EXPORT_PRIVATE bool WTFSignpostHandleIndirectLog(os_log_t, pid_t, std::span<const char> nullTerminatedLogString);

WTF_EXPORT_PRIVATE uint64_t WTFCurrentContinuousTime(Seconds deltaFromNow);

WTF_EXTERN_C_END

#define FOR_EACH_WTF_SIGNPOST_NAME(M) \
    M(AccessibilityIsolatedTreeApplyPendingChanges) \
    M(InitialAccessibilityIsolatedTreeBuild) \
    M(DataTask) \
    M(NavigationAndPaintTiming) \
    M(ExecuteScriptElement) \
    M(RegisterImportMap) \
    M(JSCGarbageCollector) \
    M(JSCJITCompiler) \
    M(JSCJITPlanQueued) \
    M(JSCJITPlanReady) \
    M(JSCJSGlobalObject) \
    M(IPCConnection) \
    M(StreamClientConnection) \
    M(ScrollingPerformanceTestFingerDownInterval) \
    M(ScrollingPerformanceTestMomentumInterval) \
    M(UpdateAccessibilityIsolatedTree) \
    M(WebKitPerformance) \
    M(UserScript) \
    M(ProcessPrewarming) \
    M(JSScriptRef) \
    M(NetworkCacheHit) \
    M(NetworkCacheMiss) \
    M(PLTSubresourceLoading) \
    M(EvaluateJavaScript) \

#define DECLARE_WTF_SIGNPOST_NAME_ENUM(name) WTFOSSignpostName ## name,

enum WTFOSSignpostName {
    FOR_EACH_WTF_SIGNPOST_NAME(DECLARE_WTF_SIGNPOST_NAME_ENUM)
    WTFOSSignpostNameCount,
};

enum WTFOSSignpostType {
    WTFOSSignpostTypeEmitEvent,
    WTFOSSignpostTypeBeginInterval,
    WTFOSSignpostTypeEndInterval,
    WTFOSSignpostTypeCount,
};

#endif

#if HAVE(OS_SIGNPOST)

#if HAVE(KDEBUG_H)
// By default, os_signpost always emits signpost data to logd. We want to avoid that for WebKit
// signposts. Instead, we use kdebug_is_enabled to make WebKit's os_signposts behave like kdebug
// trace points (i.e. we only enable them if a tracing tool is active).
#define WTFSignpostsEnabled() kdebug_is_enabled(KDBG_EVENTID(DBG_APPS, DBG_APPS_WEBKIT_MISC, 0))
#else
#define WTFSignpostsEnabled() true
#endif

// The first argument to WTF{Emit,Begin,End}Signpost is a pointer or uintptr_t that can be used to
// disambiguate nested intervals with the same name (i.e. used to create an os_signpost_id). If you
// don't care about handling nested intervals, then pass `nullptr` as the pointer argument.
//
// The second argument to these signpost APIs is a signpost name that must be listed in the
// FOR_EACH_WTF_SIGNPOST_NAME macro above.
//
// The third (or more) arguments are an optional os_log-compatible format string followed by
// arguments for the format string.
//
// These macros will emit signposts into logd's buffer only when the DBG_APPS kdebug class is
// enabled. The DBG_APPS kdebug class is enabled automatically by certain internal profiling tools,
// but is not enabled by tailspin. This means we generally emit signposts only if a developer is
// actively profiling their system.
#define WTFEmitSignpost(pointer, name, ...) WTFEmitSignpostWithTimeDelta((pointer), name, Seconds(), ##__VA_ARGS__)
#define WTFBeginSignpost(pointer, name, ...) WTFBeginSignpostWithTimeDelta((pointer), name, Seconds(), ##__VA_ARGS__)
#define WTFEndSignpost(pointer, name, ...) WTFEndSignpostWithTimeDelta((pointer), name, Seconds(), ##__VA_ARGS__)

#define WTFEmitSignpostAlways(pointer, name, ...) WTFEmitSignpostAlwaysWithTimeDelta((pointer), name, Seconds(), ##__VA_ARGS__)
#define WTFBeginSignpostAlways(pointer, name, ...) WTFBeginSignpostAlwaysWithTimeDelta((pointer), name, Seconds(), ##__VA_ARGS__)
#define WTFEndSignpostAlways(pointer, name, ...) WTFEndSignpostAlwaysWithTimeDelta((pointer), name, Seconds(), ##__VA_ARGS__)

// These work like WTF{Emit,Begin,End}Signpost, but offset the timestamp associated with the event
// by timeDelta (which should of type WTF::Seconds).
#define WTFEmitSignpostWithTimeDelta(pointer, name, timeDelta, ...) \
    WTFEmitSignpostWithType(WTFOSSignpostTypeEmitEvent, os_signpost_event_emit, (pointer), name, WTFCurrentContinuousTime(timeDelta), " %{signpost.description:event_time}llu", "" __VA_ARGS__)

#define WTFBeginSignpostWithTimeDelta(pointer, name, timeDelta, ...) \
    WTFEmitSignpostWithType(WTFOSSignpostTypeBeginInterval, os_signpost_interval_begin, (pointer), name, WTFCurrentContinuousTime(timeDelta), " %{signpost.description:begin_time}llu", "" __VA_ARGS__)

#define WTFEndSignpostWithTimeDelta(pointer, name, timeDelta, ...) \
    WTFEmitSignpostWithType(WTFOSSignpostTypeEndInterval, os_signpost_interval_end, (pointer), name, WTFCurrentContinuousTime(timeDelta), " %{signpost.description:end_time}llu", "" __VA_ARGS__)

#define WTFEmitSignpostAlwaysWithTimeDelta(pointer, name, timeDelta, ...) \
    WTFEmitSignpostAlwaysWithType(WTFOSSignpostTypeEmitEvent, os_signpost_event_emit, (pointer), name, WTFCurrentContinuousTime(timeDelta), " %{signpost.description:event_time}llu", "" __VA_ARGS__)

#define WTFBeginSignpostAlwaysWithTimeDelta(pointer, name, timeDelta, ...) \
    WTFEmitSignpostAlwaysWithType(WTFOSSignpostTypeBeginInterval, os_signpost_interval_begin, (pointer), name, WTFCurrentContinuousTime(timeDelta), " %{signpost.description:begin_time}llu", "" __VA_ARGS__)

#define WTFEndSignpostAlwaysWithTimeDelta(pointer, name, timeDelta, ...) \
    WTFEmitSignpostAlwaysWithType(WTFOSSignpostTypeEndInterval, os_signpost_interval_end, (pointer), name, WTFCurrentContinuousTime(timeDelta), " %{signpost.description:end_time}llu", "" __VA_ARGS__)


// These work like WTF{Emit,Begin,End}Signpost, but specific platform timestamp associated with the event.
#define WTFEmitSignpostWithSpecificTime(pointer, name, specificTime, ...) \
    WTFEmitSignpostWithType(WTFOSSignpostTypeEmitEvent, os_signpost_event_emit, (pointer), name, (specificTime), " %{signpost.description:event_time}llu", "" __VA_ARGS__)

#define WTFBeginSignpostWithSpecificTime(pointer, name, specificTime, ...) \
    WTFEmitSignpostWithType(WTFOSSignpostTypeBeginInterval, os_signpost_interval_begin, (pointer), name, (specificTime), " %{signpost.description:begin_time}llu", "" __VA_ARGS__)

#define WTFEndSignpostWithSpecificTime(pointer, name, specificTime, ...) \
    WTFEmitSignpostWithType(WTFOSSignpostTypeEndInterval, os_signpost_interval_end, (pointer), name, (specificTime), " %{signpost.description:end_time}llu", "" __VA_ARGS__)

#define WTFEmitSignpostAlwaysWithSpecificTime(pointer, name, specificTime, ...) \
    WTFEmitSignpostAlwaysWithType(WTFOSSignpostTypeEmitEvent, os_signpost_event_emit, (pointer), name, (specificTime), " %{signpost.description:event_time}llu", "" __VA_ARGS__)

#define WTFBeginSignpostAlwaysWithSpecificTime(pointer, name, specificTime, ...) \
    WTFEmitSignpostAlwaysWithType(WTFOSSignpostTypeBeginInterval, os_signpost_interval_begin, (pointer), name, (specificTime), " %{signpost.description:begin_time}llu", "" __VA_ARGS__)

#define WTFEndSignpostAlwaysWithSpecificTime(pointer, name, specificTime, ...) \
    WTFEmitSignpostAlwaysWithType(WTFOSSignpostTypeEndInterval, os_signpost_interval_end, (pointer), name, (specificTime), " %{signpost.description:end_time}llu", "" __VA_ARGS__)

#define WTFEmitSignpostWithType(type, emitMacro, pointer, name, specificTime, timeFormat, format, ...) \
    do { \
        if (WTFSignpostsEnabled()) [[unlikely]] \
            WTFEmitSignpostAlwaysWithType(type, emitMacro, pointer, name, specificTime, timeFormat, format, ##__VA_ARGS__); \
    } while (0)

#define WTFEmitSignpostAlwaysWithType(type, emitMacro, pointer, name, specificTime, timeFormat, format, ...) \
    do { \
        if (WTFSignpostIndirectLoggingEnabled) \
            WTFEmitSignpostIndirectlyWithType(type, pointer, name, specificTime, format, ##__VA_ARGS__); \
        else \
            WTFEmitSignpostDirectlyWithType(emitMacro, pointer, name, format timeFormat, ##__VA_ARGS__, specificTime); \
    } while (0)

#define WTFEmitSignpostDirectlyWithType(emitMacro, pointer, name, format, ...) \
    do { \
        RetainPtr<os_log_t> wtfHandle = WTFSignpostLogHandle(); \
        const void* wtfPointer = (const void *)(pointer); \
        os_signpost_id_t wtfSignpostID = wtfPointer ? os_signpost_id_make_with_pointer(wtfHandle.get(), wtfPointer) : OS_SIGNPOST_ID_EXCLUSIVE; \
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN \
        emitMacro(wtfHandle.get(), wtfSignpostID, #name, format, ##__VA_ARGS__); \
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END \
    } while (0)

#define WTFEmitSignpostIndirectlyWithType(type, pointer, name, specificTime, format, ...) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN SUPPRESS_UNCOUNTED_LOCAL os_log(WTFSignpostLogHandle(), "type=%d name=%d p=%" PRIuPTR " ts=%llu " format, type, WTFOSSignpostName ## name, reinterpret_cast<uintptr_t>(pointer), specificTime, ##__VA_ARGS__) WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#define WTFSetCounter(name, value) do { } while (0)

#elif USE(SYSPROF_CAPTURE)

#define WTFEmitSignpost(pointer, name, ...) \
    do { \
        IGNORE_WARNINGS_BEGIN("format-zero-length") \
        if (auto* annotator = SysprofAnnotator::singletonIfCreated()) \
            annotator->instantMark(std::span(_STRINGIFY(name)), "" __VA_ARGS__); \
        IGNORE_WARNINGS_END \
    } while (0)

#define WTFBeginSignpost(pointer, name, ...) \
    do { \
        IGNORE_WARNINGS_BEGIN("format-zero-length") \
        if (auto* annotator = SysprofAnnotator::singletonIfCreated()) \
            annotator->beginMark(pointer, std::span(_STRINGIFY(name)), "" __VA_ARGS__); \
        IGNORE_WARNINGS_END \
    } while (0)

#define WTFEndSignpost(pointer, name, ...)  \
    do { \
        IGNORE_WARNINGS_BEGIN("format-zero-length") \
        if (auto* annotator = SysprofAnnotator::singletonIfCreated()) \
            annotator->endMark(pointer, std::span(_STRINGIFY(name)), "" __VA_ARGS__); \
        IGNORE_WARNINGS_END \
    } while (0)

#define WTFEmitSignpostAlways(pointer, name, ...) WTFEmitSignpost((pointer), name, ##__VA_ARGS__)
#define WTFBeginSignpostAlways(pointer, name, ...) WTFBeginSignpost((pointer), name, ##__VA_ARGS__)
#define WTFEndSignpostAlways(pointer, name, ...) WTFEndSignpost((pointer), name, ##__VA_ARGS__)

#define WTFEmitSignpostWithTimeDelta(pointer, name, timeDelta, ...) \
    do { \
        IGNORE_WARNINGS_BEGIN("format-zero-length") \
        if (auto* annotator = SysprofAnnotator::singletonIfCreated()) \
            annotator->mark(SysprofAnnotator::currentContinuousTime(timeDelta), std::span(_STRINGIFY(name)), "" __VA_ARGS__); \
        IGNORE_WARNINGS_END \
    } while (0)

#define WTFBeginSignpostWithTimeDelta(pointer, name, timeDelta, ...) WTFEmitSignpostWithTimeDelta((pointer), name, (timeDelta), ##__VA_ARGS__)
#define WTFEndSignpostWithTimeDelta(pointer, name, timeDelta, ...) WTFEmitSignpostWithTimeDelta((pointer), name, (timeDelta), ##__VA_ARGS__)

#define WTFEmitSignpostAlwaysWithTimeDelta(pointer, name, timeDelta, ...) WTFEmitSignpostWithTimeDelta((pointer), name, (timeDelta), ##__VA_ARGS__)
#define WTFBeginSignpostAlwaysWithTimeDelta(pointer, name, timeDelta, ...) WTFBeginSignpostWithTimeDelta((pointer), name, (timeDelta), ##__VA_ARGS__)
#define WTFEndSignpostAlwaysWithTimeDelta(pointer, name, timeDelta, ...) WTFEndSignpostWithTimeDelta((pointer), name, (timeDelta), ##__VA_ARGS__)

#define WTFEmitSignpostWithSpecificTime(pointer, name, specificTime, ...) \
    do { \
        IGNORE_WARNINGS_BEGIN("format-zero-length") \
        if (auto* annotator = SysprofAnnotator::singletonIfCreated()) \
            annotator->mark(specificTime, std::span(_STRINGIFY(name)), "" __VA_ARGS__); \
        IGNORE_WARNINGS_END \
    } while (0)

#define WTFBeginSignpostWithSpecificTime(pointer, name, specificTime, ...) WTFEmitSignpostWithSpecificTime((pointer), name, (specificTime), ##__VA_ARGS__)
#define WTFEndSignpostWithSpecificTime(pointer, name, specificTime, ...) WTFEmitSignpostWithSpecificTime((pointer), name, (specificTime), ##__VA_ARGS__)

#define WTFEmitSignpostAlwaysWithSpecificTime(pointer, name, specificTime, ...) WTFEmitSignpostWithSpecificTime((pointer), name, (specificTime), ##__VA_ARGS__)
#define WTFBeginSignpostAlwaysWithSpecificTime(pointer, name, specificTime, ...) WTFBeginSignpostWithSpecificTime((pointer), name, (specificTime), ##__VA_ARGS__)
#define WTFEndSignpostAlwaysWithSpecificTime(pointer, name, specificTime, ...) WTFEndSignpostWithSpecificTime((pointer), name, (specificTime), ##__VA_ARGS__)

#define WTFSetCounter(name, value) \
    do { \
        if (auto* annotator = SysprofAnnotator::singletonIfCreated()) \
            annotator->setCounter(std::span(_STRINGIFY(name)), value); \
    } while (0)

#else

#define WTFEmitSignpost(pointer, name, ...) do { } while (0)
#define WTFBeginSignpost(pointer, name, ...) do { } while (0)
#define WTFEndSignpost(pointer, name, ...) do { } while (0)

#define WTFEmitSignpostAlways(pointer, name, ...) do { } while (0)
#define WTFBeginSignpostAlways(pointer, name, ...) do { } while (0)
#define WTFEndSignpostAlways(pointer, name, ...) do { } while (0)

#define WTFEmitSignpostWithTimeDelta(pointer, name, ...) do { } while (0)
#define WTFBeginSignpostWithTimeDelta(pointer, name, ...) do { } while (0)
#define WTFEndSignpostWithTimeDelta(pointer, name, ...) do { } while (0)

#define WTFEmitSignpostAlwaysWithTimeDelta(pointer, name, ...) do { } while (0)
#define WTFBeginSignpostAlwaysWithTimeDelta(pointer, name, ...) do { } while (0)
#define WTFEndSignpostAlwaysWithTimeDelta(pointer, name, ...) do { } while (0)

#define WTFEmitSignpostWithSpecificTime(pointer, name, ...) do { } while (0)
#define WTFBeginSignpostWithSpecificTime(pointer, name, ...) do { } while (0)
#define WTFEndSignpostWithSpecificTime(pointer, name, ...) do { } while (0)

#define WTFEmitSignpostAlwaysWithSpecificTime(pointer, name, ...) do { } while (0)
#define WTFBeginSignpostAlwaysWithSpecificTime(pointer, name, ...) do { } while (0)
#define WTFEndSignpostAlwaysWithSpecificTime(pointer, name, ...) do { } while (0)

#define WTFSetCounter(name, value) do { } while (0)

#endif
