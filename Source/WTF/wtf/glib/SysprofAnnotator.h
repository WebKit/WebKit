/*
 * Copyright (C) 2024 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#pragma once

#include <sysprof-capture.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>
#include <wtf/text/ASCIILiteral.h>

namespace WTF {

class SysprofAnnotator final {
    WTF_MAKE_NONCOPYABLE(SysprofAnnotator);

    using RawPointerPair = std::pair<const void*, const void*>;
    using TimestampAndString = std::pair<int64_t, Vector<char>>;

public:

    static SysprofAnnotator* singletonIfCreated()
    {
        return s_annotator;
    }

    void instantMark(std::span<const char> name, const char* description, ...) WTF_ATTRIBUTE_PRINTF(3, 4)
    {
        va_list args;
        va_start(args, description);
        sysprof_collector_mark_vprintf(SYSPROF_CAPTURE_CURRENT_TIME, 0, m_processName, name.data(), description, args);
        va_end(args);
    }

    void mark(Seconds timeDelta, std::span<const char> name, const char* description, ...) WTF_ATTRIBUTE_PRINTF(4, 5)
    {
        va_list args;
        va_start(args, description);
        sysprof_collector_mark_vprintf(SYSPROF_CAPTURE_CURRENT_TIME, timeDelta.microsecondsAs<int64_t>(), m_processName, name.data(), description, args);
        va_end(args);
    }

    void beginMark(const void* pointer, std::span<const char> name, const char* description, ...) WTF_ATTRIBUTE_PRINTF(4, 5)
    {
        auto key = std::make_pair(pointer, static_cast<const void*>(name.data()));

        Vector<char> buffer(1024);
        va_list args;
        va_start(args, description);
        vsnprintf(buffer.data(), buffer.size(), description, args);
        va_end(args);

        auto value = std::make_pair(SYSPROF_CAPTURE_CURRENT_TIME, WTFMove(buffer));

        Locker locker { m_lock };
        m_ongoingMarks.set(key, value);
    }

    void endMark(const void* pointer, std::span<const char> name, const char* description, ...) WTF_ATTRIBUTE_PRINTF(4, 5)
    {
        auto key = std::make_pair(pointer, static_cast<const void*>(name.data()));
        std::optional<TimestampAndString> value;

        {
            Locker locker { m_lock };

            TimestampAndString v = m_ongoingMarks.take(key);
            if (v.first)
                value = WTFMove(v);
        }

        if (value) {
            int64_t startTime = std::get<0>(*value);
            const Vector<char>& description = std::get<1>(*value);
            sysprof_collector_mark(startTime, SYSPROF_CAPTURE_CURRENT_TIME - startTime, m_processName, name.data(), description[0] ? description.data() : nullptr);
        } else {
            va_list args;
            va_start(args, description);
            sysprof_collector_mark_vprintf(SYSPROF_CAPTURE_CURRENT_TIME, 0, m_processName, name.data(), description, args);
            va_end(args);
        }
    }

    void tracePoint(TracePointCode code)
    {
        switch (code) {
        case VMEntryScopeStart:
        case WebAssemblyCompileStart:
        case WebAssemblyExecuteStart:
        case DumpJITMemoryStart:
        case FromJSStart:
        case IncrementalSweepStart:
        case MainResourceLoadDidStartProvisional:
        case SubresourceLoadWillStart:
        case FetchCookiesStart:
        case StyleRecalcStart:
        case RenderTreeBuildStart:
        case PerformLayoutStart:
        case PaintLayerStart:
        case AsyncImageDecodeStart:
        case RAFCallbackStart:
        case MemoryPressureHandlerStart:
        case UpdateTouchRegionsStart:
        case DisplayListRecordStart:
        case ComputeEventRegionsStart:
        case RenderingUpdateStart:
        case CompositingUpdateStart:
        case DispatchTouchEventsStart:
        case ParseHTMLStart:
        case DisplayListReplayStart:
        case ScrollingThreadRenderUpdateSyncStart:
        case ScrollingThreadDisplayDidRefreshStart:
        case RenderTreeLayoutStart:
        case PerformOpportunisticallyScheduledTasksStart:
        case WebXRLayerStartFrameStart:
        case WebXRLayerEndFrameStart:
        case WebXRSessionFrameCallbacksStart:
        case WebHTMLViewPaintStart:
        case BackingStoreFlushStart:
        case BuildTransactionStart:
        case WaitForCompositionCompletionStart:
        case FrameCompositionStart:
        case LayerFlushStart:
        case SyncMessageStart:
        case SyncTouchEventStart:
        case InitializeWebProcessStart:
        case RenderingUpdateRunLoopObserverStart:
        case LayerTreeFreezeStart:
        case FlushRemoteImageBufferStart:
        case CreateInjectedBundleStart:
        case PaintSnapshotStart:
        case RenderServerSnapshotStart:
        case TakeSnapshotStart:
        case SyntheticMomentumStart:
        case UpdateLayerContentBuffersStart:
        case CommitLayerTreeStart:
        case ProcessLaunchStart:
        case InitializeSandboxStart:
        case WebXRCPFrameWaitStart:
        case WebXRCPFrameStartSubmissionStart:
        case WebXRCPFrameEndSubmissionStart:
        case WakeUpAndApplyDisplayListStart:
            beginMark(nullptr, tracePointCodeName(code).spanIncludingNullTerminator(), "%s", "");
            break;

        case VMEntryScopeEnd:
        case WebAssemblyCompileEnd:
        case WebAssemblyExecuteEnd:
        case DumpJITMemoryStop:
        case FromJSStop:
        case IncrementalSweepEnd:
        case MainResourceLoadDidEnd:
        case SubresourceLoadDidEnd:
        case FetchCookiesEnd:
        case StyleRecalcEnd:
        case RenderTreeBuildEnd:
        case PerformLayoutEnd:
        case PaintLayerEnd:
        case AsyncImageDecodeEnd:
        case RAFCallbackEnd:
        case MemoryPressureHandlerEnd:
        case UpdateTouchRegionsEnd:
        case DisplayListRecordEnd:
        case ComputeEventRegionsEnd:
        case RenderingUpdateEnd:
        case CompositingUpdateEnd:
        case DispatchTouchEventsEnd:
        case ParseHTMLEnd:
        case DisplayListReplayEnd:
        case ScrollingThreadRenderUpdateSyncEnd:
        case ScrollingThreadDisplayDidRefreshEnd:
        case RenderTreeLayoutEnd:
        case PerformOpportunisticallyScheduledTasksEnd:
        case WebXRLayerStartFrameEnd:
        case WebXRLayerEndFrameEnd:
        case WebXRSessionFrameCallbacksEnd:
        case WebHTMLViewPaintEnd:
        case BackingStoreFlushEnd:
        case WaitForCompositionCompletionEnd:
        case FrameCompositionEnd:
        case LayerFlushEnd:
        case BuildTransactionEnd:
        case SyncMessageEnd:
        case SyncTouchEventEnd:
        case InitializeWebProcessEnd:
        case RenderingUpdateRunLoopObserverEnd:
        case LayerTreeFreezeEnd:
        case FlushRemoteImageBufferEnd:
        case CreateInjectedBundleEnd:
        case PaintSnapshotEnd:
        case RenderServerSnapshotEnd:
        case TakeSnapshotEnd:
        case SyntheticMomentumEnd:
        case UpdateLayerContentBuffersEnd:
        case CommitLayerTreeEnd:
        case ProcessLaunchEnd:
        case InitializeSandboxEnd:
        case WebXRCPFrameWaitEnd:
        case WebXRCPFrameStartSubmissionEnd:
        case WebXRCPFrameEndSubmissionEnd:
        case WakeUpAndApplyDisplayListEnd:
            endMark(nullptr, tracePointCodeName(code).spanIncludingNullTerminator(), "%s", "");
            break;

        case DisplayRefreshDispatchingToMainThread:
        case ScheduleRenderingUpdate:
        case TriggerRenderingUpdate:
        case ScrollingTreeDisplayDidRefresh:
        case SyntheticMomentumEvent:
        case RemoteLayerTreeScheduleRenderingUpdate:
        case DisplayLinkUpdate:
            instantMark(tracePointCodeName(code).spanIncludingNullTerminator(), "%s", "");
            break;

        case WTFRange:
        case JavaScriptRange:
        case WebCoreRange:
        case WebKitRange:
        case WebKit2Range:
        case UIProcessRange:
        case GPUProcessRange:
        case GTKWPEPortRange:
            break;
        }
    }

    static void createIfNeeded(ASCIILiteral processName)
    {
        if (!getenv("SYSPROF_CONTROL_FD"))
            return;

        static LazyNeverDestroyed<SysprofAnnotator> instance;
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [&] {
            instance.construct(processName);
        });
    }

private:
    friend class LazyNeverDestroyed<SysprofAnnotator>;

    static ASCIILiteral tracePointCodeName(TracePointCode code)
    {
        switch (code) {
        case VMEntryScopeStart:
        case VMEntryScopeEnd:
            return "VMEntryScope"_s;
        case WebAssemblyCompileStart:
        case WebAssemblyCompileEnd:
            return "WebAssemblyCompile"_s;
        case WebAssemblyExecuteStart:
        case WebAssemblyExecuteEnd:
            return "WebAssemblyExecute"_s;
        case DumpJITMemoryStart:
        case DumpJITMemoryStop:
            return "DumpJITMemory"_s;
        case FromJSStart:
        case FromJSStop:
            return "FromJS"_s;
        case IncrementalSweepStart:
        case IncrementalSweepEnd:
            return "IncrementalSweep"_s;

        case MainResourceLoadDidStartProvisional:
        case MainResourceLoadDidEnd:
            return "MainResourceLoad"_s;
        case SubresourceLoadWillStart:
        case SubresourceLoadDidEnd:
            return "SubresourceLoad"_s;
        case FetchCookiesStart:
        case FetchCookiesEnd:
            return "FetchCookies"_s;
        case StyleRecalcStart:
        case StyleRecalcEnd:
            return "StyleRecalc"_s;
        case RenderTreeBuildStart:
        case RenderTreeBuildEnd:
            return "RenderTreeBuild"_s;
        case PerformLayoutStart:
        case PerformLayoutEnd:
            return "PerformLayout"_s;
        case PaintLayerStart:
        case PaintLayerEnd:
            return "PaintLayer"_s;
        case AsyncImageDecodeStart:
        case AsyncImageDecodeEnd:
            return "AsyncImageDecode"_s;
        case RAFCallbackStart:
        case RAFCallbackEnd:
            return "RAFCallback"_s;
        case MemoryPressureHandlerStart:
        case MemoryPressureHandlerEnd:
            return "MemoryPressureHandler"_s;
        case UpdateTouchRegionsStart:
        case UpdateTouchRegionsEnd:
            return "UpdateTouchRegions"_s;
        case DisplayListRecordStart:
        case DisplayListRecordEnd:
            return "DisplayListRecord"_s;
        case DisplayRefreshDispatchingToMainThread:
            return "DisplayRefreshDispatchingToMainThread"_s;
        case ComputeEventRegionsStart:
        case ComputeEventRegionsEnd:
            return "ComputeEventRegions"_s;
        case ScheduleRenderingUpdate:
            return "ScheduleRenderingUpdate"_s;
        case TriggerRenderingUpdate:
            return "TriggerRenderingUpdate"_s;
        case RenderingUpdateStart:
        case RenderingUpdateEnd:
            return "RenderingUpdate"_s;
        case CompositingUpdateStart:
        case CompositingUpdateEnd:
            return "CompositingUpdate"_s;
        case DispatchTouchEventsStart:
        case DispatchTouchEventsEnd:
            return "DispatchTouchEvents"_s;
        case ParseHTMLStart:
        case ParseHTMLEnd:
            return "ParseHTML"_s;
        case DisplayListReplayStart:
        case DisplayListReplayEnd:
            return "DisplayListReplay"_s;
        case ScrollingThreadRenderUpdateSyncStart:
        case ScrollingThreadRenderUpdateSyncEnd:
            return "ScrollingThreadRenderUpdateSync"_s;
        case ScrollingThreadDisplayDidRefreshStart:
        case ScrollingThreadDisplayDidRefreshEnd:
            return "ScrollingThreadDisplayDidRefresh"_s;
        case ScrollingTreeDisplayDidRefresh:
            return "ScrollingTreeDisplayDidRefresh"_s;
        case RenderTreeLayoutStart:
        case RenderTreeLayoutEnd:
            return "RenderTreeLayout"_s;
        case PerformOpportunisticallyScheduledTasksStart:
        case PerformOpportunisticallyScheduledTasksEnd:
            return "PerformOpportunisticallyScheduledTasks"_s;
        case WebXRLayerStartFrameStart:
        case WebXRLayerStartFrameEnd:
            return "WebXRLayerStartFrame"_s;
        case WebXRLayerEndFrameStart:
        case WebXRLayerEndFrameEnd:
            return "WebXRLayerEndFrame"_s;
        case WebXRSessionFrameCallbacksStart:
        case WebXRSessionFrameCallbacksEnd:
            return "WebXRSessionFrameCallbacks"_s;

        case WebHTMLViewPaintStart:
        case WebHTMLViewPaintEnd:
            return "WebHTMLViewPaint"_s;

        case BackingStoreFlushStart:
        case BackingStoreFlushEnd:
            return "BackingStoreFlush"_s;
        case BuildTransactionStart:
        case BuildTransactionEnd:
            return "BuildTransaction"_s;
        case SyncMessageStart:
        case SyncMessageEnd:
            return "SyncMessage"_s;
        case SyncTouchEventStart:
        case SyncTouchEventEnd:
            return "SyncTouchEvent"_s;
        case InitializeWebProcessStart:
        case InitializeWebProcessEnd:
            return "InitializeWebProcess"_s;
        case RenderingUpdateRunLoopObserverStart:
        case RenderingUpdateRunLoopObserverEnd:
            return "RenderingUpdateRunLoopObserver"_s;
        case LayerTreeFreezeStart:
        case LayerTreeFreezeEnd:
            return "LayerTreeFreeze"_s;
        case FlushRemoteImageBufferStart:
        case FlushRemoteImageBufferEnd:
            return "FlushRemoteImageBuffer"_s;
        case CreateInjectedBundleStart:
        case CreateInjectedBundleEnd:
            return "CreateInjectedBundle"_s;
        case PaintSnapshotStart:
        case PaintSnapshotEnd:
            return "PaintSnapshot"_s;
        case RenderServerSnapshotStart:
        case RenderServerSnapshotEnd:
            return "RenderServerSnapshot"_s;
        case TakeSnapshotStart:
        case TakeSnapshotEnd:
            return "TakeSnapshot"_s;
        case SyntheticMomentumStart:
        case SyntheticMomentumEnd:
            return "SyntheticMomentum"_s;
        case SyntheticMomentumEvent:
            return "SyntheticMomentumEvent"_s;
        case RemoteLayerTreeScheduleRenderingUpdate:
            return "RemoteLayerTreeScheduleRenderingUpdate"_s;
        case DisplayLinkUpdate:
            return "DisplayLinkUpdate"_s;
        case UpdateLayerContentBuffersStart:
        case UpdateLayerContentBuffersEnd:
            return "UpdateLayerContentBuffers"_s;

        case CommitLayerTreeStart:
        case CommitLayerTreeEnd:
            return "CommitLayerTree"_s;
        case ProcessLaunchStart:
        case ProcessLaunchEnd:
            return "ProcessLaunch"_s;
        case InitializeSandboxStart:
        case InitializeSandboxEnd:
            return "InitializeSandbox"_s;
        case WebXRCPFrameWaitStart:
        case WebXRCPFrameWaitEnd:
            return "WebXRCPFrameWait"_s;
        case WebXRCPFrameStartSubmissionStart:
        case WebXRCPFrameStartSubmissionEnd:
            return "WebXRCPFrameStartSubmission"_s;
        case WebXRCPFrameEndSubmissionStart:
        case WebXRCPFrameEndSubmissionEnd:
            return "WebXRCPFrameEndSubmission"_s;

        case WakeUpAndApplyDisplayListStart:
        case WakeUpAndApplyDisplayListEnd:
            return "WakeUpAndApplyDisplayList"_s;

        case WaitForCompositionCompletionStart:
        case WaitForCompositionCompletionEnd:
            return "WaitForCompositionCompletion"_s;
        case FrameCompositionStart:
        case FrameCompositionEnd:
            return "FrameComposition"_s;
        case LayerFlushStart:
        case LayerFlushEnd:
            return "LayerFlush"_s;

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

        RELEASE_ASSERT_NOT_REACHED();
    }

    explicit SysprofAnnotator(ASCIILiteral processName)
        : m_processName(processName)
    {
        sysprof_collector_init();
        s_annotator = this;
    }

    ASCIILiteral m_processName;
    Lock m_lock;
    HashMap<RawPointerPair, TimestampAndString> m_ongoingMarks WTF_GUARDED_BY_LOCK(m_lock);
    static SysprofAnnotator* s_annotator;
};

inline SysprofAnnotator* SysprofAnnotator::s_annotator;

} // namespace WTF

using WTF::SysprofAnnotator;
