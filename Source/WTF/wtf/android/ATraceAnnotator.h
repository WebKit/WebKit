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

#pragma once

#include <wtf/Platform.h>

#if USE(ATRACE_CAPTURE)

#include <cstdint>
#include <type_traits>
#include <wtf/ExportMacros.h>

namespace WTF {

// Bridges WebKit's tracepoints, signposts and counters onto Android's ATrace. Everything lands in
// the same systrace/Perfetto timeline as SurfaceFlinger, input dispatch and the scheduler, which
// is what makes it possible to line up a janky frame with whatever the web process was doing at
// the time.
//
// Tracing is off until a capture session enables the app tag for the process (`atrace -a` or
// Perfetto's atrace_apps), so the cost in a normal run is one relaxed atomic load per call site.
class ATraceAnnotator {
public:
    WTF_EXPORT_PRIVATE static bool isEnabled();

    // Called from WTF::tracePoint(). The code is a TracePointCode, passed as unsigned because that
    // enum cannot be forward declared.
    WTF_EXPORT_PRIVATE static void tracePoint(unsigned code, uint64_t data1);

    // Process-scoped slices, for spans that begin and end on different threads.
    WTF_EXPORT_PRIVATE static void beginAsync(const char* name, int32_t cookie);
    WTF_EXPORT_PRIVATE static void endAsync(const char* name, int32_t cookie);

    // Zero-length marker whose name carries the description.
    WTF_EXPORT_PRIVATE static void instant(const char* format, ...) WTF_ATTRIBUTE_PRINTF(1, 2);

    WTF_EXPORT_PRIVATE static void counter(const char* name, int64_t);

    // Signposts are keyed by a pointer and may begin and end on different threads, so they map to
    // process-scoped sections with a cookie derived from that pointer. Call sites key a signpost
    // on either an address or an opaque integer identifier, so both have to fold down to a
    // cookie. The XOR keeps small identifiers distinct, which a plain shift would collapse onto
    // zero.
    static int32_t cookieForValue(uintptr_t value)
    {
        return static_cast<int32_t>(((value >> 4) ^ value) & 0x7fffffff);
    }

    static int32_t cookieForPointer(const void* pointer)
    {
        return cookieForValue(reinterpret_cast<uintptr_t>(pointer));
    }

    template<typename T> requires std::is_integral_v<T>
    static int32_t cookieForPointer(T value)
    {
        return cookieForValue(static_cast<uintptr_t>(value));
    }
};

// Lets a macro reference its arguments without evaluating them, so a disabled signpost costs
// nothing and still does not provoke unused-variable warnings.
template<typename... Args> int aTraceIgnore(Args&&...);

} // namespace WTF

#endif // USE(ATRACE_CAPTURE)
