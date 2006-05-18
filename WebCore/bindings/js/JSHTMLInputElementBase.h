/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef JSHTMLInputElementBase_H
#define JSHTMLInputElementBase_H

#include "kjs_html.h"
#include "kjs_binding.h"

namespace WebCore {

    class HTMLInputElement;

    KJS_DEFINE_PROTOTYPE_WITH_PROTOTYPE(JSHTMLInputElementBaseProto, KJS::JSHTMLElementProto)

    class JSHTMLInputElementBase : public KJS::JSHTMLElement {
    public:
        JSHTMLInputElementBase(KJS::ExecState*, PassRefPtr<HTMLInputElement>);
        virtual bool getOwnPropertySlot(KJS::ExecState*, const KJS::Identifier&, KJS::PropertySlot&);
        KJS::JSValue* getValueProperty(KJS::ExecState*, int token) const;
        virtual void put(KJS::ExecState*, const KJS::Identifier& propertyName, JSValue*, int attr);
        void putValueProperty(KJS::ExecState*, int token, KJS::JSValue*, int attr);
        virtual const KJS::ClassInfo* classInfo() const { return &info; }
        static const KJS::ClassInfo info;
        enum { SetSelectionRange, SelectionStart, SelectionEnd };
    };
}

#endif
