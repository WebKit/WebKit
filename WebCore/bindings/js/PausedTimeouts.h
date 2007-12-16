/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reseved.
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

#ifndef PausedTimeouts_h
#define PausedTimeouts_h

#include <wtf/Noncopyable.h>

namespace KJS {
    class ScheduledAction;
}

namespace WebCore {

    struct PausedTimeout {
        int timeoutId;
        int nestingLevel;
        double nextFireInterval;
        double repeatInterval;
        KJS::ScheduledAction* action;
    };

    class PausedTimeouts : Noncopyable {
    public:
        PausedTimeouts(PausedTimeout* array, size_t length)
            : m_array(array)
            , m_length(length)
        {
        }

        ~PausedTimeouts();

        size_t numTimeouts() const { return m_length; }
        PausedTimeout* takeTimeouts() { PausedTimeout* a = m_array; m_array = 0; return a; }

    private:
        PausedTimeout* m_array;
        size_t m_length;
    };

} // namespace WebCore

#endif // PausedTimeouts_h
