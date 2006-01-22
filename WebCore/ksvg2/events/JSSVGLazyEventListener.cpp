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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#if SVG_SUPPORT

#include "JSSVGLazyEventListener.h"

using namespace KJS;

namespace WebCore {

JSSVGLazyEventListener::JSSVGLazyEventListener(const DOMString& code, KJS::Window* win, NodeImpl* node, int lineno)
    : JSLazyEventListener(code, win, node, lineno)
{
}

JSValue *JSSVGLazyEventListener::eventParameterName() const
{
    static ProtectedPtr<JSValue> eventString = jsString("evt");
    return eventString.get();
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
