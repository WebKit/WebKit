// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef kjs_html_h
#define kjs_html_h

#include "kjs_dom.h"

namespace WebCore {

    class Document;
    class HTMLCollection;
    class HTMLElement;
    class JSHTMLElement;

    class ImageConstructorImp : public KJS::DOMObject {
    public:
        ImageConstructorImp(KJS::ExecState*, Document*);

        virtual bool implementsConstruct() const { return true; }
        virtual KJS::JSObject* construct(KJS::ExecState*, const KJS::List&);

    private:
        RefPtr<Document> m_doc;
    };


    // Runtime object support code for JSHTMLAppletElement, JSHTMLEmbedElement and JSHTMLObjectElement.
    // FIXME: Move these to a more appropriate place.

    KJS::JSValue* runtimeObjectGetter(KJS::ExecState*, KJS::JSObject*, const KJS::Identifier&, const KJS::PropertySlot&);
    KJS::JSValue* runtimeObjectPropertyGetter(KJS::ExecState*, KJS::JSObject*, const KJS::Identifier&, const KJS::PropertySlot&);
    bool runtimeObjectCustomGetOwnPropertySlot(KJS::ExecState*, const KJS::Identifier&, KJS::PropertySlot&, JSHTMLElement*, HTMLElement*);
    bool runtimeObjectCustomPut(KJS::ExecState*, const KJS::Identifier&, KJS::JSValue*, int attr, HTMLElement*);
    bool runtimeObjectImplementsCall(HTMLElement*);
    KJS::JSValue* runtimeObjectCallAsFunction(KJS::ExecState*, KJS::JSObject*, const KJS::List&, HTMLElement*);

} // namespace WebCore

#endif // kjs_html_h
