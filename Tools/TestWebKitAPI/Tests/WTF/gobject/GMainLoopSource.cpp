/*
 * Copyright (C) 2014 Igalia S.L.
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

#include <wtf/gobject/GMainLoopSource.h>
#include <stdio.h>

namespace TestWebKitAPI {

class GMainLoopSourceTest {
public:
    GMainLoopSourceTest()
        : m_mainLoop(g_main_loop_new(nullptr, TRUE))
    {
    }

    ~GMainLoopSourceTest()
    {
        g_main_loop_unref(m_mainLoop);
    }

    void runLoop()
    {
        g_main_loop_run(m_mainLoop);
    }

    void finish()
    {
        g_main_loop_quit(m_mainLoop);
    }

    GMainLoopSource& source() { return m_source; }

private:
    GMainLoop* m_mainLoop;
    GMainLoopSource m_source;
};

TEST(WTF_GMainLoopSource, BasicRescheduling)
{
    struct TestingContext {
        GMainLoopSourceTest test;
        bool finishedFirstTask = false;
        bool finishedSecondTask = false;
    } context;

    context.test.source().schedule("[Test] FirstTask", [&] {
        // This should never be called. That's why we assert
        // that the variable is false a few lines later.
        context.finishedFirstTask = true;
    });

    context.test.source().schedule("[Test] SecondTask", [&] {
        context.finishedSecondTask = true;
        context.test.finish();
    });

    context.test.runLoop();
    EXPECT_FALSE(context.finishedFirstTask);
    EXPECT_TRUE(context.finishedSecondTask);
}

TEST(WTF_GMainLoopSource, ReentrantRescheduling)
{
    struct TestingContext {
        GMainLoopSourceTest test;
        bool finishedFirstTask = false;
        bool finishedSecondTask = false;
    } context;

    context.test.source().schedule("[Test] FirstTask", [&] {
        context.test.source().schedule("[Test] SecondTask", [&] {
            ASSERT(context.finishedFirstTask);
            context.finishedSecondTask = true;
            context.test.finish();
        });

        context.finishedFirstTask = true;
    });

    context.test.runLoop();
    EXPECT_TRUE(context.finishedFirstTask);
    EXPECT_TRUE(context.finishedSecondTask);
}

TEST(WTF_GMainLoopSource, ReschedulingFromDifferentThread)
{
    struct TestingContext {
        GMainLoopSourceTest test;
        bool finishedFirstTask;
        bool finishedSecondTask;
    } context;

    context.test.source().schedule("[Test] FirstTask", [&] {
        g_usleep(1 * G_USEC_PER_SEC);
        context.finishedFirstTask = true;
    });

    g_thread_new(nullptr, [](gpointer data) -> gpointer {
        g_usleep(0.25 * G_USEC_PER_SEC);

        TestingContext& context = *static_cast<TestingContext*>(data);
        EXPECT_FALSE(context.finishedFirstTask);

        context.test.source().schedule("[Test] SecondTask", [&] {
            EXPECT_TRUE(context.finishedFirstTask);
            context.finishedSecondTask = true;
            context.test.finish();
        });

        g_thread_exit(nullptr);
        return nullptr;
    }, &context);

    context.test.runLoop();
    EXPECT_TRUE(context.finishedFirstTask);
    EXPECT_TRUE(context.finishedSecondTask);
}

} // namespace TestWebKitAPI
