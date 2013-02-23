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

namespace WebCore {

v8::Handle<v8::Value> V8Clipboard::typesAttrGetterCustom(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    Clipboard* clipboard = V8Clipboard::toNative(info.Holder());

    ListHashSet<String> types = clipboard->types();
    if (types.isEmpty())
        return v8Null(info.GetIsolate());

    v8::Local<v8::Array> result = v8::Array::New(types.size());
    ListHashSet<String>::const_iterator end = types.end();
    int index = 0;
    for (ListHashSet<String>::const_iterator it = types.begin(); it != end; ++it, ++index)
        result->Set(v8Integer(index, info.GetIsolate()), v8String(*it, info.GetIsolate()));

    return result;
}

v8::Handle<v8::Value> V8Clipboard::clearDataMethodCustom(const v8::Arguments& args)
{
    Clipboard* clipboard = V8Clipboard::toNative(args.Holder());

    if (!args.Length()) {
        clipboard->clearAllData();
        return v8::Undefined();
    }

    if (args.Length() != 1)
        return throwError(v8SyntaxError, "clearData: Invalid number of arguments", args.GetIsolate());

    String type = toWebCoreString(args[0]);
    clipboard->clearData(type);
    return v8::Undefined();
}

v8::Handle<v8::Value> V8Clipboard::setDragImageMethodCustom(const v8::Arguments& args)
{
    Clipboard* clipboard = V8Clipboard::toNative(args.Holder());

    if (!clipboard->isForDragAndDrop())
        return v8::Undefined();

    if (args.Length() != 3)
        return throwError(v8SyntaxError, "setDragImage: Invalid number of arguments", args.GetIsolate());

    int x = toInt32(args[1]);
    int y = toInt32(args[2]);

    Node* node = 0;
    if (V8Node::HasInstance(args[0], args.GetIsolate()))
        node = V8Node::toNative(v8::Handle<v8::Object>::Cast(args[0]));

    if (!node || !node->isElementNode())
        return throwTypeError("setDragImageFromElement: Invalid first argument", args.GetIsolate());

    if (static_cast<Element*>(node)->hasLocalName(HTMLNames::imgTag) && !node->inDocument())
        clipboard->setDragImage(static_cast<HTMLImageElement*>(node)->cachedImage(), IntPoint(x, y));
    else
        clipboard->setDragImageElement(node, IntPoint(x, y));

    return v8::Undefined();
}

} // namespace WebCore
