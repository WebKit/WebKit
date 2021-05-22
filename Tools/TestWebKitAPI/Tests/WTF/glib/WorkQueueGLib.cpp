/*
 * Copyright (C) 2015 Igalia S.L.
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

#include "Test.h"
#include <gio/gio.h>
#include <thread>
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/WorkQueue.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

namespace TestWebKitAPI {

TEST(WTF_WorkQueue, AsyncIO)
{
    struct TestingContext {
        Lock m_lock;
        Condition m_testCompleted;
        GMainContext* m_mainContext;
    } context;

    auto queue = WorkQueue::create("com.apple.WebKit.Test.AsyncIO");
    context.m_mainContext = g_main_context_default();
    EXPECT_FALSE(g_main_context_get_thread_default());

    GUniquePtr<char> currentDirectory(g_get_current_dir());
    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(currentDirectory.get()));

    LockHolder locker(context.m_lock);
    queue->dispatch([&](void) {
        EXPECT_TRUE(g_main_context_get_thread_default());
        EXPECT_TRUE(g_main_context_get_thread_default() != context.m_mainContext);
        context.m_mainContext = g_main_context_get_thread_default();
        g_file_query_info_async(file.get(), G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT, nullptr,
            [](GObject*, GAsyncResult*, gpointer userData) {
                TestingContext* context = static_cast<TestingContext*>(userData);
                LockHolder locker(context->m_lock);
                EXPECT_EQ(g_main_context_get_thread_default(), context->m_mainContext);
                context->m_testCompleted.notifyOne();
            }, &context);
    });

    context.m_testCompleted.wait(context.m_lock);
}

} // namespace TestWebKitAPI
