/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL GOOGLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(UNDO_MANAGER)

#include "V8UndoManager.h"

#include "DOMTransaction.h"
#include "ExceptionCode.h"
#include "V8DOMTransaction.h"

namespace WebCore {

v8::Handle<v8::Value> V8UndoManager::transactCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.UndoManager.transact");
    if (args.Length() < 1)
        return throwNotEnoughArgumentsError(args.GetIsolate());
    UndoManager* imp = V8UndoManager::toNative(args.Holder());

    EXCEPTION_BLOCK(v8::Local<v8::Value>, dictionary, MAYBE_MISSING_PARAMETER(args, 0, DefaultIsUndefined));
    if (dictionary.IsEmpty() || !dictionary->IsObject())
        return throwTypeError("The first argument is not of type DOMTransaction.", args.GetIsolate());

    EXCEPTION_BLOCK(bool, merge, MAYBE_MISSING_PARAMETER(args, 1, DefaultIsUndefined)->BooleanValue());

    RefPtr<DOMTransaction> transaction = DOMTransaction::create(WorldContextHandle(UseCurrentWorld));
    transaction->setData(dictionary);

    ExceptionCode ec = 0;
    imp->transact(transaction, merge, ec);
    if (ec)
        return setDOMException(ec, args.GetIsolate());
    return v8Undefined();
}

v8::Handle<v8::Value> V8UndoManager::itemCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.UndoManager.item");
    if (args.Length() < 1)
        return throwNotEnoughArgumentsError(args.GetIsolate());
    UndoManager* imp = V8UndoManager::toNative(args.Holder());

    EXCEPTION_BLOCK(unsigned, index, toUInt32(MAYBE_MISSING_PARAMETER(args, 0, DefaultIsUndefined)));

    if (index >= imp->length())
        return v8::Null(args.GetIsolate());

    const UndoManagerEntry& entry = imp->item(index);

    v8::Handle<v8::Array> result = v8::Array::New(entry.size());
    v8::Isolate* isolate = args.GetIsolate();
    for (size_t index = 0; index < entry.size(); ++index) {
        UndoStep* step = entry[index].get();
        if (step->isDOMTransaction())
            result->Set(v8Integer(index, isolate), static_cast<DOMTransaction*>(step)->data());
        else {
            // FIXME: We shouldn't be creating new object each time we return.
            // Object for the same native editing command should always be the same.
            v8::Handle<v8::Object> object = v8::Object::New();
            object->Set(v8::String::NewSymbol("label"), v8::String::New("[Editing command]"));
            result->Set(v8Integer(index, isolate), object);
        }
    }
    return result;
}

} // namespace WebCore

#endif // ENABLE(UNDO_MANAGER)
