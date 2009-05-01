/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef JSRGBColor_h
#define JSRGBColor_h

#include "Color.h"
#include "JSDOMBinding.h"

namespace WebCore {

    // FIXME: JSRGBColor should have a proper prototype and a constructor.
    class JSRGBColor : public DOMObject {
    public:
        JSRGBColor(JSC::ExecState*, unsigned color);

        virtual bool getOwnPropertySlot(JSC::ExecState*, const JSC::Identifier&, JSC::PropertySlot&);

        virtual const JSC::ClassInfo* classInfo() const { return &s_info; }
        static const JSC::ClassInfo s_info;

        unsigned impl() const { return m_color; }

        static JSC::ObjectPrototype* createPrototype(JSC::ExecState*, JSC::JSGlobalObject* globalObject)
        {
            return globalObject->objectPrototype();
        }

        static PassRefPtr<JSC::Structure> createStructure(JSC::JSValue prototype)
        {
            return JSC::Structure::create(prototype, JSC::TypeInfo(JSC::ObjectType));
        }

    private:
        unsigned m_color;
    };

    JSC::JSValue getJSRGBColor(JSC::ExecState*, unsigned color);

} // namespace WebCore

#endif // JSRGBColor_h
