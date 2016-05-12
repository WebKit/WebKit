/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "JSIDBCursor.h"
#include "JSIDBDatabase.h"
#include "JSIDBIndex.h"
#include "JSIDBObjectStore.h"

using namespace JSC;

namespace WebCore {

JSValue JSIDBRequest::result(ExecState& state) const
{
    auto& request = wrapped();

    if (!request.isDone()) {
        ExceptionCodeWithMessage ec;
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to read the 'result' property from 'IDBRequest': The request has not finished.");
        setDOMException(&state, ec);
        return jsUndefined();
    }

    if (auto* cursor = request.cursorResult())
        return toJS(&state, globalObject(), *cursor);
    if (auto* database = request.databaseResult())
        return toJS(&state, globalObject(), *database);
    if (auto result = request.scriptResult())
        return result;
    return jsNull();
}

JSValue JSIDBRequest::source(ExecState& state) const
{
    auto& request = wrapped();
    if (auto* cursor = request.cursorSource())
        return toJS(&state, globalObject(), *cursor);
    if (auto* index = request.indexSource())
        return toJS(&state, globalObject(), *index);
    return toJS(&state, globalObject(), request.objectStoreSource());
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
