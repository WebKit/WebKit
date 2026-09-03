/*
 * Copyright (C) 2026 Hayden Barnes
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if PLATFORM(GTK)

#include "Helpers/Test.h"

#include <UIProcess/API/gtk/DropTargetState.h>

namespace TestWebKitAPI {

using WebKit::DropTargetState;

// A drop after the data loaded runs straight away.
TEST(DropTargetState, SynchronousDropIsNotDeferred)
{
    DropTargetState state;
    state.didAccept(true);
    state.didFinishLoadingData();

    EXPECT_FALSE(state.isWaitingForData());
    EXPECT_FALSE(state.didRequestDrop());
    EXPECT_FALSE(state.takeDeferredDrop());

    state.didFinishDrop();
    EXPECT_FALSE(state.takeUnfinishedDrop());
}

// A drop during the mime loads waits and is handed back once.
TEST(DropTargetState, DropDuringLoadIsDeferredAndReplayedOnce)
{
    DropTargetState state;
    state.didAccept(true);

    EXPECT_TRUE(state.isWaitingForData());
    EXPECT_TRUE(state.didRequestDrop());

    state.didFinishLoadingData();
    EXPECT_TRUE(state.takeDeferredDrop());
    EXPECT_FALSE(state.takeDeferredDrop());

    state.didFinishDrop();
    EXPECT_FALSE(state.takeUnfinishedDrop());
}

// Nothing to load means nothing to defer.
TEST(DropTargetState, DropIsNotDeferredWhenNothingIsLoading)
{
    DropTargetState state;
    state.didAccept(false);

    EXPECT_FALSE(state.isWaitingForData());
    EXPECT_FALSE(state.didRequestDrop());
    EXPECT_FALSE(state.takeDeferredDrop());
}

// drag-leave cancels the reads. The GdkDrop still has to be finished.
TEST(DropTargetState, LeaveAfterDeferredDropStillOwesFinish)
{
    DropTargetState state;
    state.didAccept(true);
    EXPECT_TRUE(state.didRequestDrop());

    EXPECT_TRUE(state.takeUnfinishedDrop());
    // Only once: leave() must not finish the same GdkDrop twice.
    EXPECT_FALSE(state.takeUnfinishedDrop());
    EXPECT_FALSE(state.takeDeferredDrop());
    EXPECT_FALSE(state.isWaitingForData());
}

// A drag-leave without any drop owes nothing; the drag source keeps the drop.
TEST(DropTargetState, LeaveWithoutDropOwesNothing)
{
    DropTargetState state;
    state.didAccept(true);

    EXPECT_FALSE(state.takeUnfinishedDrop());
}

// A finished drop is not finished again.
TEST(DropTargetState, FinishedDropIsNotFinishedAgain)
{
    DropTargetState state;
    state.didAccept(true);
    state.didFinishLoadingData();
    state.didRequestDrop();
    state.didFinishDrop();

    EXPECT_FALSE(state.takeUnfinishedDrop());
}

// Destruction while a deferred drop is outstanding still owes a finish.
TEST(DropTargetState, DestroyWithDeferredDropOwesFinish)
{
    DropTargetState state;
    state.didAccept(true);
    state.didRequestDrop();

    EXPECT_TRUE(state.takeUnfinishedDrop());
}

// A new drop starts clean.
TEST(DropTargetState, AcceptResetsPreviousState)
{
    DropTargetState state;
    state.didAccept(true);
    state.didRequestDrop();
    EXPECT_TRUE(state.takeUnfinishedDrop());

    state.didAccept(true);
    EXPECT_TRUE(state.isWaitingForData());
    EXPECT_FALSE(state.takeDeferredDrop());
    EXPECT_FALSE(state.takeUnfinishedDrop());
}

// Data finishing loading on its own does not invent a drop.
TEST(DropTargetState, LoadingCompletionWithoutDropDoesNotDefer)
{
    DropTargetState state;
    state.didAccept(true);
    state.didFinishLoadingData();

    EXPECT_FALSE(state.takeDeferredDrop());
    EXPECT_FALSE(state.takeUnfinishedDrop());
}

} // namespace TestWebKitAPI

#endif // PLATFORM(GTK)
