/*
 * Copyright (C) 2026 Igalia S.L.
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

#if USE(SYSPROF_CAPTURE)

#include "Helpers/Test.h"
#include <optional>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SystemTracing.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

// A mark as the annotator wrote it, with when it ended rather than how long it ran.
struct RecordedMark {
    int64_t begin;
    int64_t end;
    String name;
    String message;
};

static Lock recordedMarksLock;
static bool isRecordingMarks WTF_GUARDED_BY_LOCK(recordedMarksLock);

static Vector<RecordedMark>& recordedMarks() WTF_REQUIRES_LOCK(recordedMarksLock)
{
    static NeverDestroyed<Vector<RecordedMark>> marks;
    return marks;
}

static void recordMark(int64_t time, int64_t duration, const char* name, const char* message)
{
    Locker locker { recordedMarksLock };
    if (isRecordingMarks)
        recordedMarks().append({ time, time + duration, String::fromLatin1(name), String::fromLatin1(message) });
}

class WTF_SysprofAnnotator : public testing::Test {
public:
    void SetUp() final
    {
        SysprofAnnotator::createForTesting(recordMark);
        Locker locker { recordedMarksLock };
        recordedMarks().clear();
        isRecordingMarks = true;
    }

    void TearDown() final
    {
        Locker locker { recordedMarksLock };
        isRecordingMarks = false;
        recordedMarks().clear();
    }
};

// The marks of that name, in the order the annotator wrote them, which is the order they ended in.
static Vector<RecordedMark> marksNamed(ASCIILiteral name)
{
    Locker locker { recordedMarksLock };
    Vector<RecordedMark> marks;
    for (auto& mark : recordedMarks()) {
        if (mark.name == name)
            marks.append(mark);
    }
    return marks;
}

// A moment after everything the annotator timed before it and before everything it times after it, since it waits
// for the clock the annotator reads to move on. Two of them bracket when whatever ran between them happened.
static int64_t checkpoint()
{
    auto moment = SYSPROF_CAPTURE_CURRENT_TIME;
    while (SYSPROF_CAPTURE_CURRENT_TIME == moment) { }
    return moment;
}

// Whether the mark began after one checkpoint and by the next, and ended after one checkpoint and by the next.
static testing::AssertionResult ranBetween(const RecordedMark& mark, int64_t beganAfter, int64_t beganBy, int64_t endedAfter, int64_t endedBy)
{
    if (mark.begin > beganAfter && mark.begin <= beganBy && mark.end > endedAfter && mark.end <= endedBy)
        return testing::AssertionSuccess();
    return testing::AssertionFailure() << "The mark ran from " << mark.begin - beganAfter << " ns to " << mark.end - beganAfter
        << " ns, rather than beginning within (0, " << beganBy - beganAfter << "] ns and ending within ("
        << endedAfter - beganAfter << ", " << endedBy - beganAfter << "] ns";
}

// A message as Heap writes it for a collection.
static constexpr auto garbageCollectionMessage = "GC:(0x7f3a2c000000),mode:(Eden),version:(3),conn:(Mut),capacity(4096kb)";

// Identifiers as SubresourceLoader passes them along with its load.
static constexpr uint64_t pageIdentifier = 1;
static constexpr uint64_t frameIdentifier = 2;

TEST_F(WTF_SysprofAnnotator, SignpostCarriesTheMessagesOfItsBeginAndEnd)
{
    // Heap begins and ends a collection with the same message.
    int heap { };

    auto beforeBegin = checkpoint();
    WTFBeginSignpost(&heap, JSCGarbageCollector, "%s", garbageCollectionMessage);
    auto beforeEnd = checkpoint();
    WTFEndSignpost(&heap, JSCGarbageCollector, "%s", garbageCollectionMessage);
    auto afterEnd = checkpoint();

    auto marks = marksNamed("JSCGarbageCollector"_s);
    ASSERT_EQ(marks.size(), 1u);
    EXPECT_TRUE(ranBetween(marks[0], beforeBegin, beforeEnd, beforeEnd, afterEnd));
    EXPECT_EQ(marks[0].message, makeString(String::fromLatin1(garbageCollectionMessage), " | "_s, String::fromLatin1(garbageCollectionMessage)));
}

TEST_F(WTF_SysprofAnnotator, SignpostsOfDifferentObjectsOverlap)
{
    // Every ThreadedCompositor paints on a thread of its own, so the compositors of two web views paint at once.
    int firstCompositor { };
    int secondCompositor { };
    auto firstCompositingThread = WorkQueue::create("First compositing thread"_s);
    auto secondCompositingThread = WorkQueue::create("Second compositing thread"_s);

    auto t0 = checkpoint();
    firstCompositingThread->dispatchSync([&] {
        WTFBeginSignpost(&firstCompositor, PaintToGLContext);
    });
    auto t1 = checkpoint();
    secondCompositingThread->dispatchSync([&] {
        WTFBeginSignpost(&secondCompositor, PaintToGLContext);
    });
    auto t2 = checkpoint();
    firstCompositingThread->dispatchSync([&] {
        WTFEndSignpost(&firstCompositor, PaintToGLContext);
    });
    auto t3 = checkpoint();
    secondCompositingThread->dispatchSync([&] {
        WTFEndSignpost(&secondCompositor, PaintToGLContext);
    });
    auto t4 = checkpoint();

    auto marks = marksNamed("PaintToGLContext"_s);
    ASSERT_EQ(marks.size(), 2u);
    EXPECT_TRUE(ranBetween(marks[0], t0, t1, t2, t3));
    EXPECT_TRUE(ranBetween(marks[1], t1, t2, t3, t4));
}

TEST_F(WTF_SysprofAnnotator, SignpostEndingOnAnotherThreadThanItBegan)
{
    // A JIT plan is queued on the thread that enqueues it, and leaves the queue once a JIT worklist thread compiles it.
    int plan { };
    auto worklistThread = WorkQueue::create("JIT worklist thread"_s);

    auto beforeBegin = checkpoint();
    WTFBeginSignpost(&plan, JSCJITPlanQueued, "%s", "DFG foo#AbCdEf");
    auto beforeEnd = checkpoint();
    worklistThread->dispatchSync([&] {
        WTFEndSignpost(&plan, JSCJITPlanQueued, "%s %s", "DFG foo#AbCdEf", "");
    });
    auto afterEnd = checkpoint();

    auto marks = marksNamed("JSCJITPlanQueued"_s);
    ASSERT_EQ(marks.size(), 1u);
    EXPECT_TRUE(ranBetween(marks[0], beforeBegin, beforeEnd, beforeEnd, afterEnd));
}

TEST_F(WTF_SysprofAnnotator, SameSignpostOnTwoThreadsAtOnce)
{
    // A wasm plan compiles its functions on several worklist threads at once, and each of them begins and ends
    // JSCJITCompiler for the plan, so the same signpost of the same object runs on two threads.
    int plan { };
    auto firstWorklistThread = WorkQueue::create("First wasm worklist thread"_s);
    auto secondWorklistThread = WorkQueue::create("Second wasm worklist thread"_s);

    auto t0 = checkpoint();
    firstWorklistThread->dispatchSync([&] {
        WTFBeginSignpost(&plan, JSCJITCompiler, "%s", "IPInt 0 instructions size = 12");
    });
    auto t1 = checkpoint();
    secondWorklistThread->dispatchSync([&] {
        WTFBeginSignpost(&plan, JSCJITCompiler, "%s", "IPInt 1 instructions size = 34");
    });
    auto t2 = checkpoint();
    firstWorklistThread->dispatchSync([&] {
        WTFEndSignpost(&plan, JSCJITCompiler, "%s", "IPInt 0 instructions size = 12");
    });
    auto t3 = checkpoint();
    secondWorklistThread->dispatchSync([&] {
        WTFEndSignpost(&plan, JSCJITCompiler, "%s", "IPInt 1 instructions size = 34");
    });
    auto t4 = checkpoint();

    auto marks = marksNamed("JSCJITCompiler"_s);
    ASSERT_EQ(marks.size(), 2u);
    EXPECT_TRUE(ranBetween(marks[0], t0, t1, t2, t3));
    EXPECT_EQ(marks[0].message, "IPInt 0 instructions size = 12 | IPInt 0 instructions size = 12"_s);
    EXPECT_TRUE(ranBetween(marks[1], t1, t2, t3, t4));
    EXPECT_EQ(marks[1].message, "IPInt 1 instructions size = 34 | IPInt 1 instructions size = 34"_s);
}

TEST_F(WTF_SysprofAnnotator, SignpostEndWithoutBegin)
{
    // Recording can start while a collection runs, so the capture holds its end alone.
    int heap { };

    auto beforeEnd = checkpoint();
    WTFEndSignpost(&heap, JSCGarbageCollector, "%s", garbageCollectionMessage);
    auto afterEnd = checkpoint();

    auto marks = marksNamed("JSCGarbageCollector"_s);
    ASSERT_EQ(marks.size(), 1u);
    EXPECT_EQ(marks[0].begin, marks[0].end);
    EXPECT_TRUE(ranBetween(marks[0], beforeEnd, afterEnd, beforeEnd, afterEnd));
    EXPECT_EQ(marks[0].message, String::fromLatin1(garbageCollectionMessage));
}

TEST_F(WTF_SysprofAnnotator, TraceScopeNestedInOneOfItsName)
{
    // LocalFrameViewLayoutContext::performLayout() resolves style before laying out, and style resolution that interleaves
    // layout runs performLayout() again from within it, through interleavedLayout(). Each lays out the render tree in turn.
    std::optional<TraceScope> outerLayout;
    std::optional<TraceScope> innerLayout;
    std::optional<TraceScope> renderTreeLayout;

    auto t0 = checkpoint();
    outerLayout.emplace(PerformLayoutStart, PerformLayoutEnd);
    auto t1 = checkpoint();
    innerLayout.emplace(PerformLayoutStart, PerformLayoutEnd);
    auto t2 = checkpoint();
    renderTreeLayout.emplace(RenderTreeLayoutStart, RenderTreeLayoutEnd);
    auto t3 = checkpoint();
    renderTreeLayout.reset();
    auto t4 = checkpoint();
    innerLayout.reset();
    auto t5 = checkpoint();
    renderTreeLayout.emplace(RenderTreeLayoutStart, RenderTreeLayoutEnd);
    auto t6 = checkpoint();
    renderTreeLayout.reset();
    auto t7 = checkpoint();
    outerLayout.reset();
    auto t8 = checkpoint();

    auto layouts = marksNamed("PerformLayout"_s);
    ASSERT_EQ(layouts.size(), 2u);
    EXPECT_TRUE(ranBetween(layouts[0], t1, t2, t4, t5));
    EXPECT_TRUE(ranBetween(layouts[1], t0, t1, t7, t8));

    auto renderTreeLayouts = marksNamed("RenderTreeLayout"_s);
    ASSERT_EQ(renderTreeLayouts.size(), 2u);
    EXPECT_TRUE(ranBetween(renderTreeLayouts[0], t2, t3, t3, t4));
    EXPECT_TRUE(ranBetween(renderTreeLayouts[1], t5, t6, t6, t7));
}

TEST_F(WTF_SysprofAnnotator, TraceScopeOfATracePointPairedByItsData)
{
    // The data of a scope names what it traces at its end as well as its begin.
    static constexpr uint64_t load = 25;
    std::optional<TraceScope> scope;

    auto beforeBegin = checkpoint();
    scope.emplace(SubresourceLoadWillStart, SubresourceLoadDidEnd, load);
    auto beforeEnd = checkpoint();
    scope.reset();
    auto afterEnd = checkpoint();

    auto marks = marksNamed("SubresourceLoad"_s);
    ASSERT_EQ(marks.size(), 1u);
    EXPECT_TRUE(ranBetween(marks[0], beforeBegin, beforeEnd, beforeEnd, afterEnd));
}

TEST_F(WTF_SysprofAnnotator, TraceScopeCarryingData)
{
    // Releasing memory under pressure passes whether the pressure is critical and whether to release synchronously,
    // which only the begin of the scope carries.
    std::optional<TraceScope> scope;

    auto beforeBegin = checkpoint();
    scope.emplace(MemoryPressureHandlerStart, MemoryPressureHandlerEnd, static_cast<uint64_t>(true), static_cast<uint64_t>(false));
    auto beforeEnd = checkpoint();
    scope.reset();
    auto afterEnd = checkpoint();

    auto marks = marksNamed("MemoryPressureHandler"_s);
    ASSERT_EQ(marks.size(), 1u);
    EXPECT_TRUE(ranBetween(marks[0], beforeBegin, beforeEnd, beforeEnd, afterEnd));
}

TEST_F(WTF_SysprofAnnotator, SameTraceScopeOnTwoThreadsAtOnce)
{
    // Every ThreadedCompositor renders its layer tree on a thread of its own, so two web views render at once.
    std::optional<TraceScope> firstRendering;
    std::optional<TraceScope> secondRendering;
    auto firstCompositingThread = WorkQueue::create("First compositing thread"_s);
    auto secondCompositingThread = WorkQueue::create("Second compositing thread"_s);

    auto t0 = checkpoint();
    firstCompositingThread->dispatchSync([&] {
        firstRendering.emplace(RenderLayerTreeStart, RenderLayerTreeEnd);
    });
    auto t1 = checkpoint();
    secondCompositingThread->dispatchSync([&] {
        secondRendering.emplace(RenderLayerTreeStart, RenderLayerTreeEnd);
    });
    auto t2 = checkpoint();
    firstCompositingThread->dispatchSync([&] {
        firstRendering.reset();
    });
    auto t3 = checkpoint();
    secondCompositingThread->dispatchSync([&] {
        secondRendering.reset();
    });
    auto t4 = checkpoint();

    auto marks = marksNamed("RenderLayerTree"_s);
    ASSERT_EQ(marks.size(), 2u);
    EXPECT_TRUE(ranBetween(marks[0], t0, t1, t2, t3));
    EXPECT_TRUE(ranBetween(marks[1], t1, t2, t3, t4));
}

TEST_F(WTF_SysprofAnnotator, TraceScopeEndDoesNotTakeTheBeginOfAnotherThread)
{
    // Recording can start while one compositor renders its layer tree, so the end of that scope has no begin. Another
    // compositor rendering meanwhile keeps its own begin.
    std::optional<TraceScope> rendering;
    auto firstCompositingThread = WorkQueue::create("First compositing thread"_s);
    auto secondCompositingThread = WorkQueue::create("Second compositing thread"_s);

    auto t0 = checkpoint();
    firstCompositingThread->dispatchSync([&] {
        rendering.emplace(RenderLayerTreeStart, RenderLayerTreeEnd);
    });
    auto t1 = checkpoint();
    secondCompositingThread->dispatchSync([&] {
        tracePoint(RenderLayerTreeEnd);
    });
    auto t2 = checkpoint();
    firstCompositingThread->dispatchSync([&] {
        rendering.reset();
    });
    auto t3 = checkpoint();

    auto marks = marksNamed("RenderLayerTree"_s);
    ASSERT_EQ(marks.size(), 2u);
    EXPECT_EQ(marks[0].begin, marks[0].end);
    EXPECT_TRUE(ranBetween(marks[0], t1, t2, t1, t2));
    EXPECT_TRUE(ranBetween(marks[1], t0, t1, t2, t3));
}

TEST_F(WTF_SysprofAnnotator, TracePointsWithoutData)
{
    // Page begins and ends updating the rendering with trace points of their own.
    auto beforeBegin = checkpoint();
    tracePoint(RenderingUpdateStart);
    auto beforeEnd = checkpoint();
    tracePoint(RenderingUpdateEnd);
    auto afterEnd = checkpoint();

    auto marks = marksNamed("RenderingUpdate"_s);
    ASSERT_EQ(marks.size(), 1u);
    EXPECT_TRUE(ranBetween(marks[0], beforeBegin, beforeEnd, beforeEnd, afterEnd));
}

TEST_F(WTF_SysprofAnnotator, TracePointEndingOnAnotherThreadWithDataItsBeginLacks)
{
    // ThreadedScrollingTree begins waiting for the scrolling thread on the main thread without data, and the scrolling
    // thread ends it, with 1 where it gave up on the rendering update taking too long.
    auto scrollingThread = WorkQueue::create("Scrolling thread"_s);

    auto beforeBegin = checkpoint();
    tracePoint(ScrollingThreadRenderUpdateSyncStart);
    auto beforeEnd = checkpoint();
    scrollingThread->dispatchSync([] {
        tracePoint(ScrollingThreadRenderUpdateSyncEnd, 1);
    });
    auto afterEnd = checkpoint();

    auto marks = marksNamed("ScrollingThreadRenderUpdateSync"_s);
    ASSERT_EQ(marks.size(), 1u);
    EXPECT_TRUE(ranBetween(marks[0], beforeBegin, beforeEnd, beforeEnd, afterEnd));
}

TEST_F(WTF_SysprofAnnotator, OverlappingTracePointsPairByTheirData)
{
    // Subresources load at once, and SubresourceLoader passes the identifier of each load at its begin and its end.
    static constexpr uint64_t firstLoad = 21;
    static constexpr uint64_t secondLoad = 22;

    auto t0 = checkpoint();
    tracePoint(SubresourceLoadWillStart, firstLoad, pageIdentifier, frameIdentifier);
    auto t1 = checkpoint();
    tracePoint(SubresourceLoadWillStart, secondLoad, pageIdentifier, frameIdentifier);
    auto t2 = checkpoint();
    tracePoint(SubresourceLoadDidEnd, firstLoad);
    auto t3 = checkpoint();
    tracePoint(SubresourceLoadDidEnd, secondLoad);
    auto t4 = checkpoint();

    auto marks = marksNamed("SubresourceLoad"_s);
    ASSERT_EQ(marks.size(), 2u);
    EXPECT_TRUE(ranBetween(marks[0], t0, t1, t2, t3));
    EXPECT_TRUE(ranBetween(marks[1], t1, t2, t3, t4));
}

TEST_F(WTF_SysprofAnnotator, RedirectedLoadIsTimedFromItsFirstBegin)
{
    // SubresourceLoader begins the trace point again for every redirect of a load, which ends once.
    static constexpr uint64_t load = 23;

    auto t0 = checkpoint();
    tracePoint(SubresourceLoadWillStart, load, pageIdentifier, frameIdentifier);
    auto t1 = checkpoint();
    tracePoint(SubresourceLoadWillStart, load, pageIdentifier, frameIdentifier);
    auto t2 = checkpoint();
    tracePoint(SubresourceLoadDidEnd, load);
    auto t3 = checkpoint();

    auto marks = marksNamed("SubresourceLoad"_s);
    ASSERT_EQ(marks.size(), 1u);
    EXPECT_TRUE(ranBetween(marks[0], t0, t1, t2, t3));
}

TEST_F(WTF_SysprofAnnotator, ProvisionalLoadBegunAgainIsTimedFromItsLatestBegin)
{
    // FrameLoader begins the trace point for the page with every provisional load, and one that fails does not end it.
    tracePoint(MainResourceLoadDidStartProvisional, pageIdentifier);
    auto t1 = checkpoint();
    tracePoint(MainResourceLoadDidStartProvisional, pageIdentifier);
    auto t2 = checkpoint();
    tracePoint(MainResourceLoadDidEnd, pageIdentifier);
    auto t3 = checkpoint();

    auto marks = marksNamed("MainResourceLoad"_s);
    ASSERT_EQ(marks.size(), 1u);
    EXPECT_TRUE(ranBetween(marks[0], t1, t2, t2, t3));
}

TEST_F(WTF_SysprofAnnotator, TracePointEndWithoutBegin)
{
    // Recording can start while a subresource loads, so the capture holds the end of the load alone.
    static constexpr uint64_t load = 24;

    auto beforeEnd = checkpoint();
    tracePoint(SubresourceLoadDidEnd, load);
    auto afterEnd = checkpoint();

    auto marks = marksNamed("SubresourceLoad"_s);
    ASSERT_EQ(marks.size(), 1u);
    EXPECT_EQ(marks[0].begin, marks[0].end);
    EXPECT_TRUE(ranBetween(marks[0], beforeEnd, afterEnd, beforeEnd, afterEnd));
}

TEST_F(WTF_SysprofAnnotator, TracePointCarryingDataEndingOnAnotherThread)
{
    // A flush fence of RemoteImageBufferSetProxy begins the trace point for itself where the flush is prepared, and ends
    // it once the flusher waited for the flush on its own thread. RemoteImageBufferProxy meanwhile flushes its drawing
    // context within a scope of the same trace point.
    int fence { };
    std::optional<TraceScope> flushingDrawingContext;
    auto flusherThread = WorkQueue::create("Flusher thread"_s);

    auto t0 = checkpoint();
    tracePoint(FlushRemoteImageBufferStart, reinterpret_cast<uintptr_t>(&fence));
    auto t1 = checkpoint();
    flushingDrawingContext.emplace(FlushRemoteImageBufferStart, FlushRemoteImageBufferEnd);
    auto t2 = checkpoint();
    flusherThread->dispatchSync([&] {
        tracePoint(FlushRemoteImageBufferEnd, reinterpret_cast<uintptr_t>(&fence), 1u);
    });
    auto t3 = checkpoint();
    flushingDrawingContext.reset();
    auto t4 = checkpoint();

    auto marks = marksNamed("FlushRemoteImageBuffer"_s);
    ASSERT_EQ(marks.size(), 2u);
    EXPECT_TRUE(ranBetween(marks[0], t0, t1, t2, t3));
    EXPECT_TRUE(ranBetween(marks[1], t1, t2, t3, t4));
}

TEST_F(WTF_SysprofAnnotator, OverlappingProcessLaunches)
{
    // The UI process launches a web process and a network process at once. ProcessLauncher passes the identifier
    // of each process, and once launched, its type and its process ID as well.
    static constexpr uint64_t webProcess = 31;
    static constexpr uint64_t networkProcess = 32;
    static constexpr uint64_t webProcessType = 0;
    static constexpr uint64_t networkProcessType = 1;

    auto t0 = checkpoint();
    tracePoint(ProcessLaunchStart, webProcess);
    auto t1 = checkpoint();
    tracePoint(ProcessLaunchStart, networkProcess);
    auto t2 = checkpoint();
    tracePoint(ProcessLaunchEnd, networkProcess, networkProcessType, 4242);
    auto t3 = checkpoint();
    tracePoint(ProcessLaunchEnd, webProcess, webProcessType, 4243);
    auto t4 = checkpoint();

    auto marks = marksNamed("ProcessLaunch"_s);
    ASSERT_EQ(marks.size(), 2u);
    EXPECT_TRUE(ranBetween(marks[0], t1, t2, t2, t3));
    EXPECT_TRUE(ranBetween(marks[1], t0, t1, t3, t4));
}

TEST_F(WTF_SysprofAnnotator, OverlappingTracePointsFromJavaScript)
{
    // A page that profilers are exposed to starts and stops trace points of identifiers it picks, in any order.
    static constexpr uint64_t firstInterval = 41;
    static constexpr uint64_t secondInterval = 42;

    auto t0 = checkpoint();
    tracePoint(FromJSStart, firstInterval);
    auto t1 = checkpoint();
    tracePoint(FromJSStart, secondInterval);
    auto t2 = checkpoint();
    tracePoint(FromJSStop, firstInterval);
    auto t3 = checkpoint();
    tracePoint(FromJSStop, secondInterval);
    auto t4 = checkpoint();

    auto marks = marksNamed("FromJS"_s);
    ASSERT_EQ(marks.size(), 2u);
    EXPECT_TRUE(ranBetween(marks[0], t0, t1, t2, t3));
    EXPECT_TRUE(ranBetween(marks[1], t1, t2, t3, t4));
}

TEST_F(WTF_SysprofAnnotator, BeginsWithoutEndAreBounded)
{
    // SubresourceLoader begins the trace point and returns if the load reached its terminal state meanwhile, so the begin
    // never ends. A load without an identifier passes no data, so its begins pile up with the others of its thread,
    // and the annotator keeps only the latest 64 of them.
    static constexpr unsigned begins = 70;
    static constexpr unsigned keptBegins = 64;

    for (unsigned i = 0; i < begins; ++i)
        tracePoint(SubresourceLoadWillStart, 0, pageIdentifier, frameIdentifier);
    // Every end then comes after every begin, even where the clock is too coarse to tell two of them apart.
    checkpoint();
    for (unsigned i = 0; i < begins; ++i)
        tracePoint(SubresourceLoadDidEnd, 0);

    auto marks = marksNamed("SubresourceLoad"_s);
    ASSERT_EQ(marks.size(), begins);
    for (unsigned i = 0; i < begins; ++i)
        EXPECT_EQ(marks[i].begin < marks[i].end, i < keptBegins) << "end " << i;
}

TEST_F(WTF_SysprofAnnotator, InstantMarks)
{
    // ThreadedCompositor emits DidRenderFrame with the reasons it rendered for, and DisplayLink emits DisplayLinkUpdate
    // for every refresh.
    auto t0 = checkpoint();
    WTFEmitSignpost(this, DidRenderFrame, "reasons: %s", "Animation");
    auto t1 = checkpoint();
    tracePoint(DisplayLinkUpdate);
    auto t2 = checkpoint();

    auto frames = marksNamed("DidRenderFrame"_s);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].begin, frames[0].end);
    EXPECT_TRUE(ranBetween(frames[0], t0, t1, t0, t1));
    EXPECT_EQ(frames[0].message, "reasons: Animation"_s);

    auto refreshes = marksNamed("DisplayLinkUpdate"_s);
    ASSERT_EQ(refreshes.size(), 1u);
    EXPECT_EQ(refreshes[0].begin, refreshes[0].end);
    EXPECT_TRUE(ranBetween(refreshes[0], t1, t2, t1, t2));
}

} // namespace TestWebKitAPI

#endif // USE(SYSPROF_CAPTURE)
