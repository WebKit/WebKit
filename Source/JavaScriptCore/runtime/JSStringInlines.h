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
#include "KeyAtomStringCacheInlines.h"

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

ALWAYS_INLINE bool JSString::equalInline(JSGlobalObject* globalObject, JSString* other) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length = this->length();
    if (length != other->length())
        return false;

    auto str1 = unsafeView(globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    auto str2 = other->unsafeView(globalObject);
    RETURN_IF_EXCEPTION(scope, false);

    ensureStillAliveHere(this);
    ensureStillAliveHere(other);
    return WTF::equal(str1, str2, length);
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

    RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
}

inline void JSRopeString::convertToNonRope(String&& string) const
{
    // Concurrent compiler threads can access String held by JSString. So we always emit
    // store-store barrier here to ensure concurrent compiler threads see initialized String.
    ASSERT(JSString::isRope());
    WTF::storeStoreFence();
    new (&uninitializedValueInternal()) String(WTFMove(string));
    static_assert(sizeof(String) == sizeof(RefPtr<StringImpl>), "JSString's String initialization must be done in one pointer move.");
    // We do not clear the trailing fibers and length information (fiber1 and fiber2) because we could be reading the length concurrently.
    ASSERT(!JSString::isRope());
}

class JSStringFibers {
public:
    JSStringFibers(JSString* s1, JSString* s2, JSString* s3)
        : m_fibers { s1, s2, s3 }
    {
    }

    JSString* fiber(unsigned index) const { return m_fibers[index]; }

private:
    std::array<JSString*, JSRopeString::s_maxInternalRopeLength> m_fibers { };
};

// Overview: These functions convert a JSString from holding a string in rope form
// down to a simple String representation. It does so by building up the string
// backwards, since we want to avoid recursion, we expect that the tree structure
// representing the rope is likely imbalanced with more nodes down the left side
// (since appending to the string is likely more common) - and as such resolving
// in this fashion should minimize work queue size.  (If we built the queue forwards
// we would likely have to place all of the constituent StringImpls into the
// Vector before performing any concatenation, but by working backwards we likely
// only fill the queue with the number of substrings at any given level in a
// rope-of-ropes.)
template<typename Fibers, typename CharacterType>
void JSRopeString::resolveToBufferSlow(Fibers* fibers, CharacterType* buffer, unsigned length)
{
    CharacterType* position = buffer + length; // We will be working backwards over the rope.
    Vector<JSString*, 32, UnsafeVectorOverflow> workQueue; // These strings are kept alive by the parent rope, so using a Vector is OK.

    for (size_t i = 0; i < JSRopeString::s_maxInternalRopeLength; ++i) {
        JSString* fiber = fibers->fiber(i);
        if (!fiber)
            break;
        workQueue.append(fiber);
    }

    while (!workQueue.isEmpty()) {
        JSString* currentFiber = workQueue.last();
        workQueue.removeLast();

        if (currentFiber->isRope()) {
            JSRopeString* currentFiberAsRope = static_cast<JSRopeString*>(currentFiber);
            if (currentFiberAsRope->isSubstring()) {
                ASSERT(!currentFiberAsRope->substringBase()->isRope());
                StringView view = *currentFiberAsRope->substringBase()->valueInternal().impl();
                unsigned offset = currentFiberAsRope->substringOffset();
                unsigned length = currentFiberAsRope->length();
                position -= length;
                view.substring(offset, length).getCharacters(position);
                continue;
            }
            for (size_t i = 0; i < JSRopeString::s_maxInternalRopeLength && currentFiberAsRope->fiber(i); ++i)
                workQueue.append(currentFiberAsRope->fiber(i));
            continue;
        }

        StringView view = *currentFiber->valueInternal().impl();
        position -= view.length();
        view.getCharacters(position);
    }

    ASSERT(buffer == position);
}

template<typename Fibers, typename CharacterType>
inline void JSRopeString::resolveToBuffer(Fibers* fibers, CharacterType* buffer, unsigned length)
{
    for (size_t i = 0; i < JSRopeString::s_maxInternalRopeLength; ++i) {
        JSString* fiber = fibers->fiber(i);
        if (!fiber)
            break;
        if (fiber->isRope()) {
            JSRopeString::resolveToBufferSlow(fibers, buffer, length);
            return;
        }
    }

    CharacterType* position = buffer;
    for (size_t i = 0; i < JSRopeString::s_maxInternalRopeLength; ++i) {
        JSString* fiber = fibers->fiber(i);
        if (!fiber)
            break;
        StringView view = fiber->valueInternal().impl();
        view.getCharacters(position);
        position += view.length();
    }
    ASSERT((buffer + length) == position);
}

