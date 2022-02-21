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

#if USE(APPLE_INTERNAL_SDK)
#include <System/sys/kdebug.h>
#define HAVE_KDEBUG_H 1
#endif

// No namespaces because this file has to be includable from C and Objective-C.

// Reserved component code. Do not change this.
#define WEBKIT_COMPONENT 47

// Trace point codes can be up to 14 bits (0-16383).
// When adding or changing these codes, update Tools/Tracing/SystemTracePoints.plist to match.
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
    LayoutStart,
    LayoutEnd,
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

    UIProcessRange = 14000,
    CommitLayerTreeStart,
    CommitLayerTreeEnd,
    ProcessLaunchStart,
    ProcessLaunchEnd,
    InitializeSandboxStart,
    InitializeSandboxEnd,

    GPUProcessRange = 16000,
    WakeUpAndApplyDisplayListStart,
    WakeUpAndApplyDisplayListEnd,
};

#ifdef __cplusplus

namespace WTF {

inline void tracePoint(TracePointCode code, uint64_t data1 = 0, uint64_t data2 = 0, uint64_t data3 = 0, uint64_t data4 = 0)
{
#if HAVE(KDEBUG_H)
    kdebug_trace(ARIADNEDBG_CODE(WEBKIT_COMPONENT, code), data1, data2, data3, data4);
#else
    UNUSED_PARAM(code);
    UNUSED_PARAM(data1);
    UNUSED_PARAM(data2);
    UNUSED_PARAM(data3);
    UNUSED_PARAM(data4);
#endif
}

class TraceScope {
    WTF_MAKE_FAST_ALLOCATED;
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

#import <os/signpost.h>

WTF_EXTERN_C_BEGIN
WTF_EXPORT_PRIVATE bool WTFSignpostsEnabled();
WTF_EXPORT_PRIVATE os_log_t WTFSignpostLogHandle();
WTF_EXTERN_C_END

// These macros only emit signposts on internal builds when WEBKIT_SIGNPOSTS_ENABLED is set.
#define WTFEmitSignpost(pointer, name, ...) \
    WTFEmitSignpostWithFunction(os_signpost_event_emit, (pointer), name, ##__VA_ARGS__)

#define WTFBeginSignpost(pointer, name, ...) \
    WTFEmitSignpostWithFunction(os_signpost_interval_begin, (pointer), name, ##__VA_ARGS__)

#define WTFEndSignpost(pointer, name, ...) \
    WTFEmitSignpostWithFunction(os_signpost_interval_end, (pointer), name, ##__VA_ARGS__)

#define WTFEmitSignpostWithFunction(emitFunc, pointer, name, ...) \
do { \
    if (UNLIKELY(WTFSignpostsEnabled())) { \
        os_log_t handle = WTFSignpostLogHandle(); \
        os_signpost_id_t signpostID = os_signpost_id_make_with_pointer(handle, (pointer)); \
        emitFunc(handle, signpostID, name, ##__VA_ARGS__); \
    } \
} while (0)

// These macros emit signposts on all builds.
#define WTFEmitSignpostAlways(name, format, ...) \
    do { os_signpost_event_emit(WTFSignpostLogHandle(), OS_SIGNPOST_ID_EXCLUSIVE, name, format, ##__VA_ARGS__); } } while (0)

#define WTFBeginSignpostIntervalAlways(name, format, ...) \
    do { os_signpost_interval_begin(WTFSignpostLogHandle(), OS_SIGNPOST_ID_EXCLUSIVE, name, format, ##__VA_ARGS__); } while (0)

#define WTF_OS_SIGNPOST_ANIMATION_INTERVAL_TAG "isAnimation=YES"

#define WTFBeginAnimationSignpostIntervalAlways(name, format, ...) \
    do { os_signpost_interval_begin(WTFSignpostLogHandle(), OS_SIGNPOST_ID_EXCLUSIVE, name, format " " WTF_OS_SIGNPOST_ANIMATION_INTERVAL_TAG, ##__VA_ARGS__); } while (0)

#define WTFEndSignpostIntervalAlways(name, format, ...) \
    do { os_signpost_interval_end(WTFSignpostLogHandle(), OS_SIGNPOST_ID_EXCLUSIVE, name, format, ##__VA_ARGS__); } while (0)

#else

#define WTFEmitSignpost(pointer, name, ...) do { } while (0)
#define WTFBeginSignpost(pointer, name, ...) do { } while (0)
#define WTFEndSignpost(pointer, name, ...) do { } while (0)

#define WTFEmitSignpostAlways(name, format, ...) do { } while (0)
#define WTFBeginSignpostIntervalAlways(name, format, ...) do { } while (0)
#define WTFBeginAnimationSignpostIntervalAlways(name, format, ...) do { } while (0)
#define WTFEndSignpostIntervalAlways(name, format, ...) do { } while (0)

#endif
