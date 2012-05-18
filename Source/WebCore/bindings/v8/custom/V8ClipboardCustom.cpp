/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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
#include "V8Clipboard.h"

#include "Clipboard.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "IntPoint.h"
#include "Node.h"
#include "Element.h"

#include "V8Binding.h"
#include "V8Node.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8Clipboard::typesAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Clipboard.types()");
    Clipboard* clipboard = V8Clipboard::toNative(info.Holder());

    HashSet<String> types = clipboard->types();
    if (types.isEmpty())
        return v8::Null();

    v8::Local<v8::Array> result = v8::Array::New(types.size());
    HashSet<String>::const_iterator end = types.end();
    int index = 0;
    for (HashSet<String>::const_iterator it = types.begin(); it != end; ++it, ++index)
        result->Set(v8::Integer::New(index), v8String(*it));

    return result;
}

v8::Handle<v8::Value> V8Clipboard::clearDataCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Clipboard.clearData()");
    Clipboard* clipboard = V8Clipboard::toNative(args.Holder());

    if (!args.Length()) {
        clipboard->clearAllData();
        return v8::Undefined();
    }

    if (args.Length() != 1)
        return V8Proxy::throwError(V8Proxy::SyntaxError, "clearData: Invalid number of arguments");

    String type = toWebCoreString(args[0]);
    clipboard->clearData(type);
    return v8::Undefined();
}

v8::Handle<v8::Value> V8Clipboard::setDragImageCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Clipboard.setDragImage()");
    Clipboard* clipboard = V8Clipboard::toNative(args.Holder());

    if (!clipboard->isForDragAndDrop())
        return v8::Undefined();

    if (args.Length() != 3)
        return V8Proxy::throwError(V8Proxy::SyntaxError, "setDragImage: Invalid number of arguments");

    int x = toInt32(args[1]);
    int y = toInt32(args[2]);

    Node* node = 0;
    if (V8Node::HasInstance(args[0]))
        node = V8Node::toNative(v8::Handle<v8::Object>::Cast(args[0]));

    if (!node || !node->isElementNode())
        return V8Proxy::throwTypeError("setDragImageFromElement: Invalid first argument");

    if (static_cast<Element*>(node)->hasLocalName(HTMLNames::imgTag) && !node->inDocument())
        clipboard->setDragImage(static_cast<HTMLImageElement*>(node)->cachedImage(), IntPoint(x, y));
    else
        clipboard->setDragImageElement(node, IntPoint(x, y));

    return v8::Undefined();
}

} // namespace WebCore
