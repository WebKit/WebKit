/*
 *  Copyright (C) 2015 Igalia S.L
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
 */

#pragma once

#include <functional>
#include <wtf/Atomics.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

template <typename T>
class MainThreadNotifier final : public ThreadSafeRefCounted<MainThreadNotifier<T>> {
public:
    static Ref<MainThreadNotifier> create()
    {
        return adoptRef(*new MainThreadNotifier());
    }

    ~MainThreadNotifier()
    {
        ASSERT(!m_isValid.load());
    }

    bool isValid() const { return m_isValid.load(); }

    template<typename F>
    void notify(T notificationType, F&& callbackFunctor)
    {
        ASSERT(m_isValid.load());
        // Assert that there is only one bit on at a time.
        ASSERT(!(static_cast<int>(notificationType) & (static_cast<int>(notificationType) - 1)));
        if (isMainThread()) {
            removePendingNotification(notificationType);
            callbackFunctor();
            return;
        }

        if (!addPendingNotification(notificationType))
            return;

        RunLoop::main().dispatch([this, protectedThis = makeRef(*this), notificationType, callback = WTF::Function<void()>(WTFMove(callbackFunctor))] {
            if (!m_isValid.load())
                return;
            if (removePendingNotification(notificationType))
                callback();
        });
    }

    template<typename F>
    void notifyAndWait(T notificationType, F&& callbackFunctor)
    {
        Lock mutex;
        Condition condition;

        notify(notificationType, [functor = WTFMove(callbackFunctor), &condition, &mutex] {
            functor();
            LockHolder holder(mutex);
            condition.notifyOne();
        });

        if (!isMainThread()) {
            LockHolder holder(mutex);
            condition.wait(mutex);
        }
    }

    void cancelPendingNotifications(unsigned mask = 0)
    {
        ASSERT(m_isValid.load());
        LockHolder locker(m_pendingNotificationsLock);
        if (mask)
            m_pendingNotifications &= ~mask;
        else
            m_pendingNotifications = 0;
    }

    void invalidate()
    {
        ASSERT(m_isValid.load());
        m_isValid.store(false);
    }

private:
    MainThreadNotifier()
    {
        m_isValid.store(true);
    }

    bool addPendingNotification(T notificationType)
    {
        LockHolder locker(m_pendingNotificationsLock);
        if (notificationType & m_pendingNotifications)
            return false;
        m_pendingNotifications |= notificationType;
        return true;
    }

    bool removePendingNotification(T notificationType)
    {
        LockHolder locker(m_pendingNotificationsLock);
        if (notificationType & m_pendingNotifications) {
            m_pendingNotifications &= ~notificationType;
            return true;
        }
        return false;
    }

    Lock m_pendingNotificationsLock;
    unsigned m_pendingNotifications { 0 };
    Atomic<bool> m_isValid;
};

} // namespace WebCore