inline JSString* jsAtomString(JSGlobalObject* globalObject, VM& vm, JSString* string)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length = string->length();
    if (length > KeyAtomStringCache::maxStringLengthForCache) {
        scope.release();
        string->toIdentifier(globalObject);
        return string;
    }

    if (!string->isRope()) {
        auto createFromNonRope = [&](VM& vm, auto&) {
            AtomString atom(string->valueInternal());
            if (!string->valueInternal().impl()->isAtom())
                string->swapToAtomString(vm, RefPtr { atom.impl() });
            return string;
        };

        if (string->valueInternal().is8Bit()) {
            WTF::HashTranslatorCharBuffer<LChar> buffer { string->valueInternal().characters8(), length, string->valueInternal().hash() };
            return vm.keyAtomStringCache.make(vm, buffer, createFromNonRope);
        }

        WTF::HashTranslatorCharBuffer<UChar> buffer { string->valueInternal().characters16(), length, string->valueInternal().hash() };
        return vm.keyAtomStringCache.make(vm, buffer, createFromNonRope);
    }

    JSRopeString* ropeString = jsCast<JSRopeString*>(string);

    auto createFromRope = [&](VM& vm, auto& buffer) {
        auto impl = AtomStringImpl::add(buffer);
        if (impl->hasOneRef())
            vm.heap.reportExtraMemoryAllocated(impl->cost());
        ropeString->convertToNonRope(String { WTFMove(impl) });
        return ropeString;
    };

    AtomString atomString;
    if (!ropeString->isSubstring()) {
        if (ropeString->is8Bit()) {
            LChar characters[KeyAtomStringCache::maxStringLengthForCache];
            JSRopeString::resolveToBuffer(ropeString, characters, length);
            WTF::HashTranslatorCharBuffer<LChar> buffer { characters, length };
            return vm.keyAtomStringCache.make(vm, buffer, createFromRope);
        }
        UChar characters[KeyAtomStringCache::maxStringLengthForCache];
        JSRopeString::resolveToBuffer(ropeString, characters, length);
        WTF::HashTranslatorCharBuffer<UChar> buffer { characters, length };
        return vm.keyAtomStringCache.make(vm, buffer, createFromRope);
    }

    auto view = StringView { ropeString->substringBase()->valueInternal() }.substring(ropeString->substringOffset(), length);
    if (view.is8Bit()) {
        WTF::HashTranslatorCharBuffer<LChar> buffer { view.characters8(), length };
        return vm.keyAtomStringCache.make(vm, buffer, createFromRope);
    }
    WTF::HashTranslatorCharBuffer<UChar> buffer { view.characters16(), length };
    return vm.keyAtomStringCache.make(vm, buffer, createFromRope);
}

inline JSString* jsAtomString(JSGlobalObject* globalObject, VM& vm, JSString* s1, JSString* s2)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length1 = s1->length();
    if (!length1)
        RELEASE_AND_RETURN(scope, jsAtomString(globalObject, vm, s2));
    unsigned length2 = s2->length();
    if (!length2)
        RELEASE_AND_RETURN(scope, jsAtomString(globalObject, vm, s1));
    static_assert(JSString::MaxLength == std::numeric_limits<int32_t>::max());
    if (sumOverflows<int32_t>(length1, length2)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    unsigned length = length1 + length2;
    if (length > KeyAtomStringCache::maxStringLengthForCache) {
        auto* ropeString = jsString(globalObject, s1, s2);
        RETURN_IF_EXCEPTION(scope, nullptr);
        ropeString->toIdentifier(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
        return ropeString;
    }

    auto createFromFibers = [&](VM& vm, auto& buffer) {
        return jsString(vm, String { AtomStringImpl::add(buffer) });
    };

    JSStringFibers fibers(s1, s2, nullptr);
    if (s1->is8Bit() && s2->is8Bit()) {
        LChar characters[KeyAtomStringCache::maxStringLengthForCache];
        JSRopeString::resolveToBuffer(&fibers, characters, length);
        WTF::HashTranslatorCharBuffer<LChar> buffer { characters, length };
        return vm.keyAtomStringCache.make(vm, buffer, createFromFibers);
    }
    UChar characters[KeyAtomStringCache::maxStringLengthForCache];
    JSRopeString::resolveToBuffer(&fibers, characters, length);
    WTF::HashTranslatorCharBuffer<UChar> buffer { characters, length };
    return vm.keyAtomStringCache.make(vm, buffer, createFromFibers);
}

inline JSString* jsAtomString(JSGlobalObject* globalObject, VM& vm, JSString* s1, JSString* s2, JSString* s3)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length1 = s1->length();
    if (!length1)
        RELEASE_AND_RETURN(scope, jsAtomString(globalObject, vm, s2, s3));

    unsigned length2 = s2->length();
    if (!length2)
        RELEASE_AND_RETURN(scope, jsAtomString(globalObject, vm, s1, s3));

    unsigned length3 = s3->length();
    if (!length3)
        RELEASE_AND_RETURN(scope, jsAtomString(globalObject, vm, s1, s2));

    static_assert(JSString::MaxLength == std::numeric_limits<int32_t>::max());
    if (sumOverflows<int32_t>(length1, length2, length3)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    unsigned length = length1 + length2 + length3;
    if (length > KeyAtomStringCache::maxStringLengthForCache) {
        auto* ropeString = jsString(globalObject, s1, s2, s3);
        RETURN_IF_EXCEPTION(scope, nullptr);
        ropeString->toIdentifier(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
        return ropeString;
    }

    auto createFromFibers = [&](VM& vm, auto& buffer) {
        return jsString(vm, String { AtomStringImpl::add(buffer) });
    };

    JSStringFibers fibers(s1, s2, s3);
    if (s1->is8Bit() && s2->is8Bit() && s3->is8Bit()) {
        LChar characters[KeyAtomStringCache::maxStringLengthForCache];
        JSRopeString::resolveToBuffer(&fibers, characters, length);
        WTF::HashTranslatorCharBuffer<LChar> buffer { characters, length };
        return vm.keyAtomStringCache.make(vm, buffer, createFromFibers);
    }
    UChar characters[KeyAtomStringCache::maxStringLengthForCache];
    JSRopeString::resolveToBuffer(&fibers, characters, length);
    WTF::HashTranslatorCharBuffer<UChar> buffer { characters, length };
    return vm.keyAtomStringCache.make(vm, buffer, createFromFibers);
}

} // namespace JSC
