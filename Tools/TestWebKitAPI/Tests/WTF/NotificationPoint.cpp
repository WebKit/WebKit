/*
 *  Copyright (C) 2026 Igalia. S.L. . All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include <wtf/NotificationPoint.h>

#include <wtf/BitSet.h>
#include <wtf/Seconds.h>
#if USE(GLIB)
#include <glib.h>
#else
#include <dispatch/dispatch.h>
#endif

#define TEST_NOTIFICATION_POINT_OWNER "com.example.testme"_s

#if USE(GLIB)
class NotificationPointTestExecutor {
public:
    void waitForNotificationPointToGoLive(NotificationPoint* n)
    {
        for (; !n->isLive();)
            g_main_context_iteration(g_main_context_default(), false);
    }

    void callbackHasRun(unsigned = 0) { }
    void waitForNotification(unsigned = 0) { }
    void waitForAllNotifications() { }
    void synchronize() { }

    void run(Function<void()>operation)
    {
        operation();
        g_main_context_iteration(g_main_context_default(), false);
    }

    NotificationPointTestExecutor(long = 0)
    {
        m_loop = g_main_loop_new(g_main_context_default(), true);
    }

    ~NotificationPointTestExecutor()
    {
        g_main_loop_quit(m_loop);
    }
private:
    GMainLoop* m_loop;
};

#elif OS(DARWIN)
class NotificationPointTestExecutor {
public:
    void waitForNotificationPointToGoLive(NotificationPoint*) { }

    void callbackHasRun(unsigned i = 0)
    {
        dispatch_semaphore_signal(m_semaphores[i]);
    }

    void waitForNotification(unsigned i = 0)
    {
        dispatch_semaphore_wait(m_semaphores[i], DISPATCH_TIME_FOREVER);
    }

    void waitForAllNotifications()
    {
        size_t count = m_semaphores.size();
        WTF::BitSet<3> completed;
        RELEASE_ASSERT(completed.size() <= m_semaphores.size());
        static constexpr int64_t ns = 100 * MSEC_PER_SEC;

        do {
            for (size_t i = 0; i < count; ++i) {
                if (dispatch_semaphore_wait(m_semaphores[i], dispatch_time(DISPATCH_TIME_NOW, ns)))
                    continue;
                completed.set(i);
            }
        } while (completed.count() < count);
    }

    void synchronize()
    {
        auto cb = [this]() {
            callbackHasRun();
        };
        // We create a new notification point and do a round-trip, in the hopes
        // that whatever notify/setState action we took before will also have
        // completed. Would be great to have a better way to synchronize with
        // setState.
        auto result = NotificationPoint::createWithName(TEST_NOTIFICATION_POINT_OWNER, "internal"_s, WTF::move(cb));
        ASSERT_TRUE(result.has_value());
        RefPtr<NotificationPoint> point = result.value();
        point->notify();
        waitForNotification();
    }

    void run(Function<void()>operation)
    {
        operation();
    }

    NotificationPointTestExecutor(long count = 1)
        : m_semaphores(count)
    {
        m_semaphores = Vector<dispatch_semaphore_t>();
        for (long i = 0; i < count; ++i)
            m_semaphores.append(dispatch_semaphore_create(0));
        m_queue = dispatch_queue_create("testNotificationPointQueue", DISPATCH_QUEUE_CONCURRENT);
        NotificationPoint::testWTFsetDispatchQueue(m_queue);
    }

    ~NotificationPointTestExecutor()
    {
        dispatch_release(m_queue);
        for (auto sema : m_semaphores)
            dispatch_release(sema);
    }
private :
    Vector<dispatch_semaphore_t> m_semaphores;
    dispatch_queue_t m_queue;
};
#endif

TEST(WTF, TestNotificationPointNotify)
{
    auto executor = NotificationPointTestExecutor();
    bool callbackHasRun = false;
    {
        RefPtr<NotificationPoint> point;
        executor.run([&point, &callbackHasRun, &executor] {
            auto f = [&callbackHasRun, &executor]() {
                callbackHasRun = true;
                executor.callbackHasRun();
            };
            auto result = NotificationPoint::createWithName(TEST_NOTIFICATION_POINT_OWNER, "test"_s, WTF::move(f));
            ASSERT_TRUE(result.has_value());
            point = result.value();
            executor.waitForNotificationPointToGoLive(point);
        });
        executor.run([&] {
            point->notify();
            executor.waitForNotification();
        });
        executor.run([&callbackHasRun] {
            ASSERT_TRUE(callbackHasRun);
        });
    }
    ASSERT_TRUE(NotificationPoint::testWTFisEmpty());
}

TEST(WTF, TestNotificationPointState)
{
    auto executor = NotificationPointTestExecutor();
    ASCIILiteral path = "stateful"_s;
    {
        RefPtr<NotificationPoint> point;
        executor.run([&] {
            auto result = NotificationPoint::createWithName(TEST_NOTIFICATION_POINT_OWNER, path, nullptr);
            ASSERT_TRUE(result.has_value());
            point = result.value();
            executor.waitForNotificationPointToGoLive(point);
        });
        executor.run([&] {
            auto state = point->getState();
            ASSERT_TRUE(state.has_value());
            ASSERT_EQ(state.value(), static_cast<uint64_t>(0));
            state = NotificationPoint::getState(TEST_NOTIFICATION_POINT_OWNER, path);
            ASSERT_TRUE(state.has_value());
            ASSERT_EQ(state.value(), static_cast<uint64_t>(0));
        });
        executor.run([&] {

            ASSERT_TRUE(point->setState(7).has_value());
            executor.synchronize();

            auto state = point->getState();
            ASSERT_TRUE(state.has_value());
            ASSERT_EQ(state.value(), static_cast<uint64_t>(7));

            ASSERT_TRUE(NotificationPoint::setState(TEST_NOTIFICATION_POINT_OWNER, path, 8).has_value());
            executor.synchronize();

            state = NotificationPoint::getState(TEST_NOTIFICATION_POINT_OWNER, path);
            ASSERT_TRUE(state.has_value());
            ASSERT_EQ(state.value(), static_cast<uint64_t>(8));
        });
    }
    ASSERT_TRUE(NotificationPoint::testWTFisEmpty());
}

TEST(WTF, TestNotificationPointStateWithCallback)
{
    auto executor = NotificationPointTestExecutor();
    ASCIILiteral path = "stateful"_s;
    {
        RefPtr<NotificationPoint> point;
        bool callbackHasRun = false;
        auto callback = [&]() {
            callbackHasRun = true;
            executor.callbackHasRun();
        };
        executor.run([&] {
            auto result = NotificationPoint::createWithName(TEST_NOTIFICATION_POINT_OWNER, path, WTF::move(callback));
            ASSERT_TRUE(result.has_value());
            point = result.value();
            executor.waitForNotificationPointToGoLive(point);
        });
        executor.run([&] {
            auto state = point->getState();
            ASSERT_TRUE(state.has_value());
            ASSERT_EQ(state.value(), static_cast<uint64_t>(0));
            state = NotificationPoint::getState(TEST_NOTIFICATION_POINT_OWNER, path);
            ASSERT_TRUE(state.has_value());
            ASSERT_EQ(state.value(), static_cast<uint64_t>(0));
        });
        executor.run([&] {
            ASSERT_TRUE(point->setState(7).has_value());
            executor.synchronize();

            auto state = point->getState();
            ASSERT_TRUE(state.has_value());
            ASSERT_EQ(state.value(), static_cast<uint64_t>(7));

            ASSERT_TRUE(NotificationPoint::setState(TEST_NOTIFICATION_POINT_OWNER, path, 8).has_value());
            executor.synchronize();

            state = NotificationPoint::getState(TEST_NOTIFICATION_POINT_OWNER, path);
            ASSERT_TRUE(state.has_value());
            ASSERT_EQ(state.value(), static_cast<uint64_t>(8));
        });
        ASSERT_FALSE(callbackHasRun);
    }
    ASSERT_TRUE(NotificationPoint::testWTFisEmpty());
}

TEST(WTF, TestNotificationPointPendingNotifications)
{
    auto executor = NotificationPointTestExecutor();
    ASCIILiteral path = "pending"_s;
    {
        RefPtr<NotificationPoint> point;

        executor.run([&] {
            auto result = NotificationPoint::createWithName(TEST_NOTIFICATION_POINT_OWNER, path, nullptr);
            ASSERT_TRUE(result.has_value());
            point = result.value();
            executor.waitForNotificationPointToGoLive(point);
        });
        executor.run([&] {
            auto posted = point->getAndClearNotificationsPosted();
            ASSERT_TRUE(posted.has_value());
            // Use same semantics as on Darwin, there's always a posted
            // notification after creation.
            ASSERT_TRUE(posted.value());
        });
        executor.run([&] {
            auto posted = point->getAndClearNotificationsPosted();
            ASSERT_TRUE(posted.has_value());
            // But now that we've retrieved the peosted notification, this should be false.
            ASSERT_FALSE(posted.value());
        });
        executor.run([&] {
            // Post a notification.
            ASSERT_TRUE(NotificationPoint::notify(TEST_NOTIFICATION_POINT_OWNER, path).has_value());
            executor.synchronize();
        });
        executor.run([&] {
            // Is the notification there?
            auto posted = point->getAndClearNotificationsPosted();
            ASSERT_TRUE(posted.has_value());
            ASSERT_TRUE(posted.value());
        });
        executor.run([&] {
            // Is it gone now?
            auto posted = point->getAndClearNotificationsPosted();
            ASSERT_TRUE(posted.has_value());
            ASSERT_FALSE(posted.value());
        });
    }
    ASSERT_TRUE(NotificationPoint::testWTFisEmpty());
}

TEST(WTF, TestNotificationPointSameNamePathWorks)
{
    auto executor = NotificationPointTestExecutor(3);
    bool callback1HasRun = false;
    auto callback1 = [&]() {
        callback1HasRun = true;
        executor.callbackHasRun(0);
    };
    bool callback2HasRun = false;
    auto callback2 = [&]() {
        callback2HasRun = true;
        executor.callbackHasRun(1);
    };
    bool callback3HasRun = false;
    auto callback3 = [&]() {
        callback3HasRun = true;
        executor.callbackHasRun(2);
    };

    ASCIILiteral path = "multiple"_s;
    {
        RefPtr<NotificationPoint> point1, point2, point3;

        executor.run([&] {
            auto result1 = NotificationPoint::createWithName(TEST_NOTIFICATION_POINT_OWNER, path, WTF::move(callback1));
            ASSERT_TRUE(result1.has_value());
            point1 = result1.value();
            ASSERT_NE(point1, nullptr);
            // Immediately try to create a duplicate, without giving the point
            // time to register. This is handled asynchronously for glib, so
            // test the "pending NotificationPoint" path.
            auto result2 = NotificationPoint::createWithName(TEST_NOTIFICATION_POINT_OWNER, path, WTF::move(callback2));
            ASSERT_TRUE(result2.has_value());
            point2 = result2.value();
            ASSERT_NE(point2, nullptr);
            executor.waitForNotificationPointToGoLive(point1);
            executor.waitForNotificationPointToGoLive(point2);
        });
        executor.run([&] {
            // Try to create a duplicate after point1 is certainly live.
            auto result3 = NotificationPoint::createWithName(TEST_NOTIFICATION_POINT_OWNER, path, WTF::move(callback3));
            ASSERT_TRUE(result3.has_value());
            point3 = result3.value();
            ASSERT_NE(point3, nullptr);
            executor.waitForNotificationPointToGoLive(point3);
        });
        ASSERT_FALSE(callback1HasRun);
        ASSERT_FALSE(callback2HasRun);
        ASSERT_FALSE(callback3HasRun);
        executor.run([&] {
            point1->notify();
            executor.waitForAllNotifications();
        });

        ASSERT_TRUE(callback1HasRun);
        ASSERT_TRUE(callback2HasRun);
        ASSERT_TRUE(callback3HasRun);
        ASSERT_TRUE(callback1HasRun && callback2HasRun && callback3HasRun);
        callback1HasRun = callback2HasRun = callback3HasRun = false;
        executor.run([&] {
            point2->notify();
            executor.waitForAllNotifications();
        });
        ASSERT_TRUE(callback1HasRun);
        ASSERT_TRUE(callback2HasRun);
        ASSERT_TRUE(callback3HasRun);
        ASSERT_TRUE(callback1HasRun && callback2HasRun && callback3HasRun);
        callback1HasRun = callback2HasRun = callback3HasRun = false;
        executor.run([&] {
            point3->notify();
            executor.waitForAllNotifications();
        });
        ASSERT_TRUE(callback1HasRun);
        ASSERT_TRUE(callback2HasRun);
        ASSERT_TRUE(callback3HasRun);
        ASSERT_TRUE(callback1HasRun && callback2HasRun && callback3HasRun);
    }
    ASSERT_TRUE(NotificationPoint::testWTFisEmpty());
}

TEST(WTF, TestNotificationPointPendingNotificationsWithCallback)
{
    auto executor = NotificationPointTestExecutor();
    ASCIILiteral path = "pending"_s;
    {
        RefPtr<NotificationPoint> point;
        bool callbackHasRun = false;
        auto callback = [&]() {
            callbackHasRun = true;
            executor.callbackHasRun();
        };

        executor.run([&] {
            auto result = NotificationPoint::createWithName(TEST_NOTIFICATION_POINT_OWNER, path, WTF::move(callback));
            ASSERT_TRUE(result.has_value());
            point = result.value();
            executor.waitForNotificationPointToGoLive(point);
        });
        executor.run([&] {
            auto posted = point->getAndClearNotificationsPosted();
            ASSERT_TRUE(posted.has_value());
            // Use same semantics as on Darwin, there's always a posted
            // notification after creation.
            ASSERT_TRUE(posted.value());
        });
        executor.run([&] {
            auto posted = point->getAndClearNotificationsPosted();
            ASSERT_TRUE(posted.has_value());
            // But now that we've retrieved the peosted notification, this should be false.
            ASSERT_FALSE(posted.value());
        });
        executor.run([&] {
            // Post a notification.
            ASSERT_TRUE(NotificationPoint::notify(TEST_NOTIFICATION_POINT_OWNER, path).has_value());
        });
        executor.run([&] {
            // Is the notification there?
            auto posted = point->getAndClearNotificationsPosted();
            ASSERT_TRUE(posted.has_value());
            ASSERT_TRUE(posted.value());
        });
        executor.run([&] {
            // Is it gone now?
            auto posted = point->getAndClearNotificationsPosted();
            ASSERT_TRUE(posted.has_value());
            ASSERT_FALSE(posted.value());
        });
    }
    ASSERT_TRUE(NotificationPoint::testWTFisEmpty());
}

TEST(WTF, TestNotificationPointMultiple)
{
    {
        Vector<RefPtr<NotificationPoint>> points;
        auto executor = NotificationPointTestExecutor();
        auto point = NotificationPoint::createWithName(TEST_NOTIFICATION_POINT_OWNER "-0", "J0", nullptr);
        ASSERT_TRUE(point.has_value());
        points.append(point.value());
        point = NotificationPoint::createWithName(TEST_NOTIFICATION_POINT_OWNER "-0", "J1", nullptr);
        ASSERT_TRUE(point.has_value());
        points.append(point.value());
        point = NotificationPoint::createWithName(TEST_NOTIFICATION_POINT_OWNER "-1", "J0", nullptr);
        ASSERT_TRUE(point.has_value());
        points.append(point.value());
        point = NotificationPoint::createWithName(TEST_NOTIFICATION_POINT_OWNER "-1", "J1", nullptr);
        ASSERT_TRUE(point.has_value());
        points.append(point.value());
        // Test immediate destruction of the NotificationPoints. This exercises the
        // pendingNotificationPoints() destruction path in the glib implementation.
    }
    ASSERT_TRUE(NotificationPoint::testWTFisEmpty());
}
