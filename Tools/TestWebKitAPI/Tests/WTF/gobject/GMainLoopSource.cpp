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

#include <wtf/gobject/GThreadSafeMainLoopSource.h>
#include <stdio.h>

namespace TestWebKitAPI {

template <typename T>
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

    void delayedFinish()
    {
        g_timeout_add(250,
            [](gpointer data) {
                GMainLoopSourceTest& test = *static_cast<GMainLoopSourceTest*>(data);
                test.finish();
                return G_SOURCE_REMOVE;
            }, this);
    }

    void finish()
    {
        g_main_loop_quit(m_mainLoop);
    }

    T& source() { return m_source; }

private:
    GMainLoop* m_mainLoop;
    T m_source;
};

template <typename T>
static void basicRescheduling(T& context)
{
    EXPECT_TRUE(!context.test.source().isActive());

    context.test.source().schedule("[Test] FirstTask", [&] {
        // This should never be called. That's why we assert
        // that the variable is false a few lines later.
        context.finishedFirstTask = true;
    });
    EXPECT_TRUE(context.test.source().isScheduled());

    context.test.source().schedule("[Test] SecondTask", [&] {
        EXPECT_TRUE(context.test.source().isActive() && !context.test.source().isScheduled());
        context.finishedSecondTask = true;
        context.test.finish();
    });
    EXPECT_TRUE(context.test.source().isScheduled());

    context.test.runLoop();

    EXPECT_TRUE(!context.test.source().isActive());
    EXPECT_FALSE(context.finishedFirstTask);
    EXPECT_TRUE(context.finishedSecondTask);
}

TEST(WTF_GMainLoopSource, BasicRescheduling)
{
    struct TestingContext {
        GMainLoopSourceTest<GMainLoopSource> test;
        bool finishedFirstTask = false;
        bool finishedSecondTask = false;
    } context;
    basicRescheduling<TestingContext>(context);

    struct ThreadSafeTestingContext {
        GMainLoopSourceTest<GThreadSafeMainLoopSource> test;
        bool finishedFirstTask = false;
        bool finishedSecondTask = false;
    } threadSafeContext;
    basicRescheduling<ThreadSafeTestingContext>(threadSafeContext);
}

template <typename T>
static void reentrantRescheduling(T& context)
{
    EXPECT_TRUE(!context.test.source().isActive());

    context.test.source().schedule("[Test] FirstTask", [&] {
        EXPECT_TRUE(context.test.source().isActive() && !context.test.source().isScheduled());

        context.test.source().schedule("[Test] SecondTask", [&] {
            EXPECT_TRUE(context.test.source().isActive() && !context.test.source().isScheduled());
            EXPECT_TRUE(context.finishedFirstTask);

            context.finishedSecondTask = true;
            context.test.finish();
        });
        EXPECT_TRUE(context.test.source().isScheduled());

        context.finishedFirstTask = true;
    });
    EXPECT_TRUE(context.test.source().isScheduled());

    context.test.runLoop();

    EXPECT_TRUE(!context.test.source().isActive());
    EXPECT_TRUE(context.finishedFirstTask);
    EXPECT_TRUE(context.finishedSecondTask);
}

TEST(WTF_GMainLoopSource, ReentrantRescheduling)
{
    struct TestingContext {
        GMainLoopSourceTest<GMainLoopSource> test;
        bool finishedFirstTask = false;
        bool finishedSecondTask = false;
    } context;
    reentrantRescheduling<TestingContext>(context);

    struct ThreadSafeTestingContext {
        GMainLoopSourceTest<GThreadSafeMainLoopSource> test;
        bool finishedFirstTask = false;
        bool finishedSecondTask = false;
    } threadSafeContext;
    reentrantRescheduling<ThreadSafeTestingContext>(threadSafeContext);
}

