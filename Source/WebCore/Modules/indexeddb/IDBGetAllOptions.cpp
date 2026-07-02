/*
* Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "IDBGetAllOptions.h"

#include "IDBKeyRange.h"
#include "IndexedDB.h"
#include "JSIDBGetAllOptions.h"
#include "JSIDBKeyRange.h"
#include "ScriptExecutionContext.h"
#include "Settings.h"
#include <JavaScriptCore/JSGlobalObject.h>

namespace WebCore {

// https://w3c.github.io/IndexedDB/#create-a-request-to-retrieve-multiple-items (Steps 8 and 9).
ExceptionOr<ParsedGetAllQueryOrOptions> parseQueryOrOptions(JSC::JSGlobalObject& execState, ScriptExecutionContext* context, JSC::JSValue queryOrOptions, std::optional<uint32_t> count)
{
    if (queryOrOptions.isUndefinedOrNull())
        return ParsedGetAllQueryOrOptions { IDBKeyRange::unbounded(), count, IndexedDB::CursorDirection::Next };

    if (IDBKeyRange::isPotentiallyValidKeyRange(execState, queryOrOptions)) {
        auto keyRangeOrException = IDBKeyRange::fromValue(execState, queryOrOptions);
        if (keyRangeOrException.hasException())
            return Exception(ExceptionCode::DataError, "The parameter is not a valid key range."_s);

        return ParsedGetAllQueryOrOptions { keyRangeOrException.releaseReturnValue(), count, IndexedDB::CursorDirection::Next };
    }

    if (!context || !context->settingsValues().indexedDBGetAllRecordsAndGetAllOptionsEnabled)
        return Exception(ExceptionCode::DataError, "The parameter is not a valid key range."_s);

    auto throwScope = DECLARE_THROW_SCOPE(execState.vm());
    auto optionsOrException = convertDictionary<IDBGetAllOptions>(execState, queryOrOptions);
    if (throwScope.exception())
        return Exception { ExceptionCode::DataError, "The parameter is not a valid options object."_s };

    return parseGetAllOptions(execState, optionsOrException.releaseReturnValue());
}

// https://w3c.github.io/IndexedDB/#create-a-request-to-retrieve-multiple-items (Step 9).
ExceptionOr<ParsedGetAllQueryOrOptions> parseGetAllOptions(JSC::JSGlobalObject& execState, IDBGetAllOptions options)
{
    auto keyRangeOrException = IDBKeyRange::fromValue(execState, options.query);
    if (keyRangeOrException.hasException())
        return Exception(ExceptionCode::DataError, "The query specified in options is not a valid key range."_s);

    return ParsedGetAllQueryOrOptions { keyRangeOrException.releaseReturnValue(), options.count, options.direction };
}

} // namespace WebCore
