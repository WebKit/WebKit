/*
 * Copyright (C) 2017 Igalia, S.L.
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

#if USE(GLIB)

namespace WTF {

#if PLATFORM(GTK)

// This is a global enum to define priorities used by GLib run loop sources.
// In GLib, priorities are represented by an integer where lower values mean
// higher priority. The following macros are defined in GLib:
// G_PRIORITY_LOW = 300
// G_PRIORITY_DEFAULT_IDLE = 200
// G_PRIORITY_HIGH_IDLE = 100
// G_PRIORITY_DEFAULT = 0
// G_PRIORITY_HIGH = -100
// We don't use those macros here to avoid having to include glib header only
// for this. But we should take into account that GLib uses G_PRIORITY_DEFAULT
// for timeout sources and G_PRIORITY_DEFAULT_IDLE for idle sources.
// Changes in these priorities can have a huge impact in performance, and in
// the correctness too, so be careful when changing them.
enum RunLoopSourcePriority {
    // RunLoop::dispatch().
    RunLoopDispatcher = 100,

    // RunLoopTimer priority by default. It can be changed with RunLoopTimer::setPriority().
    RunLoopTimer = 0,

    // Garbage collector timers.
    JavascriptTimer = 200,

    // Memory pressure monitor.
    MemoryPressureHandlerTimer = -100,

    // WebCore timers.
    MainThreadSharedTimer = 100,

    // Used for timers that discard resources like backing store, buffers, etc.
    ReleaseUnusedResourcesTimer = 200,

    // Rendering timer in the threaded compositor.
    CompositingThreadUpdateTimer = 110,

    // Layer flush.
    LayerFlushTimer = 110,

    // DisplayRefreshMonitor timer, should have the same value as the LayerFlushTimer.
    DisplayRefreshMonitorTimer = 110,

    // Rendering timer in the main thread when accelerated compositing is not used.
    NonAcceleratedDrawingTimer = 100,

    // Async IO network callbacks.
    AsyncIONetwork = 100,
};

#else

enum RunLoopSourcePriority {
    RunLoopDispatcher = 0,
    RunLoopTimer = 0,

    MemoryPressureHandlerTimer = -10,

    JavascriptTimer = 10,
    MainThreadSharedTimer = 10,

    LayerFlushTimer = 0,
    DisplayRefreshMonitorTimer = 0,

    CompositingThreadUpdateTimer = 0,

    ReleaseUnusedResourcesTimer = 0,

    AsyncIONetwork = 10,
};

#endif

} // namespace WTF

using WTF::RunLoopSourcePriority;

#endif // USE(GLIB)