TEST(WTF_GMainLoopSource, ReschedulingFromDifferentThread)
{
    struct TestingContext {
        GMainLoopSourceTest<GThreadSafeMainLoopSource> test;
        bool finishedFirstTask;
        bool finishedSecondTask;
    } context;

    EXPECT_TRUE(!context.test.source().isActive());

    context.test.source().schedule("[Test] FirstTask", [&] {
        EXPECT_TRUE(context.test.source().isActive() && !context.test.source().isScheduled());

        g_usleep(1 * G_USEC_PER_SEC);
        context.finishedFirstTask = true;
    });
    EXPECT_TRUE(context.test.source().isScheduled());

    GThread* helperThread = g_thread_new(nullptr, [](gpointer data) -> gpointer {
        g_usleep(0.25 * G_USEC_PER_SEC);

        TestingContext& context = *static_cast<TestingContext*>(data);
        EXPECT_TRUE(context.test.source().isActive() && !context.test.source().isScheduled());
        EXPECT_FALSE(context.finishedFirstTask);

        context.test.source().schedule("[Test] SecondTask", [&] {
            EXPECT_TRUE(context.finishedFirstTask);

            context.finishedSecondTask = true;
            context.test.finish();
        });
        EXPECT_TRUE(context.test.source().isScheduled());

        g_thread_exit(nullptr);
        return nullptr;
    }, &context);

    context.test.runLoop();
    g_thread_unref(helperThread);

    EXPECT_TRUE(!context.test.source().isActive());
    EXPECT_TRUE(context.finishedFirstTask);
    EXPECT_TRUE(context.finishedSecondTask);
}

TEST(WTF_GMainLoopSource, DestructionDuringDispatch)
{
    // This is just a raw test that ensures deleting the GMainLoopSource object during
    // dispatch does not cause problems. This test succeeds if it doesn't crash.

    GMainLoopSource* source;
    GMainLoop* loop = g_main_loop_new(nullptr, TRUE);

    source = new GMainLoopSource;
    source->schedule("[Test] DestroySourceTask", [&] {
        delete source;
        g_main_loop_quit(loop);
    });
    g_main_loop_run(loop);

    source = new GMainLoopSource;
    source->schedule("[Test] DestroySourceTask", std::function<bool ()>([&] {
        delete source;
        g_main_loop_quit(loop);
        return false;
    }));
    g_main_loop_run(loop);

    g_main_loop_unref(loop);
}

template <typename T>
static void cancelRepeatingSourceDuringDispatch(T& context)
{
    EXPECT_TRUE(!context.test.source().isActive());

    context.test.source().schedule("[Test] RepeatingTask",
        std::function<bool ()>([&] {
            EXPECT_TRUE(context.test.source().isActive() && !context.test.source().isScheduled());

            context.callCount++;
            if (context.callCount == 3)
                context.test.source().cancel();
            return true;
        }));
    EXPECT_TRUE(context.test.source().isScheduled());

    context.test.delayedFinish();
    context.test.runLoop();

    EXPECT_TRUE(!context.test.source().isActive());
    EXPECT_EQ(3, context.callCount);
}

TEST(WTF_GMainLoopSource, CancelRepeatingSourceDuringDispatch)
{
    struct TestingContext {
        GMainLoopSourceTest<GMainLoopSource> test;
        unsigned callCount = 0;
    } context;
    cancelRepeatingSourceDuringDispatch<TestingContext>(context);

    struct ThreadSafeTestingContext {
        GMainLoopSourceTest<GThreadSafeMainLoopSource> test;
        unsigned callCount = 0;
    } threadSafeContext;
    cancelRepeatingSourceDuringDispatch<ThreadSafeTestingContext>(threadSafeContext);
}

template <typename T>
static void basicDestroyCallbacks()
{
    {
        T context;
        EXPECT_TRUE(!context.test.source().isActive());
        context.test.source().schedule("[Test] DestroyCallback",
            [&] {
                EXPECT_TRUE(context.test.source().isActive() && !context.test.source().isScheduled());
                context.callbackCalled = true;
            }, G_PRIORITY_DEFAULT,
            [&] {
                EXPECT_TRUE(!context.test.source().isActive());
                context.destroyCallbackCalled = true;
                context.test.finish();
            });
        EXPECT_TRUE(context.test.source().isScheduled());

        context.test.runLoop();

        EXPECT_TRUE(!context.test.source().isActive());
        EXPECT_TRUE(context.callbackCalled);
        EXPECT_TRUE(context.destroyCallbackCalled);
    }

    {
        T context;
        EXPECT_TRUE(!context.test.source().isActive());
        context.test.source().schedule("[Test] DestroyCallback",
            std::function<bool ()>([&] {
                EXPECT_TRUE(context.test.source().isActive() && !context.test.source().isScheduled());
                context.callbackCalled = true;
                return false;
            }), G_PRIORITY_DEFAULT,
            [&] {
                EXPECT_TRUE(!context.test.source().isActive());
                context.destroyCallbackCalled = true;
                context.test.finish();
            });
        EXPECT_TRUE(context.test.source().isScheduled());

        context.test.runLoop();

        EXPECT_TRUE(!context.test.source().isActive());
        EXPECT_TRUE(context.callbackCalled);
        EXPECT_TRUE(context.destroyCallbackCalled);
    }
}

