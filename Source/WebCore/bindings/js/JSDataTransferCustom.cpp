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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSDataTransfer.h"

#include "DataTransfer.h"
#include "Document.h"
#include "IDLTypes.h"
#include "JSDOMBinding.h"
#include "JSDOMConvertSequences.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMGlobalObject.h"
#include <JavaScriptCore/JSCInlines.h>

namespace WebCore {
using namespace JSC;

// https://html.spec.whatwg.org/multipage/dnd.html#dom-datatransfer-types
// The types attribute must return the same FrozenArray object each time it is accessed,
// as long as the data store item list has not changed since the last time the attribute was accessed.
JSValue JSDataTransfer::types(JSGlobalObject& lexicalGlobalObject) const
{
    DataTransfer& impl = wrapped();

    if (impl.typesCacheIsValid()) {
        if (JSValue cachedValue = m_types.get())
            return cachedValue;
    }

    auto throwScope = DECLARE_THROW_SCOPE(lexicalGlobalObject.vm());

    RefPtr context = jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject)->scriptExecutionContext();
    if (!context) [[unlikely]]
        return jsUndefined();
    Ref document = downcast<Document>(*context);

    JSValue result = toJS<IDLFrozenArray<IDLDOMString>>(lexicalGlobalObject, *globalObject(), throwScope, impl.types(document.get()));
    RETURN_IF_EXCEPTION(throwScope, { });

    m_types.set(JSC::getVM(&lexicalGlobalObject), this, result);
    impl.didCacheTypes();
    return result;
}

} // namespace WebCore
