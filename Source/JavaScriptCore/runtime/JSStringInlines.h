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

ALWAYS_INLINE void JSString::destroy(JSCell* cell)
{
    auto* string = static_cast<JSString*>(cell);
    string->valueInternal().~String();
}

ALWAYS_INLINE void JSRopeString::destroy(JSCell* cell)
{
    auto* string = static_cast<JSRopeString*>(cell);
    if (string->isRope())
        return;
    string->valueInternal().~String();
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
template<typename CharacterType>
void JSRopeString::resolveToBufferSlow(JSString* fiber0, JSString* fiber1, JSString* fiber2, CharacterType* buffer, unsigned length, uint8_t*)
{
    // Keep in mind that resolveToBufferSlow signature must be the same to resolveToBuffer to encourage tail-calls by clang, that's the reason why
    // it takes the last stackLimit parameter still while it is not used here.

    CharacterType* position = buffer + length; // We will be working backwards over the rope.
    Vector<JSString*, 32, UnsafeVectorOverflow> workQueue; // These strings are kept alive by the parent rope, so using a Vector is OK.

    workQueue.append(fiber0);
    if (fiber1) {
        workQueue.append(fiber1);
        if (fiber2)
            workQueue.append(fiber2);
    }

    do {
        JSString* currentFiber = workQueue.takeLast();

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
    } while (!workQueue.isEmpty());

    ASSERT(buffer == position);
}

template<typename CharacterType>
inline void JSRopeString::resolveToBuffer(JSString* fiber0, JSString* fiber1, JSString* fiber2, CharacterType* buffer, unsigned length, uint8_t* stackLimit)
{
    ASSERT(fiber0);

    // We must ensure that all JSRopeString::resolveToBufferSlow and JSRopeString::resolveToBuffer calls must be done directly from this function, and it has
    // exact same signature to JSRopeString::resolveToBuffer, which will be esured by clang via MUST_TAIL_CALL attribute.
    // This allows clang to make these calls tail-calls, constructing significantly efficient rope resolution here.
    static_assert(3 == JSRopeString::s_maxInternalRopeLength);

    if (UNLIKELY(bitwise_cast<uint8_t*>(currentStackPointer()) < stackLimit))
        MUST_TAIL_CALL return JSRopeString::resolveToBufferSlow(fiber0, fiber1, fiber2, buffer, length, stackLimit);

    // 3 fibers.
    if (fiber2) {
        if (fiber0->isRope() || fiber1->isRope() || fiber2->isRope())
            MUST_TAIL_CALL return JSRopeString::resolveToBufferSlow(fiber0, fiber1, fiber2, buffer, length, stackLimit);

        StringView view0 = fiber0->valueInternal().impl();
        view0.getCharacters(buffer);
        StringView view1 = fiber1->valueInternal().impl();
        view1.getCharacters(buffer + view0.length());
        StringView view2 = fiber2->valueInternal().impl();
        view2.getCharacters(buffer + view0.length() + view1.length());
        return;
    }

    // 2 fibers.
    if (LIKELY(fiber1)) {
        if (fiber0->isRope()) {
            if (fiber1->isRope())
                MUST_TAIL_CALL return JSRopeString::resolveToBufferSlow(fiber0, fiber1, fiber2, buffer, length, stackLimit);

            auto* rope0 = static_cast<const JSRopeString*>(fiber0);
            {
                StringView view1 = fiber1->valueInternal().impl();
                view1.getCharacters(buffer + rope0->length());
            }
            if (rope0->isSubstring()) {
                StringView view0 = *rope0->substringBase()->valueInternal().impl();
                unsigned offset = rope0->substringOffset();
                unsigned length = rope0->length();
                view0.substring(offset, length).getCharacters(buffer);
                return;
            }
            MUST_TAIL_CALL return resolveToBuffer(rope0->fiber0(), rope0->fiber1(), rope0->fiber2(), buffer, rope0->length(), stackLimit);
        }

        if (fiber1->isRope()) {
            auto* rope1 = static_cast<const JSRopeString*>(fiber1);
            {
                StringView view0 = fiber0->valueInternal().impl();
                view0.getCharacters(buffer);
                buffer += view0.length();
            }
            if (rope1->isSubstring()) {
                StringView view1 = *rope1->substringBase()->valueInternal().impl();
                unsigned offset = rope1->substringOffset();
                unsigned length = rope1->length();
                view1.substring(offset, length).getCharacters(buffer);
                return;
            }
            MUST_TAIL_CALL return resolveToBuffer(rope1->fiber0(), rope1->fiber1(), rope1->fiber2(), buffer, rope1->length(), stackLimit);
        }

        StringView view0 = fiber0->valueInternal().impl();
        view0.getCharacters(buffer);
        StringView view1 = fiber1->valueInternal().impl();
        view1.getCharacters(buffer + view0.length());
        return;
    }

    // 1 fiber.
    if (!fiber0->isRope()) {
        StringView view0 = fiber0->valueInternal().impl();
        view0.getCharacters(buffer);
        return;
    }

    auto* rope0 = static_cast<const JSRopeString*>(fiber0);
    if (rope0->isSubstring()) {
        StringView view0 = *rope0->substringBase()->valueInternal().impl();
        unsigned offset = rope0->substringOffset();
        unsigned length = rope0->length();
        view0.substring(offset, length).getCharacters(buffer);
        return;
    }
    MUST_TAIL_CALL return resolveToBuffer(rope0->fiber0(), rope0->fiber1(), rope0->fiber2(), buffer, rope0->length(), stackLimit);
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
        size_t sizeToReport = impl->hasOneRef() ? impl->cost() : 0;
        ropeString->convertToNonRope(String { WTFMove(impl) });
        vm.heap.reportExtraMemoryAllocated(ropeString, sizeToReport);
        return ropeString;
    };

    AtomString atomString;
    if (!ropeString->isSubstring()) {
        uint8_t* stackLimit = bitwise_cast<uint8_t*>(vm.softStackLimit());
        JSString* fiber0 = ropeString->fiber0();
        JSString* fiber1 = ropeString->fiber1();
        JSString* fiber2 = ropeString->fiber2();
        if (ropeString->is8Bit()) {
            LChar characters[KeyAtomStringCache::maxStringLengthForCache];
            JSRopeString::resolveToBuffer(fiber0, fiber1, fiber2, characters, length, stackLimit);
            WTF::HashTranslatorCharBuffer<LChar> buffer { characters, length };
            return vm.keyAtomStringCache.make(vm, buffer, createFromRope);
        }
        UChar characters[KeyAtomStringCache::maxStringLengthForCache];
        JSRopeString::resolveToBuffer(fiber0, fiber1, fiber2, characters, length, stackLimit);
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

    // This is quite unfortunate, but duplicating this part here is the key of performance improvement in JetStream2/WSL,
    // which stress this jsAtomString significantly.
    auto resolveWith2Fibers = [&](JSString* fiber0, JSString* fiber1, auto* buffer, unsigned length) {
        uint8_t* stackLimit = bitwise_cast<uint8_t*>(vm.softStackLimit());
        if (fiber0->isRope()) {
            if (fiber1->isRope())
                return JSRopeString::resolveToBufferSlow(fiber0, fiber1, nullptr, buffer, length, stackLimit);

            auto* rope0 = static_cast<const JSRopeString*>(fiber0);
            StringView view1 = fiber1->valueInternal().impl();
            view1.getCharacters(buffer + rope0->length());
            if (rope0->isSubstring()) {
                StringView view0 = *rope0->substringBase()->valueInternal().impl();
                unsigned offset = rope0->substringOffset();
                unsigned length = rope0->length();
                view0.substring(offset, length).getCharacters(buffer);
                return;
            }
            return JSRopeString::resolveToBuffer(rope0->fiber0(), rope0->fiber1(), rope0->fiber2(), buffer, rope0->length(), stackLimit);
        }

        if (fiber1->isRope()) {
            StringView view0 = fiber0->valueInternal().impl();
            view0.getCharacters(buffer);
            auto* rope1 = static_cast<const JSRopeString*>(fiber1);
            if (rope1->isSubstring()) {
                StringView view1 = *rope1->substringBase()->valueInternal().impl();
                unsigned offset = rope1->substringOffset();
                unsigned length = rope1->length();
                view1.substring(offset, length).getCharacters(buffer + view0.length());
                return;
            }
            return JSRopeString::resolveToBuffer(rope1->fiber0(), rope1->fiber1(), rope1->fiber2(), buffer + view0.length(), rope1->length(), stackLimit);
        }

        StringView view0 = fiber0->valueInternal().impl();
        view0.getCharacters(buffer);
        StringView view1 = fiber1->valueInternal().impl();
        view1.getCharacters(buffer + view0.length());
    };

    if (s1->is8Bit() && s2->is8Bit()) {
        LChar characters[KeyAtomStringCache::maxStringLengthForCache];
        resolveWith2Fibers(s1, s2, characters, length);
        WTF::HashTranslatorCharBuffer<LChar> buffer { characters, length };
        return vm.keyAtomStringCache.make(vm, buffer, createFromFibers);
    }
    UChar characters[KeyAtomStringCache::maxStringLengthForCache];
    resolveWith2Fibers(s1, s2, characters, length);
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

    auto resolveWith3Fibers = [&](JSString* fiber0, JSString* fiber1, JSString* fiber2, auto* buffer, unsigned length) {
        if (fiber0->isRope() || fiber1->isRope() || fiber2->isRope())
            return JSRopeString::resolveToBufferSlow(fiber0, fiber1, fiber2, buffer, length, bitwise_cast<uint8_t*>(vm.softStackLimit()));

        StringView view0 = fiber0->valueInternal().impl();
        view0.getCharacters(buffer);
        StringView view1 = fiber1->valueInternal().impl();
        view1.getCharacters(buffer + view0.length());
        StringView view2 = fiber2->valueInternal().impl();
        view2.getCharacters(buffer + view0.length() + view1.length());
    };

    if (s1->is8Bit() && s2->is8Bit() && s3->is8Bit()) {
        LChar characters[KeyAtomStringCache::maxStringLengthForCache];
        resolveWith3Fibers(s1, s2, s3, characters, length);
        WTF::HashTranslatorCharBuffer<LChar> buffer { characters, length };
        return vm.keyAtomStringCache.make(vm, buffer, createFromFibers);
    }
    UChar characters[KeyAtomStringCache::maxStringLengthForCache];
    resolveWith3Fibers(s1, s2, s3, characters, length);
    WTF::HashTranslatorCharBuffer<UChar> buffer { characters, length };
    return vm.keyAtomStringCache.make(vm, buffer, createFromFibers);
}

inline JSString* jsSubstringOfResolved(VM& vm, GCDeferralContext* deferralContext, JSString* s, unsigned offset, unsigned length)
{
    ASSERT(offset <= s->length());
    ASSERT(length <= s->length());
    ASSERT(offset + length <= s->length());
    ASSERT(!s->isRope());
    if (!length)
        return vm.smallStrings.emptyString();
    if (!offset && length == s->length())
        return s;
    if (length == 1) {
        auto& base = s->valueInternal();
        if (auto c = base.characterAt(offset); c <= maxSingleCharacterString)
            return vm.smallStrings.singleCharacterString(c);
    } else if (length == 2) {
        auto& base = s->valueInternal();
        UChar first = base.characterAt(offset);
        UChar second = base.characterAt(offset + 1);
        if ((first | second) < 0x80) {
            auto createFromSubstring = [&](VM& vm, auto& buffer) {
                auto impl = AtomStringImpl::add(buffer);
                return JSString::create(vm, deferralContext, impl.releaseNonNull());
            };
            LChar buf[] = { static_cast<LChar>(first), static_cast<LChar>(second) };
            WTF::HashTranslatorCharBuffer<LChar> buffer { buf, length };
            return vm.keyAtomStringCache.make(vm, buffer, createFromSubstring);
        }
    }
    return JSRopeString::createSubstringOfResolved(vm, deferralContext, s, offset, length);
}

} // namespace JSC
