/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef JSHTMLInputElementBase_h
#define JSHTMLInputElementBase_h

#include "JSHTMLElement.h"

namespace WebCore {

    class HTMLInputElement;

    class JSHTMLInputElementBase : public JSHTMLElement {
    public:
        JSHTMLInputElementBase(JSC::StructureID*, PassRefPtr<HTMLInputElement>);

        virtual bool getOwnPropertySlot(JSC::ExecState*, const JSC::Identifier&, JSC::PropertySlot&);
        JSC::JSValue* getValueProperty(JSC::ExecState*, int token) const;
        virtual void put(JSC::ExecState*, const JSC::Identifier&, JSValue*, JSC::PutPropertySlot&);
        void putValueProperty(JSC::ExecState*, int token, JSC::JSValue*);
        virtual const JSC::ClassInfo* classInfo() const { return &s_info; }
        static const JSC::ClassInfo s_info;
        enum { SelectionStart, SelectionEnd };
    };

    class JSHTMLInputElementBasePrototype {
    public:
        static JSC::JSObject* self(JSC::ExecState* exec) { return JSHTMLElementPrototype::self(exec); }
    };

} // namespace WebCore

#endif // JSHTMLInputElementBase_h
