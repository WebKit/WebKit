/*
 *  Copyright (C) 1999 Harri Porten (porten@kde.org)
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

#ifndef kjs_html_h
#define kjs_html_h

#include "JSDOMBinding.h"

namespace WebCore {

    class HTMLElement;
    class JSHTMLElement;
    class Node;

    // Runtime object support code for JSHTMLAppletElement, JSHTMLEmbedElement and JSHTMLObjectElement.
    // FIXME: Move these to a more appropriate place.

    KJS::JSValue* runtimeObjectGetter(KJS::ExecState*, const KJS::Identifier&, const KJS::PropertySlot&);
    KJS::JSValue* runtimeObjectPropertyGetter(KJS::ExecState*, const KJS::Identifier&, const KJS::PropertySlot&);
    bool runtimeObjectCustomGetOwnPropertySlot(KJS::ExecState*, const KJS::Identifier&, KJS::PropertySlot&, JSHTMLElement*, HTMLElement*);
    bool runtimeObjectCustomPut(KJS::ExecState*, const KJS::Identifier&, KJS::JSValue*, HTMLElement*);
    bool runtimeObjectImplementsCall(HTMLElement*);
    KJS::JSValue* runtimeObjectCallAsFunction(KJS::ExecState*, KJS::JSObject*, const KJS::ArgList&, HTMLElement*);

} // namespace WebCore

#endif // kjs_html_h
