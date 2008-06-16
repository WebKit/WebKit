/*
    Copyright (C) 2006, 2008 Apple Inc. All rights reserved.

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

#ifndef JSSVGLazyEventListener_h
#define JSSVGLazyEventListener_h

#include "JSEventListener.h"

#if ENABLE(SVG)

namespace WebCore {

    class JSSVGLazyEventListener : public JSLazyEventListener {
    public:
        static PassRefPtr<JSSVGLazyEventListener> create(const String& functionName, const String& code, JSDOMWindow* window, Node* node, int lineNumber)
        {
            return adoptRef(new JSSVGLazyEventListener(functionName, code, window, node, lineNumber));
        }

    private:
        JSSVGLazyEventListener(const String& functionName, const String& code, JSDOMWindow*, Node*, int lineNumber);

        virtual KJS::JSValue* eventParameterName() const;
    };

} // namespace WebCore

#endif // ENABLE(SVG)

#endif // JSSVGLazyEventListener_h
