/*
 * Copyright (C) 2007, 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLDocument.h"

#include "Frame.h"
#include "HTMLCollection.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"

#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"

#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

NAMED_PROPERTY_DELETER(HTMLDocument)
{
    // Only handle document.all.  Insert the marker object into the
    // shadow internal field to signal that document.all is no longer
    // shadowed.
    AtomicString key = v8StringToAtomicWebCoreString(name);
    DEFINE_STATIC_LOCAL(const AtomicString, all, ("all"));
    if (key != all)
        return deletionNotHandledByInterceptor();

    ASSERT(info.Holder()->InternalFieldCount() == kHTMLDocumentInternalFieldCount);
    v8::Local<v8::Value> marker = info.Holder()->GetInternalField(kHTMLDocumentMarkerIndex);
    info.Holder()->SetInternalField(kHTMLDocumentShadowIndex, marker);
    return v8::True();
}

NAMED_PROPERTY_GETTER(HTMLDocument)
{
    INC_STATS("DOM.HTMLDocument.NamedPropertyGetter");
    AtomicString key = v8StringToAtomicWebCoreString(name);

    // Special case for document.all.  If the value in the shadow
    // internal field is not the marker object, then document.all has
    // been temporarily shadowed and we return the value.
    DEFINE_STATIC_LOCAL(const AtomicString, all, ("all"));
    if (key == all) {
        ASSERT(info.Holder()->InternalFieldCount() == kHTMLDocumentInternalFieldCount);
        v8::Local<v8::Value> marker = info.Holder()->GetInternalField(kHTMLDocumentMarkerIndex);
        v8::Local<v8::Value> value = info.Holder()->GetInternalField(kHTMLDocumentShadowIndex);
        if (marker != value)
            return value;
    }

    HTMLDocument* htmlDocument = V8Proxy::DOMWrapperToNode<HTMLDocument>(info.Holder());

    // Fast case for named elements that are not there.
    if (!htmlDocument->hasNamedItem(key.impl()) && !htmlDocument->hasExtraNamedItem(key.impl()))
        return v8::Handle<v8::Value>();

    RefPtr<HTMLCollection> items = htmlDocument->documentNamedItems(key);
    if (!items->length())
        return notHandledByInterceptor();

    if (items->length() == 1) {
        Node* node = items->firstItem();
        Frame* frame = 0;
        if (node->hasTagName(HTMLNames::iframeTag) && (frame = static_cast<HTMLIFrameElement*>(node)->contentFrame()))
            return V8Proxy::ToV8Object(V8ClassIndex::DOMWINDOW, frame->domWindow());

        return V8Proxy::NodeToV8Object(node);
    }

    return V8Proxy::ToV8Object(V8ClassIndex::HTMLCOLLECTION, items.get());
}

// HTMLDocument ----------------------------------------------------------------

// Concatenates "args" to a string. If args is empty, returns empty string.
// Firefox/Safari/IE support non-standard arguments to document.write, ex:
//   document.write("a", "b", "c") --> document.write("abc")
//   document.write() --> document.write("")
static String writeHelperGetString(const v8::Arguments& args)
{
    String str = "";
    for (int i = 0; i < args.Length(); ++i)
        str += toWebCoreString(args[i]);
    return str;
}

CALLBACK_FUNC_DECL(HTMLDocumentWrite)
{
    INC_STATS("DOM.HTMLDocument.write()");
    HTMLDocument* htmlDocument = V8Proxy::DOMWrapperToNode<HTMLDocument>(args.Holder());
    Frame* frame = V8Proxy::retrieveFrameForEnteredContext();
    ASSERT(frame);
    htmlDocument->write(writeHelperGetString(args), frame->document());
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(HTMLDocumentWriteln)
{
    INC_STATS("DOM.HTMLDocument.writeln()");
    HTMLDocument* htmlDocument = V8Proxy::DOMWrapperToNode<HTMLDocument>(args.Holder());
    Frame* frame = V8Proxy::retrieveFrameForEnteredContext();
    ASSERT(frame);
    htmlDocument->writeln(writeHelperGetString(args), frame->document());
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(HTMLDocumentOpen)
{
    INC_STATS("DOM.HTMLDocument.open()");
    HTMLDocument* htmlDocument = V8Proxy::DOMWrapperToNode<HTMLDocument>(args.Holder());

    if (args.Length() > 2) {
        if (Frame* frame = htmlDocument->frame()) {
            // Fetch the global object for the frame.
            v8::Local<v8::Context> context = V8Proxy::GetContext(frame);
            // Bail out if we cannot get the context.
            if (context.IsEmpty())
                return v8::Undefined();
            v8::Local<v8::Object> global = context->Global();
            // Get the open property of the global object.
            v8::Local<v8::Value> function = global->Get(v8::String::New("open"));
            // If the open property is not a function throw a type error.
            if (!function->IsFunction()) {
                throwError("open is not a function");
                return v8::Undefined();
            }
            // Wrap up the arguments and call the function.
            v8::Local<v8::Value>* params = new v8::Local<v8::Value>[args.Length()];
            for (int i = 0; i < args.Length(); i++)
                params[i] = args[i];

            V8Proxy* proxy = V8Proxy::retrieve(frame);
            ASSERT(proxy);

            v8::Local<v8::Value> result = proxy->CallFunction(v8::Local<v8::Function>::Cast(function), global, args.Length(), params);
            delete[] params;
            return result;
        }
    }

    Frame* frame = V8Proxy::retrieveFrameForEnteredContext();
    htmlDocument->open(frame->document());
    // Return the document.
    return args.Holder();
}

ACCESSOR_GETTER(HTMLDocumentAll)
{
    INC_STATS("DOM.HTMLDocument.all._get");
    v8::HandleScope scope;
    v8::Handle<v8::Object> holder = info.Holder();
    HTMLDocument* htmlDocument = V8Proxy::DOMWrapperToNode<HTMLDocument>(holder);
    RefPtr<HTMLCollection> collection = WTF::getPtr(htmlDocument->all());
    return V8Proxy::ToV8Object(V8ClassIndex::HTMLCOLLECTION, WTF::getPtr(collection));
}

ACCESSOR_SETTER(HTMLDocumentAll)
{
    INC_STATS("DOM.HTMLDocument.all._set");
    v8::Handle<v8::Object> holder = info.Holder();
    ASSERT(info.Holder()->InternalFieldCount() == kHTMLDocumentInternalFieldCount);
    info.Holder()->SetInternalField(kHTMLDocumentShadowIndex, value);
}

} // namespace WebCore