TEST(WTF_GMainLoopSource, BasicDestroyCallbacks)
{
    struct TestingContext {
        GMainLoopSourceTest<GMainLoopSource> test;
        bool callbackCalled = false;
        bool destroyCallbackCalled = false;
    };
    basicDestroyCallbacks<TestingContext>();

    struct ThreadSafeTestingContext {
        GMainLoopSourceTest<GThreadSafeMainLoopSource> test;
        bool callbackCalled = false;
        bool destroyCallbackCalled = false;
    };
    basicDestroyCallbacks<ThreadSafeTestingContext>();
}

template <typename T>
static void destroyCallbacksAfterCancellingDuringDispatch()
{
    {
        T context;
        EXPECT_TRUE(!context.test.source().isActive());
        context.test.source().schedule("[Test] DestroyCallback",
            [&] {
                EXPECT_TRUE(context.test.source().isActive() && !context.test.source().isScheduled());
                context.callbackCallCount++;
                context.test.source().cancel();
            }, G_PRIORITY_DEFAULT,
            [&] {
                EXPECT_TRUE(!context.test.source().isActive());
                context.destroyCallbackCalled = true;
                context.test.finish();
            });
        EXPECT_TRUE(context.test.source().isScheduled());

        context.test.delayedFinish();
        context.test.runLoop();

        EXPECT_TRUE(!context.test.source().isActive());
        EXPECT_EQ(1, context.callbackCallCount);
        EXPECT_TRUE(context.destroyCallbackCalled);
    }

    {
        T context;
        EXPECT_TRUE(!context.test.source().isActive());
        context.test.source().schedule("[Test] DestroyCallback",
            std::function<bool ()>([&] {
                EXPECT_TRUE(context.test.source().isActive() && !context.test.source().isScheduled());
                context.callbackCallCount++;
                if (context.callbackCallCount == 3)
                    context.test.source().cancel();
                return true;
            }), G_PRIORITY_DEFAULT,
            [&] {
                EXPECT_TRUE(!context.test.source().isActive());
                context.destroyCallbackCalled = true;
            });
        EXPECT_TRUE(context.test.source().isScheduled());

        context.test.delayedFinish();
        context.test.runLoop();

        EXPECT_TRUE(!context.test.source().isActive());
        EXPECT_EQ(3, context.callbackCallCount);
        EXPECT_TRUE(context.destroyCallbackCalled);
    }
}

TEST(WTF_GMainLoopSource, DestroyCallbacksAfterCancellingDuringDispatch)
{
    struct TestingContext {
        GMainLoopSourceTest<GMainLoopSource> test;
        unsigned callbackCallCount= 0;
        bool destroyCallbackCalled = false;
    };
    destroyCallbacksAfterCancellingDuringDispatch<TestingContext>();

    struct ThreadSafeTestingContext {
        GMainLoopSourceTest<GThreadSafeMainLoopSource> test;
        unsigned callbackCallCount= 0;
        bool destroyCallbackCalled = false;
    };
    destroyCallbacksAfterCancellingDuringDispatch<ThreadSafeTestingContext>();
}

