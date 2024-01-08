/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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

#include "IDBBindingUtilities.h"
#include "JSDOMConvertIndexedDB.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertSequences.h"
#include "JSIDBCursor.h"
#include "JSIDBDatabase.h"

namespace WebCore {
using namespace JSC;

JSC::JSValue JSIDBRequest::result(JSC::JSGlobalObject& lexicalGlobalObject) const
{
    auto throwScope = DECLARE_THROW_SCOPE(lexicalGlobalObject.vm());
    auto result = wrapped().result();
    if (UNLIKELY(result.hasException())) {
        propagateException(lexicalGlobalObject, throwScope, result.releaseException());
        return jsNull();
    }

    auto resultValue = result.releaseReturnValue();
    auto& resultWrapper = wrapped().resultWrapper();
    return WTF::switchOn(resultValue, [] (const IDBRequest::NullResultType& result) {
        if (result == IDBRequest::NullResultType::Empty)
            return JSC::jsNull();
        return JSC::jsUndefined();
    }, [] (uint64_t number) {
        return toJS<IDLUnsignedLongLong>(number);
    }, [&] (const RefPtr<IDBCursor>& cursor) {
        return cachedPropertyValue(throwScope, lexicalGlobalObject, *this, resultWrapper, [&](JSC::ThrowScope& throwScope) {
            return toJS<IDLInterface<IDBCursor>>(lexicalGlobalObject, *jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject), throwScope, cursor.get());
        });
    }, [&] (const RefPtr<IDBDatabase>& database) {
        return cachedPropertyValue(throwScope, lexicalGlobalObject, *this, resultWrapper, [&](JSC::ThrowScope& throwScope) {
            return toJS<IDLInterface<IDBDatabase>>(lexicalGlobalObject, *jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject), throwScope, database.get());
        });
    }, [&] (const IDBKeyData& keyData) {
        return cachedPropertyValue(throwScope, lexicalGlobalObject, *this, resultWrapper, [&](JSC::ThrowScope&) {
            return toJS<IDLIDBKeyData>(lexicalGlobalObject, *jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject), keyData);
        });
    }, [&] (const Vector<IDBKeyData>& keyDatas) {
        return cachedPropertyValue(throwScope, lexicalGlobalObject, *this, resultWrapper, [&](JSC::ThrowScope&) {
            return toJS<IDLSequence<IDLIDBKeyData>>(lexicalGlobalObject, *jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject), keyDatas);
        });
    }, [&] (const IDBGetResult& getResult) {
        return cachedPropertyValue(throwScope, lexicalGlobalObject, *this, resultWrapper, [&](JSC::ThrowScope&) {
            auto result = deserializeIDBValueWithKeyInjection(lexicalGlobalObject, getResult.value(), getResult.keyData(), getResult.keyPath());
            return result ? result.value() : jsNull();
        });
    }, [&] (const IDBGetAllResult& getAllResult) {
        return cachedPropertyValue(throwScope, lexicalGlobalObject, *this, resultWrapper, [&](JSC::ThrowScope& throwScope) {
            auto& keys = getAllResult.keys();
            auto& values = getAllResult.values();
            auto& keyPath = getAllResult.keyPath();
            JSC::MarkedArgumentBuffer list;
            list.ensureCapacity(values.size());
            for (unsigned i = 0; i < values.size(); i ++) {
                auto result = deserializeIDBValueWithKeyInjection(lexicalGlobalObject, values[i], keys[i], keyPath);
                if (!result)
                    return jsNull();
                list.append(result.value());
                if (UNLIKELY(list.hasOverflowed())) {
                    propagateException(lexicalGlobalObject, throwScope, Exception(ExceptionCode::UnknownError));
                    return jsNull();
                }
            }
            return JSValue(JSC::constructArray(&lexicalGlobalObject, static_cast<JSC::ArrayAllocationProfile*>(nullptr), list));
        });
    });
}

template<typename Visitor>
void JSIDBRequest::visitAdditionalChildren(Visitor& visitor)
{
    auto& request = wrapped();
    request.resultWrapper().visit(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSIDBRequest);

} // namespace WebCore
