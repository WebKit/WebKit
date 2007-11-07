/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSHTMLDocument.h"

#include "Frame.h"
#include "HTMLBodyElement.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "JSHTMLCollection.h"
#include "kjs_html.h"
#include "kjs_window.h"

namespace WebCore {

using namespace KJS;
using namespace HTMLNames;

bool JSHTMLDocument::canGetItemsForName(ExecState*, HTMLDocument* doc, const Identifier& propertyName)
{
    return doc->hasNamedItem(propertyName) || doc->hasDocExtraNamedItem(propertyName);
}

JSValue* JSHTMLDocument::nameGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLDocument* thisObj = static_cast<JSHTMLDocument*>(slot.slotBase());
    HTMLDocument* doc = static_cast<HTMLDocument*>(thisObj->impl());

    String name = propertyName;
    RefPtr<HTMLCollection> collection = doc->documentNamedItems(name);

    unsigned length = collection->length();
    if (!length)
        return jsUndefined();

    if (length == 1) {
        Node* node = collection->firstItem();

        Frame* frame;
        if (node->hasTagName(iframeTag) && (frame = static_cast<HTMLIFrameElement*>(node)->contentFrame()))
            return KJS::Window::retrieve(frame);

        return toJS(exec, node);
    } 

    return toJS(exec, collection.get());
}

// Custom attributes

JSValue* JSHTMLDocument::all(ExecState* exec) const
{
    // If "all" has been overwritten, return the overwritten value
    if (JSValue* v = getDirect("all"))
        return v;

    return toJS(exec, static_cast<HTMLDocument*>(impl())->all().get());
}

void JSHTMLDocument::setAll(ExecState*, JSValue* value)
{
    // Add "all" to the property map.
    putDirect("all", value);
}

// Custom functions

JSValue* JSHTMLDocument::open(ExecState* exec, const List& args)
{
    // For compatibility with other browsers, pass open calls with more than 2 parameters to the window.
    if (args.size() > 2) {
        Frame* frame = static_cast<HTMLDocument*>(impl())->frame();
        if (frame) {
            KJS::Window* window = KJS::Window::retrieveWindow(frame);
            if (window) {
                JSObject* functionObject = window->get(exec, "open")->getObject();
                if (!functionObject || !functionObject->implementsCall())
                    return throwError(exec, TypeError);
                return functionObject->call(exec, window, args);
            }
        }
        return jsUndefined();
    }

    // In the case of two parameters or fewer, do a normal document open.
    static_cast<HTMLDocument*>(impl())->open();
    return jsUndefined();
}

static String writeHelper(ExecState* exec, const List& args)
{
    // DOM only specifies single string argument, but NS & IE allow multiple
    // or no arguments.
    String str = "";
    for (unsigned int i = 0; i < args.size(); ++i)
        str += args[i]->toString(exec);
    return str;
}

JSValue* JSHTMLDocument::write(ExecState* exec, const List& args)
{
    static_cast<HTMLDocument*>(impl())->write(writeHelper(exec, args));
    return jsUndefined();
}

JSValue* JSHTMLDocument::writeln(ExecState* exec, const List& args)
{
    static_cast<HTMLDocument*>(impl())->write(writeHelper(exec, args) + "\n");
    return jsUndefined();
}

JSValue* JSHTMLDocument::clear(ExecState*, const List&)
{
    // even IE doesn't support this one...
    // static_cast<HTMLDocument*>(impl())->clear();
    return jsUndefined();
}

} // namespace WebCore
