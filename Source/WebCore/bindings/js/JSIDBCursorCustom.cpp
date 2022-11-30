/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSIDBCursor.h"

#include "IDBBindingUtilities.h"
#include "JSDOMBinding.h"
#include "JSIDBCursorWithValue.h"
#include "WebCoreOpaqueRoot.h"


namespace WebCore {
using namespace JSC;

JSC::JSValue JSIDBCursor::key(JSC::JSGlobalObject& lexicalGlobalObject) const
{
    auto throwScope = DECLARE_THROW_SCOPE(lexicalGlobalObject.vm());
    return cachedPropertyValue(throwScope, lexicalGlobalObject, *this, wrapped().keyWrapper(), [&](JSC::ThrowScope&) {
        return toJS(lexicalGlobalObject, lexicalGlobalObject, wrapped().key());
    });
}

JSC::JSValue JSIDBCursor::primaryKey(JSC::JSGlobalObject& lexicalGlobalObject) const
{
    auto throwScope = DECLARE_THROW_SCOPE(lexicalGlobalObject.vm());
    return cachedPropertyValue(throwScope, lexicalGlobalObject, *this, wrapped().primaryKeyWrapper(), [&](JSC::ThrowScope&) {
        return toJS(lexicalGlobalObject, lexicalGlobalObject, wrapped().primaryKey());
    });
}

template<typename Visitor>
void JSIDBCursor::visitAdditionalChildren(Visitor& visitor)
{
    auto& cursor = wrapped();
    if (auto* request = cursor.request())
        addWebCoreOpaqueRoot(visitor, *request);
    cursor.keyWrapper().visit(visitor);
    cursor.primaryKeyWrapper().visit(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSIDBCursor);

JSValue toJSNewlyCreated(JSC::JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<IDBCursor>&& cursor)
{
    if (is<IDBCursorWithValue>(cursor))
        return createWrapper<IDBCursorWithValue>(globalObject, WTFMove(cursor));
    return createWrapper<IDBCursor>(globalObject, WTFMove(cursor));
}

JSValue toJS(JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, IDBCursor& cursor)
{
    return wrap(lexicalGlobalObject, globalObject, cursor);
}

} // namespace WebCore
