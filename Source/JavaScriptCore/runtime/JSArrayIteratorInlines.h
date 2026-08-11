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

#pragma once

#include <JavaScriptCore/JSArray.h>
#include <JavaScriptCore/JSArrayIterator.h>
#include <JavaScriptCore/JSObjectInlines.h>

namespace JSC {

ALWAYS_INLINE std::optional<uint32_t> JSArrayIterator::nextWithAdvance()
{
    auto* array = downcast<JSArray>(iteratedObject());
    ASSERT(isJSArray(array));

    int64_t index = this->index();
    ASSERT(index == doneIndex || (0 <= index && index <= maxSafeInteger()));
    if (index == doneIndex || index >= array->length()) {
        setIndex(doneIndex);
        return std::nullopt;
    }

    setIndex(index + 1);
    ASSERT(index == static_cast<uint32_t>(index));
    return static_cast<uint32_t>(index);
}

ALWAYS_INLINE bool JSArrayIterator::next(JSGlobalObject* globalObject, JSValue& value)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto index = nextWithAdvance();
    if (!index)
        return false;

    IterationKind kind = this->kind();
    if (kind == IterationKind::Keys) {
        value = jsNumber(*index);
        return true;
    }

    JSValue element = uncheckedDowncast<JSArray>(iteratedObject())->getIndex(globalObject, *index);
    RETURN_IF_EXCEPTION(scope, false);

    if (kind == IterationKind::Values) {
        value = element;
        return true;
    }

    value = constructArrayPair(globalObject, jsNumber(*index), element);
    RETURN_IF_EXCEPTION(scope, false);
    return true;
}

} // namespace JSC
