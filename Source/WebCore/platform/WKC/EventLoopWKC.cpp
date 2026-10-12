/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (c) 2010, 2012 ACCESS CO., LTD. All rights reserved.
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

#include <wtf/RunLoop.h>

namespace WebCore {

typedef bool (*CycleProc)(void*);
static CycleProc gCycleProc = nullptr;
static void* gCycleOpaque = nullptr;

void EventLoop_setCycleProc(bool(*proc)(void*), void* opaque)
{
    gCycleProc = proc;
    gCycleOpaque = opaque;
}

void wkcRunLoopCycle()
{
    // Modern networking callbacks are dispatched onto WTF's main RunLoop.
    // The legacy WKC embedder drives this cycle explicitly, so drain one
    // RunLoop iteration here before handing control to the platform cycle
    // callback. Without this, asynchronous cURL responses can arrive on the
    // worker thread but never reach WebCore's loader on the main thread.
    WTF::RunLoop::cycle();

    if (gCycleProc)
        gCycleProc(gCycleOpaque);
}

} // namespace WebCore
