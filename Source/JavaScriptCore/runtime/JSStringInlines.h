/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "JSGlobalObjectInlines.h"
#include "JSString.h"

namespace JSC {
    
inline JSString::~JSString()
{
    if (isRope())
        return;
    valueInternal().~String();
}

bool JSString::equal(JSGlobalObject* globalObject, JSString* other) const
{
    if (isRope() || other->isRope())
        return equalSlowCase(globalObject, other);
    return WTF::equal(*valueInternal().impl(), *other->valueInternal().impl());
}

template<typename StringType>
inline JSValue jsMakeNontrivialString(VM& vm, StringType&& string)
{
    return jsNontrivialString(vm, std::forward<StringType>(string));
}

template<typename StringType, typename... StringTypes>
inline JSValue jsMakeNontrivialString(JSGlobalObject* globalObject, StringType&& string, StringTypes&&... strings)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    String result = tryMakeString(std::forward<StringType>(string), std::forward<StringTypes>(strings)...);
    if (UNLIKELY(!result))
        return throwOutOfMemoryError(globalObject, scope);
    ASSERT(result.length() <= JSString::MaxLength);
    return jsNontrivialString(vm, WTFMove(result));
}

template <typename CharacterType>
inline JSString* repeatCharacter(JSGlobalObject* globalObject, CharacterType character, unsigned repeatCount)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    CharacterType* buffer = nullptr;
    auto impl = StringImpl::tryCreateUninitialized(repeatCount, buffer);
    if (!impl) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    std::fill_n(buffer, repeatCount, character);

    RELEASE_AND_RETURN(scope, jsString(vm, WTFMove(impl)));
}

} // namespace JSC