template <typename T>
static void destroyCallbacksAfterReschedulingDuringDispatch()
{
    {
        T context;
        EXPECT_TRUE(!context.test.source().isActive());
        context.test.source().schedule("[Test] BaseCallback",
            [&] {
                EXPECT_TRUE(context.test.source().isActive() && !context.test.source().isScheduled());
                context.firstCallbackCallCount++;
                context.test.source().schedule("[Test] ReschedulingCallback",
                    [&] {
                        EXPECT_TRUE(context.test.source().isActive() && !context.test.source().isScheduled());
                        context.secondCallbackCallCount++;
                    }, G_PRIORITY_DEFAULT,
                    [&] {
                        EXPECT_TRUE(!context.test.source().isActive());
                        context.secondDestroyCallbackCalled = true;
                    });
                EXPECT_TRUE(context.test.source().isScheduled());
            }, G_PRIORITY_DEFAULT,
            [&] {
                // At this point the GMainLoopSource has been rescheduled, ergo the Scheduled status.
                EXPECT_TRUE(context.test.source().isScheduled());
                context.firstDestroyCallbackCalled = true;
            });
        EXPECT_TRUE(context.test.source().isScheduled());

        context.test.delayedFinish();
        context.test.runLoop();

        EXPECT_TRUE(!context.test.source().isActive());
        EXPECT_EQ(1, context.firstCallbackCallCount);
        EXPECT_TRUE(context.firstDestroyCallbackCalled);
        EXPECT_EQ(1, context.secondCallbackCallCount);
        EXPECT_TRUE(context.secondDestroyCallbackCalled);
    }

    {
        T context;
        EXPECT_TRUE(!context.test.source().isActive());
        context.test.source().schedule("[Test] BaseCallback",
            std::function<bool ()>([&] {
                EXPECT_TRUE(context.test.source().isActive() && !context.test.source().isScheduled());
                context.firstCallbackCallCount++;
                context.test.source().schedule("[Test] ReschedulingCallback",
                    std::function<bool ()>([&] {
                        EXPECT_TRUE(context.test.source().isActive() && !context.test.source().isScheduled());
                        context.secondCallbackCallCount++;
                        return context.secondCallbackCallCount != 3;
                    }), G_PRIORITY_DEFAULT,
                    [&] {
                        EXPECT_TRUE(!context.test.source().isActive());
                        context.secondDestroyCallbackCalled = true;
                    });
                EXPECT_TRUE(context.test.source().isScheduled());
                return true;
            }), G_PRIORITY_DEFAULT,
            [&] {
                // At this point the GMainLoopSource has been rescheduled, ergo the Scheduled status.
                EXPECT_TRUE(context.test.source().isScheduled());
                context.firstDestroyCallbackCalled = true;
            });
        EXPECT_TRUE(context.test.source().isScheduled());

        context.test.delayedFinish();
        context.test.runLoop();

        EXPECT_TRUE(!context.test.source().isActive());
        EXPECT_EQ(1, context.firstCallbackCallCount);
        EXPECT_TRUE(context.firstDestroyCallbackCalled);
        EXPECT_EQ(3, context.secondCallbackCallCount);
        EXPECT_TRUE(context.secondDestroyCallbackCalled);
    }
}

TEST(WTF_GMainLoopSource, DestroyCallbacksAfterReschedulingDuringDispatch)
{
    struct TestingContext {
        GMainLoopSourceTest<GMainLoopSource> test;
        unsigned firstCallbackCallCount = 0;
        bool firstDestroyCallbackCalled = false;
        unsigned secondCallbackCallCount = 0;
        bool secondDestroyCallbackCalled = false;
    };
    destroyCallbacksAfterReschedulingDuringDispatch<TestingContext>();

    struct ThreadSafeTestingContext {
        GMainLoopSourceTest<GThreadSafeMainLoopSource> test;
        unsigned firstCallbackCallCount = 0;
        bool firstDestroyCallbackCalled = false;
        unsigned secondCallbackCallCount = 0;
        bool secondDestroyCallbackCalled = false;
    };
    destroyCallbacksAfterReschedulingDuringDispatch<ThreadSafeTestingContext>();
}

TEST(WTF_GMainLoopSource, DeleteOnDestroySources)
{
    // Testing the delete-on-destroy sources is very limited. There's no good way
    // of testing that the GMainLoopSource objects are deleted when their GSource
    // is destroyed.

    struct TestingContext {
        GMainLoopSourceTest<GMainLoopSource> test;
        unsigned callbackCallCount = 0;
        bool destroyCallbackCalled = false;
    } context;

    {
        TestingContext context;

        GMainLoopSource::scheduleAndDeleteOnDestroy("[Test] DeleteOnDestroy",
            [&] {
                context.callbackCallCount++;
            }, G_PRIORITY_DEFAULT,
            [&] {
                EXPECT_FALSE(context.destroyCallbackCalled);
                context.destroyCallbackCalled = true;
            });

        context.test.delayedFinish();
        context.test.runLoop();
        EXPECT_EQ(1, context.callbackCallCount);
        EXPECT_TRUE(context.destroyCallbackCalled);
    }

    {
        TestingContext context;

        GMainLoopSource::scheduleAndDeleteOnDestroy("[Test] DeleteOnDestroy",
            std::function<bool ()>([&] {
                context.callbackCallCount++;
                return context.callbackCallCount != 3;
            }), G_PRIORITY_DEFAULT,
            [&] {
                EXPECT_FALSE(context.destroyCallbackCalled);
                context.destroyCallbackCalled = true;
            });

        context.test.delayedFinish();
        context.test.runLoop();
        EXPECT_EQ(3, context.callbackCallCount);
        EXPECT_TRUE(context.destroyCallbackCalled);
    }
}

} // namespace TestWebKitAPI
