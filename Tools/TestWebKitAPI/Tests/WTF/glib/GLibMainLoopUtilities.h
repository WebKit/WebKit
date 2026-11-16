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

#pragma once

#include <glib.h>

namespace TestWebKitAPI {

// Spin the default GLib main context until either `predicate` returns true or
// `timeoutSeconds` elapses. Returns whatever `predicate` reports on exit.
template<typename Predicate>
bool runMainLoopUntil(Predicate&& predicate, unsigned timeoutSeconds = 5)
{
    GMainContext* context = g_main_context_default();
    GMainLoop* loop = g_main_loop_new(context, false);

    struct Quit {
        GMainLoop* loop;
        bool* timedOut;
    };

    bool timedOut = false;
    Quit quit { loop, &timedOut };

    guint timeoutId = g_timeout_add_seconds(timeoutSeconds, [](gpointer userData) -> gboolean {
        auto* q = static_cast<Quit*>(userData);
        *q->timedOut = true;
        g_main_loop_quit(q->loop);
        return G_SOURCE_REMOVE;
    }, &quit);

    while (!predicate() && !timedOut)
        g_main_context_iteration(context, true);

    // Only remove the timeout if it hasn't already fired; otherwise GLib logs a
    // "source ID not found" critical because the callback already returned
    // G_SOURCE_REMOVE.
    if (!timedOut)
        g_source_remove(timeoutId);
    g_main_loop_unref(loop);
    return predicate();
}

} // namespace TestWebKitAPI
