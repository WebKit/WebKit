/*
    Copyright (C) 2006 Apple Computer, Inc.
                  
    This file is part of the WebKit project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#if ENABLE(SVG)

#include "JSSVGLazyEventListener.h"

#include "JSDOMWindow.h"

using namespace KJS;

namespace WebCore {

JSSVGLazyEventListener::JSSVGLazyEventListener(const String& functionName, const String& code, JSDOMWindow* window, Node* node, int lineNumber)
    : JSLazyEventListener(functionName, code, window, node, lineNumber)
{
}

JSValue* JSSVGLazyEventListener::eventParameterName() const
{
    static ProtectedPtr<JSValue> eventString = jsString(window()->globalExec(), "evt");
    return eventString.get();
}

} // namespace WebCore

#endif // ENABLE(SVG)
