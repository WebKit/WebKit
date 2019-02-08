/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "JSIDBRequest.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBBindingUtilities.h"
#include "JSDOMConvertInterface.h"
#include "JSIDBCursor.h"
#include "JSIDBDatabase.h"

namespace WebCore {
using namespace JSC;

JSC::JSValue JSIDBRequest::result(JSC::ExecState& state) const
{
    return cachedPropertyValue(state, *this, wrapped().resultWrapper(), [&] {
        auto result = wrapped().result();
        if (UNLIKELY(result.hasException())) {
            auto throwScope = DECLARE_THROW_SCOPE(state.vm());
            propagateException(state, throwScope, result.releaseException());
            return jsNull();
        }

        IDBRequest::Result resultValue = result.releaseReturnValue();
        return WTF::switchOn(resultValue, [&state] (RefPtr<IDBCursor>& cursor) {
            auto throwScope = DECLARE_THROW_SCOPE(state.vm());
            return toJS<IDLInterface<IDBCursor>>(state, *jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject()), throwScope, cursor.get());
        }, [&state] (RefPtr<IDBDatabase>& database) {
            auto throwScope = DECLARE_THROW_SCOPE(state.vm());
            return toJS<IDLInterface<IDBDatabase>>(state, *jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject()), throwScope, database.get());
        }, [&state] (IDBKeyData keyData) {
            return toJS<IDLIDBKeyData>(state, *jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject()), keyData);
        }, [&state] (Vector<IDBKeyData> keyDatas) {
            return toJS<IDLSequence<IDLIDBKeyData>>(state, *jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject()), keyDatas);
        }, [&state] (IDBValue value) {
            return toJS<IDLIDBValue>(state, *jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject()), value);
        }, [&state] (Vector<IDBValue> values) {
            return toJS<IDLSequence<IDLIDBValue>>(state, *jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject()), values);
        }, [] (uint64_t number) {
            return toJS<IDLUnsignedLongLong>(number);
        }, [] (IDBRequest::NullResultType other) {
            if (other == IDBRequest::NullResultType::Empty)
                return JSC::jsNull();
            return JSC::jsUndefined();
        });
    });
}

void JSIDBRequest::visitAdditionalChildren(SlotVisitor& visitor)
{
    auto& request = wrapped();
    request.resultWrapper().visit(visitor);
}

}
#endif // ENABLE(INDEXED_DATABASE)
