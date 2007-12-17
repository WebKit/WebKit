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

#ifndef ScheduledAction_h
#define ScheduledAction_h

#include "PlatformString.h"
#include <kjs/protect.h>
#include <wtf/Vector.h>

namespace KJS {
    class Window;
    class JSValue;
    class List;
}

namespace WebCore {

  /**
   * An action (either function or string) to be executed after a specified
   * time interval, either once or repeatedly. Used for window.setTimeout()
   * and window.setInterval()
   */
    class ScheduledAction {
    public:
        ScheduledAction(KJS::JSValue* func, const KJS::List& args);
        ScheduledAction(const String& code)
            : m_code(code)
        {
        }

        void execute(KJS::Window*);

    private:
        KJS::ProtectedPtr<KJS::JSValue> m_func;
        Vector<KJS::ProtectedPtr<KJS::JSValue> > m_args;
        String m_code;
    };

} // namespace WebCore

#endif // ScheduledAction_h
