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

#include <glib.h>
#include <sysprof-capture.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/text/ASCIILiteral.h>

namespace WTF {

class SysprofAnnotator final {
    WTF_MAKE_NONCOPYABLE(SysprofAnnotator);

    using NameAndPointer = std::pair<const void*, const void*>;
    using NameAndThread = std::pair<const void*, uint32_t>;
    using NameAndData = std::pair<const void*, uint64_t>;

    struct OngoingMark {
        int64_t time;
        uint32_t threadUID;
        Vector<char> message;
    };
    using OngoingMarks = Vector<OngoingMark, 1>;
    static constexpr size_t maxOngoingMarksPerKey = 64;

public:
    // Used for unit testing.
    using MarkWriterForTesting = void (*)(int64_t time, int64_t duration, const char* name, const char* message);

    static SysprofAnnotator* singletonIfCreated()
    {
        return s_annotator;
    }

    static int64_t currentContinuousTime(Seconds timeDelta)
    {
        return SYSPROF_CAPTURE_CURRENT_TIME + timeDelta.microsecondsAs<int64_t>();
    }

    void instantMark(std::span<const char> name, const char* description, ...) WTF_ATTRIBUTE_PRINTF(3, 4)
    {
        va_list args;
        va_start(args, description);
        writeFormattedMark(SYSPROF_CAPTURE_CURRENT_TIME, 0, name, description, args);
        va_end(args);
    }

    void mark(int64_t time, std::span<const char> name, const char* description, ...) WTF_ATTRIBUTE_PRINTF(4, 5)
    {
        va_list args;
        va_start(args, description);
        writeFormattedMark(time, 0, name, description, args);
        va_end(args);
    }

    void beginMark(const void* pointer, std::span<const char> name, const char* description, ...) WTF_ATTRIBUTE_PRINTF(4, 5)
    {
        auto time = SYSPROF_CAPTURE_CURRENT_TIME;

        Vector<char> buffer(1024);

        if (!description || description[0] == '\0') {
            buffer.resize(0);
            buffer.append('\0');
        } else {
            WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
            va_list args, copyArgs;
            va_start(args, description);
            va_copy(copyArgs, args);

            auto descriptionLength = vsnprintf(nullptr, 0, description, args);
            va_end(args);
            buffer.resize(descriptionLength + 1);

            vsnprintf(buffer.mutableSpan().data(), descriptionLength + 1, description, copyArgs);
            va_end(copyArgs);
            WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        }

        OngoingMark begin { time, Thread::currentSingleton().uid(), WTF::move(buffer) };

        Locker locker { m_lock };
        push(m_ongoingMarks, NameAndPointer { name.data(), pointer }, WTF::move(begin));
    }

    void endMark(const void* pointer, std::span<const char> name, const char* description, ...) WTF_ATTRIBUTE_PRINTF(4, 5)
    {
        auto time = SYSPROF_CAPTURE_CURRENT_TIME;
        auto threadUID = Thread::currentSingleton().uid();

        std::optional<OngoingMark> begin;
        {
            Locker locker { m_lock };
            begin = takeInnermost(m_ongoingMarks, NameAndPointer { name.data(), pointer }, threadUID);
        }

        if (begin) {
            int64_t startTime = begin->time;
            Vector<char>& buffer = begin->message;

            if (description && description[0] != '\0') {
                WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
                va_list args, copyArgs;
                va_start(args, description);
                va_copy(copyArgs, args);

                auto descriptionLength = vsnprintf(nullptr, 0, description, args);
                va_end(args);

                if (descriptionLength > 0) {
                    bool needsSeparator = buffer.size() > 1;
                    static constexpr auto separator = " | "_span;

                    auto oldSize = buffer.size();
                    buffer.resize(oldSize + (needsSeparator ? separator.size() : 0) + descriptionLength);

                    auto span = buffer.mutableSpan().subspan(oldSize - 1);
                    if (needsSeparator) {
                        memcpySpan(span, separator);
                        skip(span, separator.size());
                    }
                    vsnprintf(span.data(), descriptionLength + 1, description, copyArgs);
                }
                va_end(copyArgs);
                WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
            }

            writeMark(startTime, time - startTime, name, buffer.span().data());
        } else {
            va_list args;
            va_start(args, description);
            writeFormattedMark(time, 0, name, description, args);
            va_end(args);
        }
    }

