/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "Utilities.h"

#include <glib.h>
#include <wtf/RunLoop.h>

namespace TestWebKitAPI {
namespace Util {

void run(bool* done)
{
    g_idle_add([](gpointer userData) -> gboolean {
        bool* done = static_cast<bool*>(userData);
        if (*done)
            RunLoop::current().stop();

        return !*done;
    }, done);
    RunLoop::run();
}

void spinRunLoop(uint64_t count)
{
    g_idle_add([](gpointer userData) -> gboolean {
        uint64_t* count = static_cast<uint64_t*>(userData);
        return --(*count) ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
    }, &count);
}

void sleep(double seconds)
{
    g_timeout_add(seconds * 1000, [](gpointer userData) -> gboolean {
        RunLoop::main().stop();
        return G_SOURCE_REMOVE;
    }, nullptr);
    RunLoop::current().stop();
}

} // namespace Util
} // namespace TestWebKitAPI
