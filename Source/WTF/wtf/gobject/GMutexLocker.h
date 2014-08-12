/*
 *  Copyright (C) 2013 Collabora Ltd.
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

#ifndef GMutexLocker_h
#define GMutexLocker_h

#if USE(GLIB)

#include <glib.h>
#include <wtf/Noncopyable.h>

namespace WTF {

class GMutexLocker {
    WTF_MAKE_NONCOPYABLE(GMutexLocker);
public:
    explicit GMutexLocker(GMutex& mutex)
        : m_mutex(mutex)
        , m_locked(false)
    {
        lock();
    }

    ~GMutexLocker()
    {
        unlock();
    }

    void lock()
    {
        if (!m_locked) {
            g_mutex_lock(&m_mutex);
            m_locked = true;
        }
    }

    void unlock()
    {
        if (m_locked) {
            m_locked = false;
            g_mutex_unlock(&m_mutex);
        }
    }

private:
    GMutex& m_mutex;
    bool m_locked;
};

} // namespace WTF

using WTF::GMutexLocker;

#endif // USE(GLIB)

#endif // GMutexLocker_h
