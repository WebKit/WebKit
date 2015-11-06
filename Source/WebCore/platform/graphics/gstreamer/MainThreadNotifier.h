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

#ifndef MainThreadNotifier_h
#define MainThreadNotifier_h

#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

template <typename T>
class MainThreadNotifier {
public:
    MainThreadNotifier()
        : m_weakPtrFactory(this)
    {
    }

    template<typename F>
    void notify(T notificationType, const F& callbackFunctor)
    {
        if (isMainThread()) {
            removePendingNotification(notificationType);
            callbackFunctor();
            return;
        }

        if (!addPendingNotification(notificationType))
            return;

        auto weakThis = m_weakPtrFactory.createWeakPtr();
        std::function<void ()> callback(callbackFunctor);
        RunLoop::main().dispatch([weakThis, notificationType, callback] {
            if (weakThis && weakThis->removePendingNotification(notificationType))
                callback();
        });
    }

    void cancelPendingNotifications(unsigned mask = 0)
    {
        LockHolder locker(m_pendingNotificationsLock);
        if (mask)
            m_pendingNotifications &= ~mask;
        else
            m_pendingNotifications = 0;
    }

private:

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

    WeakPtrFactory<MainThreadNotifier> m_weakPtrFactory;
    Lock m_pendingNotificationsLock;
    unsigned m_pendingNotifications { 0 };
};


} // namespace WebCore

#endif // MainThreadNotifier_h