    void tracePoint(TracePointCode code, uint64_t data1 = 0)
    {
        auto name = [code] {
            return tracePointCodeName(code).spanIncludingNullTerminator();
        };

        switch (code) {
        // These pass the same data at their begin and their end, which tells apart what they trace: the load, the page of
        // a main resource load, the launched process, the flush, or the number a script picked. Pair them by that data,
        // so they pair even when they end on another thread.
        case SubresourceLoadWillStart:
            // A redirect begins the load again without having ended it, and the load is timed from its first begin.
            beginTracePoint(data1, name(), BegunAgain::KeepsFirstBegin);
            break;
        case MainResourceLoadDidStartProvisional:
        case FlushRemoteImageBufferStart:
        case ProcessLaunchStart:
        case FromJSStart:
            beginTracePoint(data1, name(), BegunAgain::ReplacesBegin);
            break;
        case MainResourceLoadDidEnd:
        case SubresourceLoadDidEnd:
        case FlushRemoteImageBufferEnd:
        case ProcessLaunchEnd:
        case FromJSStop:
            endTracePoint(data1, name());
            break;

        // This begins on the main thread and ends on the scrolling thread, and its data is a flag at the end only.
        case ScrollingThreadRenderUpdateSyncStart:
            beginMark(nullptr, name(), "%s", "");
            break;
        case ScrollingThreadRenderUpdateSyncEnd:
            endMark(nullptr, name(), "%s", "");
            break;

        // The rest begin and end on one thread, and carry a measurement or a flag rather than what they trace.
        case VMEntryScopeStart:
        case WebAssemblyCompileStart:
        case WebAssemblyExecuteStart:
        case DumpJITMemoryStart:
        case IncrementalSweepStart:
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
        case RenderLayerTreeStart:
        case LayerTreeHostRenderingUpdateStart:
        case SyncMessageStart:
        case SyncTouchEventStart:
        case InitializeWebProcessStart:
        case RenderingUpdateRunLoopObserverStart:
        case LayerTreeFreezeStart:
        case CreateInjectedBundleStart:
        case PaintSnapshotStart:
        case RenderServerSnapshotStart:
        case TakeSnapshotStart:
        case SyntheticMomentumStart:
        case ProcessInitializeStart:
        case UpdateLayerContentBuffersStart:
        case CommitLayerTreeStart:
        case InitializeSandboxStart:
        case WebXRCPFrameWaitStart:
        case WebXRCPFrameStartSubmissionStart:
        case WebXRCPFrameEndSubmissionStart:
        case WakeUpAndApplyDisplayListStart:
        case ThreadTimersStart:
        case TimerFiredStart:
        case CoreImageRenderStart:
        case TextExtractionStart:
        case RemoteLayerTreeAnimationsUpdateStart:
            beginScope(name());
            break;

        case VMEntryScopeEnd:
        case WebAssemblyCompileEnd:
        case WebAssemblyExecuteEnd:
        case DumpJITMemoryStop:
        case IncrementalSweepEnd:
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
        case ScrollingThreadDisplayDidRefreshEnd:
        case RenderTreeLayoutEnd:
        case PerformOpportunisticallyScheduledTasksEnd:
        case WebXRLayerStartFrameEnd:
        case WebXRLayerEndFrameEnd:
        case WebXRSessionFrameCallbacksEnd:
        case ProgrammaticScroll:
        case FixedContainerEdgeSamplingStart:
        case FixedContainerEdgeSamplingEnd:
        case WebHTMLViewPaintEnd:
        case BackingStoreFlushEnd:
        case WaitForCompositionCompletionEnd:
        case RenderLayerTreeEnd:
        case LayerTreeHostRenderingUpdateEnd:
        case BuildTransactionEnd:
        case SyncMessageEnd:
        case SyncTouchEventEnd:
        case InitializeWebProcessEnd:
        case RenderingUpdateRunLoopObserverEnd:
        case LayerTreeFreezeEnd:
        case CreateInjectedBundleEnd:
        case PaintSnapshotEnd:
        case RenderServerSnapshotEnd:
        case TakeSnapshotEnd:
        case SyntheticMomentumEnd:
        case ProcessInitializeEnd:
        case UpdateLayerContentBuffersEnd:
        case CommitLayerTreeEnd:
        case InitializeSandboxEnd:
        case WebXRCPFrameWaitEnd:
        case WebXRCPFrameStartSubmissionEnd:
        case WebXRCPFrameEndSubmissionEnd:
        case WakeUpAndApplyDisplayListEnd:
        case ThreadTimersEnd:
        case TimerFiredEnd:
        case CoreImageRenderEnd:
        case TextExtractionEnd:
        case RemoteLayerTreeAnimationsUpdateEnd:
            endScope(name());
            break;

        case DisplayRefreshDispatchingToMainThread:
        case ScheduleRenderingUpdate:
        case TriggerRenderingUpdate:
        case ScrollingTreeDisplayDidRefresh:
        case SyntheticMomentumEvent:
        case RemoteLayerTreeScheduleRenderingUpdate:
        case DisplayLinkUpdate:
            instantMark(name(), "%s", "");
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

    void setCounter(std::span<const char> name, int64_t value)
    {
        Locker locker { m_countersLock };

        if (auto id = m_counters.getOptional(static_cast<const void*>(name.data()))) {
            SysprofCaptureCounterValue counterValue;
            counterValue.v64 = value;
            sysprof_collector_set_counters(&id.value(), &counterValue, 1);
        } else {
            unsigned newId = sysprof_collector_request_counters(1);

            // Temporary workaround for libsysprof-capture providing conflicting IDs to threads.
            static unsigned maxId = 0;
            if (newId <= maxId)
                newId = sysprof_collector_request_counters(maxId - newId + 1) + maxId - newId;
            maxId = newId;

            m_counters.add(static_cast<const void*>(name.data()), newId);

            SysprofCaptureCounter counter = { };
            counter.id = newId;
            counter.type = SYSPROF_CAPTURE_COUNTER_INT64;
            counter.value.v64 = value;
            g_strlcpy(counter.category, m_processName.characters(), sizeof counter.category);
            g_strlcpy(counter.name, name.data(), sizeof counter.name);
            g_strlcpy(counter.description, "", sizeof counter.description);

            sysprof_collector_define_counters(&counter, 1);
        }
    }

    static void createIfNeeded(ASCIILiteral processName)
    {
        if (!getenv("SYSPROF_CONTROL_FD"))
            return;

        create(processName);
    }

    // Used by unit testing facility, to observe the trace marks without running sysprof, or storing syscap files.
    static void createForTesting(MarkWriterForTesting writer)
    {
        s_markWriterForTesting.store(writer);
        create("Testing"_s);
    }

private:
    friend class LazyNeverDestroyed<SysprofAnnotator>;

    static void create(ASCIILiteral processName)
    {
        static LazyNeverDestroyed<SysprofAnnotator> instance;
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [&] {
            instance.construct(processName);
        });
    }

    void writeMark(int64_t time, int64_t duration, std::span<const char> name, const char* message)
    {
        if (auto writer = s_markWriterForTesting.load()) [[unlikely]] {
            writer(time, duration, name.data(), message);
            return;
        }
        sysprof_collector_mark(time, duration, m_processName, name.data(), message);
    }

    void writeFormattedMark(int64_t time, int64_t duration, std::span<const char> name, const char* description, va_list args) WTF_ATTRIBUTE_PRINTF(5, 0)
    {
        if (auto writer = s_markWriterForTesting.load()) [[unlikely]] {
            WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
            va_list lengthArgs;
            va_copy(lengthArgs, args);
            auto descriptionLength = vsnprintf(nullptr, 0, description, lengthArgs);
            va_end(lengthArgs);

            Vector<char> message(static_cast<size_t>(std::max(descriptionLength, 0)) + 1);
            vsnprintf(message.mutableSpan().data(), message.size(), description, args);
            WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

            writer(time, duration, name.data(), message.span().data());
            return;
        }

        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        sysprof_collector_mark_vprintf(time, duration, m_processName, name.data(), description, args);
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }

    enum class BegunAgain : bool { ReplacesBegin, KeepsFirstBegin };

    // Without data there is nothing to pair by, so pair the trace point as a scope instead.
    void beginTracePoint(uint64_t data, std::span<const char> name, BegunAgain begunAgain)
    {
        if (!data) {
            beginScope(name);
            return;
        }

        auto time = SYSPROF_CAPTURE_CURRENT_TIME;
        NameAndData key { name.data(), data };

        Locker locker { m_lock };
        if (begunAgain == BegunAgain::KeepsFirstBegin)
            m_ongoingTracePoints.add(key, time);
        else
            m_ongoingTracePoints.set(key, time);
    }

    void endTracePoint(uint64_t data, std::span<const char> name)
    {
        if (!data) {
            endScope(name);
            return;
        }

        auto time = SYSPROF_CAPTURE_CURRENT_TIME;

        std::optional<int64_t> startTime;
        {
            Locker locker { m_lock };
            startTime = m_ongoingTracePoints.takeOptional(NameAndData { name.data(), data });
        }

        auto start = startTime.value_or(time);
        writeMark(start, time - start, name, "");
    }

    // A scope begins and ends on the same thread, so pair it by its thread. That keeps the same scope running on two threads at once apart.
    void beginScope(std::span<const char> name)
    {
        auto time = SYSPROF_CAPTURE_CURRENT_TIME;
        auto threadUID = Thread::currentSingleton().uid();

        Locker locker { m_lock };
        push(m_ongoingScopes, NameAndThread { name.data(), threadUID }, OngoingMark { time, threadUID, { } });
    }

    void endScope(std::span<const char> name)
    {
        auto time = SYSPROF_CAPTURE_CURRENT_TIME;
        auto threadUID = Thread::currentSingleton().uid();

        std::optional<OngoingMark> begin;
        {
            Locker locker { m_lock };
            begin = takeInnermost(m_ongoingScopes, NameAndThread { name.data(), threadUID }, threadUID);
        }

        auto start = begin ? begin->time : time;
        writeMark(start, time - start, name, "");
    }

    template<typename Key>
    static void push(UncheckedKeyHashMap<Key, OngoingMarks>& ongoingMarks, const Key& key, OngoingMark&& begin)
    {
        auto& begins = ongoingMarks.add(key, OngoingMarks { }).iterator->value;
        if (begins.size() == maxOngoingMarksPerKey)
            begins.removeAt(0);
        begins.append(WTF::move(begin));
    }

    // Takes the begin that an end belongs to: the latest begin on the same thread as the end. This pairs nested marks,
    // and keeps the same mark running on two threads at once apart. If that thread has no begin, the mark began on
    // another thread, so take the latest begin from any thread.
    template<typename Key>
    static std::optional<OngoingMark> takeInnermost(UncheckedKeyHashMap<Key, OngoingMarks>& ongoingMarks, const Key& key, uint32_t threadUID)
    {
        auto iterator = ongoingMarks.find(key);
        if (iterator == ongoingMarks.end())
            return std::nullopt;

        auto& begins = iterator->value;
        auto index = begins.reverseFindIf([&](auto& begin) {
            return begin.threadUID == threadUID;
        });
        if (index == notFound)
            index = begins.size() - 1;

        auto begin = WTF::move(begins[index]);
        begins.removeAt(index);
        if (begins.isEmpty())
            ongoingMarks.remove(iterator);
        return begin;
    }

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
        case ProgrammaticScroll:
            return "ProgrammaticScroll"_s;
        case FixedContainerEdgeSamplingStart:
        case FixedContainerEdgeSamplingEnd:
            return "FixedContainerEdgeSampling"_s;
        case ThreadTimersStart:
        case ThreadTimersEnd:
            return "WebCoreThreadTimers"_s;
        case TimerFiredStart:
        case TimerFiredEnd:
            return "WebCoreTimerExecution"_s;
        case CoreImageRenderStart:
        case CoreImageRenderEnd:
            return "CoreImageRender"_s;

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
        case ProcessInitializeStart:
        case ProcessInitializeEnd:
            return "ProcessInitialize"_s;
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

        case LayerTreeHostRenderingUpdateStart:
        case LayerTreeHostRenderingUpdateEnd:
            return "LayerTreeHostRenderingUpdate"_s;
        case WaitForCompositionCompletionStart:
        case WaitForCompositionCompletionEnd:
            return "WaitForCompositionCompletion"_s;

        case TextExtractionStart:
        case TextExtractionEnd:
            return "TextExtraction"_s;

        case RenderLayerTreeStart:
        case RenderLayerTreeEnd:
            return "RenderLayerTree"_s;

        case RemoteLayerTreeAnimationsUpdateStart:
        case RemoteLayerTreeAnimationsUpdateEnd:
            return "RemoteLayerTreeAnimationsUpdate"_s;

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
        if (!s_markWriterForTesting.load())
            sysprof_collector_init();
        s_annotator = this;
    }

    ASCIILiteral m_processName;
    Lock m_lock;
    UncheckedKeyHashMap<NameAndPointer, OngoingMarks> m_ongoingMarks WTF_GUARDED_BY_LOCK(m_lock);
    UncheckedKeyHashMap<NameAndThread, OngoingMarks> m_ongoingScopes WTF_GUARDED_BY_LOCK(m_lock);
    UncheckedKeyHashMap<NameAndData, int64_t> m_ongoingTracePoints WTF_GUARDED_BY_LOCK(m_lock);
    Lock m_countersLock;
    UncheckedKeyHashMap<const void*, unsigned> m_counters WTF_GUARDED_BY_LOCK(m_countersLock);
    static SysprofAnnotator* s_annotator;
    static std::atomic<MarkWriterForTesting> s_markWriterForTesting;
};

inline SysprofAnnotator* SysprofAnnotator::s_annotator;
inline std::atomic<SysprofAnnotator::MarkWriterForTesting> SysprofAnnotator::s_markWriterForTesting;

} // namespace WTF

using WTF::SysprofAnnotator;
