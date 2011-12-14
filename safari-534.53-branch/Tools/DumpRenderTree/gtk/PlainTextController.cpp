/*
 * Copyright (C) 2011 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlainTextController.h"

#include "DumpRenderTree.h"
#include "WebCoreSupport/DumpRenderTreeSupportGtk.h"
#include <GOwnPtrGtk.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <webkit/webkit.h>

static JSValueRef plainTextCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    g_return_val_if_fail(argumentCount == 1, JSValueMakeUndefined(context));
    WebKitDOMRange* kitRange = DumpRenderTreeSupportGtk::jsValueToDOMRange(context, arguments[0]);
    g_return_val_if_fail(kitRange, JSValueMakeUndefined(context));

    GOwnPtr<gchar> text(webkit_dom_range_get_text(kitRange));
    JSRetainPtr<JSStringRef> jsText(Adopt, JSStringCreateWithUTF8CString(text.get()));
    return JSValueMakeString(context, jsText.get());
}

JSObjectRef makePlainTextController(JSContextRef context)
{
    static JSStaticFunction staticFunctions[] = {
        { "plainText", plainTextCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 }
    };

    static JSClassRef plainTextControllerClass = 0;
    if (!plainTextControllerClass) {
        JSClassDefinition classDefinition = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        classDefinition.staticFunctions = staticFunctions;
        plainTextControllerClass = JSClassCreate(&classDefinition);
    }
    return JSObjectMake(context, plainTextControllerClass, 0);
}
