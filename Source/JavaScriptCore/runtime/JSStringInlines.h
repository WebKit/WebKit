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

#include <JavaScriptCore/ExceptionHelpers.h>
#include <JavaScriptCore/GetVM.h>
#include <JavaScriptCore/HeapCellInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/KeyAtomStringCacheInlines.h>
#include <JavaScriptCore/MarkedBlockInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/ParsingUtilities.h>

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

    auto str1 = view(globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    auto str2 = other->view(globalObject);
    RETURN_IF_EXCEPTION(scope, false);

    ensureStillAliveHere(this);
    ensureStillAliveHere(other);
    return WTF::equal(str1, str2, length);
}

JSString* JSString::tryReplaceOneCharImpl(JSGlobalObject* globalObject, char16_t search, JSString* replacement, uint8_t* stackLimit, bool& found)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (std::bit_cast<uint8_t*>(currentStackPointer()) < stackLimit) [[unlikely]]
        return nullptr; // Stack overflow

    if (this->isNonSubstringRope()) {
        JSRopeString* rope = static_cast<JSRopeString*>(this);
        JSString* oldFiber0 = rope->fiber0();
        JSString* oldFiber1 = rope->fiber1();
        JSString* oldFiber2 = rope->fiber2();

        ASSERT(oldFiber0);
        JSString* newFiber0 = oldFiber0->tryReplaceOneCharImpl(globalObject, search, replacement, stackLimit, found);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!newFiber0) [[unlikely]]
            return nullptr;
        if (found)
            RELEASE_AND_RETURN(scope, jsString(globalObject, newFiber0, oldFiber1, oldFiber2));

        if (oldFiber1) {
            JSString* newFiber1 = oldFiber1->tryReplaceOneCharImpl(globalObject, search, replacement, stackLimit, found);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!newFiber1) [[unlikely]]
                return nullptr;
            if (found)
                RELEASE_AND_RETURN(scope, jsString(globalObject, oldFiber0, newFiber1, oldFiber2));
        }

        if (oldFiber2) {
            JSString* newFiber2 = oldFiber2->tryReplaceOneCharImpl(globalObject, search, replacement, stackLimit, found);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!newFiber2) [[unlikely]]
                return nullptr;
            if (found)
                RELEASE_AND_RETURN(scope, jsString(globalObject, oldFiber0, oldFiber1, newFiber2));
        }

        return this; // Not found.
    }

    auto thisView = this->view(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    size_t index = thisView->find(search);
    if (index == WTF::notFound)
        return this; // Not found.
    found = true;

    // Case 1: The matched character is the only character in the string.
    unsigned length = thisView->length();
    if (length == 1)
        return replacement;

    // Case 2: The matched character is the last character in the string.
    JSString* left = nullptr;
    if (index) {
        left = jsSubstring(globalObject, this, 0, index);
        RETURN_IF_EXCEPTION(scope, nullptr);
        // There is a match at this point, then length must be larger than zero.
        if (index == length - 1)
            RELEASE_AND_RETURN(scope, jsString(globalObject, left, replacement));
    }

    // Case 3: The matched character is the first character in the string.
    size_t rightStart = index + 1; // At this point, the index must be less than length - 1.
    JSString* right = jsSubstring(globalObject, this, rightStart, length - rightStart);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!index)
        RELEASE_AND_RETURN(scope, jsString(globalObject, replacement, right));

    // Case 4: The matched character is in the middle of the string.
    RELEASE_AND_RETURN(scope, jsString(globalObject, left, replacement, right));
}

JSString* JSString::tryReplaceOneChar(JSGlobalObject* globalObject, char16_t search, JSString* replacement)
{
    uint8_t* stackLimit = std::bit_cast<uint8_t*>(globalObject->vm().softStackLimit());
    bool found = false;
    if (JSString* result = tryReplaceOneCharImpl(globalObject, search, replacement, stackLimit, found); result && found)
        return result;
    return nullptr;
}

std::optional<size_t> JSString::tryFindOneChar(JSGlobalObject*, char16_t character, unsigned& startPosition) const
{
    ASSERT(isRope());

    // Search for a single character in a rope without resolving it.
    // If the root is a substring rope, scan it directly via its base's buffer.
    // If the root is a non-substring rope, iterate its top-level fibers:
    //   - Resolved string or substring rope: scan via StringView::find().
    //   - Non-substring rope fiber: bail out (return nullopt).
    // Returns position if found, WTF::notFound if definitively
    // absent, or std::nullopt if the rope structure is too complex to walk.

    auto scanSubstring = [&](const JSRopeString* substringRope, unsigned fiberLength, unsigned offset) -> std::optional<size_t> {
        JSString* base = substringRope->substringBase();
        ASSERT(!base->isRope());
        unsigned localStart = startPosition > offset ? startPosition - offset : 0;
        StringView view = StringView(base->valueInternal()).substring(substringRope->substringOffset(), fiberLength);
        size_t result = view.find(character, localStart);
        if (result != WTF::notFound)
            return offset + result;
        return std::nullopt;
    };

    if (isSubstring()) {
        auto result = scanSubstring(static_cast<const JSRopeString*>(this), length(), 0);
        return result ? result : std::optional<size_t>(WTF::notFound);
    }

    const JSRopeString* rope = static_cast<const JSRopeString*>(this);
    unsigned offset = 0;
    for (unsigned i = 0; i < JSRopeString::s_maxInternalRopeLength; ++i) {
        JSString* fiber = rope->fiber(i);
        if (!fiber)
            break;

        unsigned fiberLength = fiber->length();
        if (startPosition >= offset + fiberLength) {
            offset += fiberLength;
            continue;
        }

        if (!fiber->isRope()) {
            unsigned localStart = startPosition > offset ? startPosition - offset : 0;
            size_t result = StringView(fiber->valueInternal()).find(character, localStart);
            if (result != WTF::notFound)
                return offset + result;
        } else if (fiber->isSubstring()) {
            if (auto result = scanSubstring(static_cast<const JSRopeString*>(fiber), fiberLength, offset))
                return result;
        } else {
            startPosition = std::max(startPosition, offset);
            return std::nullopt;
        }

        offset += fiberLength;
    }

    return WTF::notFound;
}

std::optional<size_t> JSString::tryFindLastOneChar(JSGlobalObject*, char16_t character, unsigned& startPosition) const
{
    ASSERT(isRope());

    // Reverse counterpart of tryFindOneChar: walk the rope structure without resolving it
    // and return the last occurrence of `character` at or before `startPosition` (inclusive).
    // nullopt means the rope structure is too complex to walk; in that case `startPosition`
    // is updated to mark the latest position known not to contain the character (so the
    // caller can resolve only the remaining prefix).

    auto scanSubstring = [&](const JSRopeString* substringRope, unsigned fiberLength, unsigned offset) -> std::optional<size_t> {
        JSString* base = substringRope->substringBase();
        ASSERT(!base->isRope());
        ASSERT(startPosition >= offset);
        if (!fiberLength)
            return std::nullopt;
        unsigned localStart = startPosition >= offset + fiberLength ? fiberLength - 1 : startPosition - offset;
        StringView view = StringView(base->valueInternal()).substring(substringRope->substringOffset(), fiberLength);
        size_t result = view.reverseFind(character, localStart);
        if (result != WTF::notFound)
            return offset + result;
        return std::nullopt;
    };

    if (isSubstring()) {
        auto result = scanSubstring(static_cast<const JSRopeString*>(this), length(), 0);
        return result ? result : std::optional<size_t>(WTF::notFound);
    }

    const JSRopeString* rope = static_cast<const JSRopeString*>(this);
    unsigned fiberLengths[JSRopeString::s_maxInternalRopeLength] { };
    JSString* fibers[JSRopeString::s_maxInternalRopeLength] { };
    unsigned fiberCount = 0;
    for (unsigned i = 0; i < JSRopeString::s_maxInternalRopeLength; ++i) {
        JSString* fiber = rope->fiber(i);
        if (!fiber)
            break;
        fibers[fiberCount] = fiber;
        fiberLengths[fiberCount] = fiber->length();
        ++fiberCount;
    }

    unsigned totalLength = length();

    // Walk fibers in reverse order so we return the largest matching index.
    unsigned offset = totalLength;
    for (unsigned i = fiberCount; i-- > 0;) {
        unsigned fiberLength = fiberLengths[i];
        offset -= fiberLength;

        if (!fiberLength)
            continue;
        if (offset > startPosition)
            continue; // fiber is entirely past `startPosition`.

        JSString* fiber = fibers[i];
        if (!fiber->isRope()) {
            unsigned localStart = startPosition >= offset + fiberLength ? fiberLength - 1 : startPosition - offset;
            size_t result = StringView(fiber->valueInternal()).reverseFind(character, localStart);
            if (result != WTF::notFound)
                return offset + result;
        } else if (fiber->isSubstring()) {
            if (auto result = scanSubstring(static_cast<const JSRopeString*>(fiber), fiberLength, offset))
                return result;
        } else {
            // Bail out but update startPosition so the caller only needs to scan
            // [0, offset + fiberLength - 1].
            startPosition = std::min(startPosition, offset + fiberLength - 1);
            return std::nullopt;
        }
    }

    return WTF::notFound;
}

ALWAYS_INLINE std::optional<char16_t> JSString::tryGetCharAt(JSGlobalObject*, unsigned index) const
{
    ASSERT(isRope());
    ASSERT(index < length());

    if (isSubstring()) {
        const JSRopeString* substringRope = static_cast<const JSRopeString*>(this);
        return StringView(substringRope->substringBase()->valueInternal())[substringRope->substringOffset() + index];
    }

    const JSRopeString* rope = static_cast<const JSRopeString*>(this);
    unsigned offset = 0;
    for (unsigned i = 0; i < JSRopeString::s_maxInternalRopeLength; ++i) {
        JSString* fiber = rope->fiber(i);
        ASSERT(fiber);
        unsigned fiberLength = fiber->length();
        if (index >= offset + fiberLength) {
            offset += fiberLength;
            continue;
        }

        unsigned localIndex = index - offset;
        if (!fiber->isRope())
            return StringView(fiber->valueInternal())[localIndex];
        if (fiber->isSubstring()) {
            const JSRopeString* substringFiber = static_cast<const JSRopeString*>(fiber);
            return StringView(substringFiber->substringBase()->valueInternal())[substringFiber->substringOffset() + localIndex];
        }
        return std::nullopt;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return std::nullopt;
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
    if (!result) [[unlikely]]
        return throwOutOfMemoryError(globalObject, scope);
    ASSERT(result.length() <= JSString::MaxLength);
    return jsNontrivialString(vm, WTF::move(result));
}

template <typename CharacterType>
    requires (std::same_as<CharacterType, Latin1Character> || std::same_as<CharacterType, char16_t>)
inline JSString* repeatCharacter(JSGlobalObject* globalObject, CharacterType character, unsigned repeatCount)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!repeatCount)
        return jsEmptyString(vm);

    std::span<CharacterType> buffer;
    auto impl = StringImpl::tryCreateUninitialized(repeatCount, buffer);
    if (!impl) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    *buffer.data() = character;
    unsigned copied = 1;
    while (copied < repeatCount) {
        unsigned copyLen = std::min(copied, repeatCount - copied);
        memcpySpan(buffer.subspan(copied, copyLen), buffer.subspan(0, copyLen));
        copied += copyLen;
    }

    RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
}

inline void JSRopeString::convertToNonRope(String&& string) const
{
    // Concurrent compiler threads can access String held by JSString. So we always emit
    // store-store barrier here to ensure concurrent compiler threads see initialized String.
    ASSERT(JSString::isRope());
    WTF::storeStoreFence();
    new (&uninitializedValueInternal()) String(WTF::move(string));
    static_assert(sizeof(String) == sizeof(RefPtr<StringImpl>), "JSString's String initialization must be done in one pointer move.");
    // We do not clear the trailing fibers and length information (fiber1 and fiber2) because we could be reading the length concurrently.
    ASSERT(!JSString::isRope());
    notifyNeedsDestruction();
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
NEVER_INLINE void JSRopeString::resolveToBufferSlow(JSString* fiber0, JSString* fiber1, JSString* fiber2, std::span<CharacterType> buffer, uint8_t*)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    // Keep in mind that resolveToBufferSlow signature must be the same to resolveToBuffer to encourage tail-calls by clang, that's the reason why
    // it takes the last stackLimit parameter still while it is not used here.

    CharacterType* end = std::to_address(buffer.end()); // We will be working backwards over the rope.
    CharacterType* position = end; // We will be working backwards over the rope.
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
                view.substring(offset, length).getCharacters(unsafeMakeSpan(position, end - position));
                continue;
            }
            for (size_t i = 0; i < JSRopeString::s_maxInternalRopeLength && currentFiberAsRope->fiber(i); ++i)
                workQueue.append(currentFiberAsRope->fiber(i));
            continue;
        }

        StringView view = *currentFiber->valueInternal().impl();
        position -= view.length();
        view.getCharacters(unsafeMakeSpan(position, end - position));
    } while (!workQueue.isEmpty());

    ASSERT(buffer.data() == position);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

template<typename CharacterType>
inline void JSRopeString::resolveToBuffer(JSString* fiber0, JSString* fiber1, JSString* fiber2, std::span<CharacterType> buffer, uint8_t* stackLimit)
{
#if HAVE(MUST_TAIL_CALL)
    ASSERT(fiber0);

    // We must ensure that all JSRopeString::resolveToBufferSlow and JSRopeString::resolveToBuffer calls must be done directly from this function, and it has
    // exact same signature to JSRopeString::resolveToBuffer, which will be esured by clang via MUST_TAIL_CALL attribute.
    // This allows clang to make these calls tail-calls, constructing significantly efficient rope resolution here.
    static_assert(3 == JSRopeString::s_maxInternalRopeLength);

    // 3 fibers.
    if (fiber2) {
        if (fiber0->isRope()) {
            auto* rope0 = static_cast<const JSRopeString*>(fiber0);
            auto rope0Length = rope0->length();
            if (rope0->isSubstring()) {
                StringView view0 = *rope0->substringBase()->valueInternal().impl();
                unsigned offset = rope0->substringOffset();
                view0.substring(offset, rope0Length).getCharacters(buffer);
            } else {
                if (std::bit_cast<uint8_t*>(currentStackPointer()) < stackLimit) [[unlikely]]
                    MUST_TAIL_CALL return JSRopeString::resolveToBufferSlow(fiber0, fiber1, fiber2, buffer, stackLimit);
                resolveToBuffer(rope0->fiber0(), rope0->fiber1(), rope0->fiber2(), buffer.first(rope0Length), stackLimit);
            }
            skip(buffer, rope0Length);
        } else {
            StringView view0 = fiber0->valueInternal().impl();
            view0.getCharacters(buffer);
            skip(buffer, view0.length());
        }
        fiber0 = fiber1;
        fiber1 = fiber2;
        fiber2 = nullptr;
        // Fall through to the 2 fibers case.
    }

    // 2 fibers.
    if (fiber1) [[likely]] {
        if (fiber0->isRope()) {
            if (fiber1->isRope()) {
                if (std::bit_cast<uint8_t*>(currentStackPointer()) < stackLimit) [[unlikely]]
                    MUST_TAIL_CALL return JSRopeString::resolveToBufferSlow(fiber0, fiber1, fiber2, buffer, stackLimit);

                auto* rope0 = static_cast<const JSRopeString*>(fiber0);
                auto rope0Length = rope0->length();

                auto* rope1 = static_cast<const JSRopeString*>(fiber1);
                auto rope1Length = rope1->length();

                auto rope0Buffer = buffer.first(rope0Length);
                auto rope1Buffer = buffer.subspan(rope0Length);

                bool rope0Resolved = false;
                if (rope0->isSubstring()) {
                    {
                        StringView view = *rope0->substringBase()->valueInternal().impl();
                        unsigned offset = rope0->substringOffset();
                        view.substring(offset, rope0Length).getCharacters(rope0Buffer);
                    }
                    if (rope0 == rope1) {
                        memcpySpan(rope1Buffer, rope0Buffer);
                        return;
                    }
                    rope0Resolved = true;
                }

                if (rope1->isSubstring()) {
                    {
                        StringView view = *rope1->substringBase()->valueInternal().impl();
                        unsigned offset = rope1->substringOffset();
                        view.substring(offset, rope1Length).getCharacters(rope1Buffer);
                    }
                    if (rope0Resolved)
                        return;
                    MUST_TAIL_CALL return resolveToBuffer(rope0->fiber0(), rope0->fiber1(), rope0->fiber2(), rope0Buffer, stackLimit);
                }

                if (rope0Resolved)
                    MUST_TAIL_CALL return resolveToBuffer(rope1->fiber0(), rope1->fiber1(), rope1->fiber2(), rope1Buffer, stackLimit);

                // We resolve short rope first. Our heuristic is that longer rope can potentially have deeper nestings.
                // Thus we would like to resolve that nestings in a tail-call form to avoid deeply nested call stacks.
                const JSRopeString* shortRope;
                const JSRopeString* longRope;
                std::span<CharacterType> shortBuffer;
                std::span<CharacterType> longBuffer;

                if (rope0Length < rope1Length) {
                    shortRope = rope0;
                    longRope = rope1;
                    shortBuffer = rope0Buffer;
                    longBuffer = rope1Buffer;
                } else {
                    shortRope = rope1;
                    longRope = rope0;
                    shortBuffer = rope1Buffer;
                    longBuffer = rope0Buffer;
                }

                resolveToBuffer(shortRope->fiber0(), shortRope->fiber1(), shortRope->fiber2(), shortBuffer, stackLimit);
                if (rope0 == rope1) {
                    memcpySpan(longBuffer, shortBuffer);
                    return;
                }
                MUST_TAIL_CALL return resolveToBuffer(longRope->fiber0(), longRope->fiber1(), longRope->fiber2(), longBuffer, stackLimit);
            }

            auto* rope0 = static_cast<const JSRopeString*>(fiber0);
            auto rope0Length = rope0->length();
            {
                StringView view1 = fiber1->valueInternal().impl();
                view1.getCharacters(buffer.subspan(rope0Length));
            }
            if (rope0->isSubstring()) {
                StringView view0 = *rope0->substringBase()->valueInternal().impl();
                unsigned offset = rope0->substringOffset();

                view0.substring(offset, rope0Length).getCharacters(buffer);
                return;
            }
            MUST_TAIL_CALL return resolveToBuffer(rope0->fiber0(), rope0->fiber1(), rope0->fiber2(), buffer.first(rope0Length), stackLimit);
        }

        if (fiber1->isRope()) {
            auto* rope1 = static_cast<const JSRopeString*>(fiber1);
            auto rope1Length = rope1->length();
            {
                StringView view0 = fiber0->valueInternal().impl();
                view0.getCharacters(buffer);
                skip(buffer, view0.length());
            }
            if (rope1->isSubstring()) {
                StringView view1 = *rope1->substringBase()->valueInternal().impl();
                unsigned offset = rope1->substringOffset();
                view1.substring(offset, rope1Length).getCharacters(buffer);
                return;
            }
            MUST_TAIL_CALL return resolveToBuffer(rope1->fiber0(), rope1->fiber1(), rope1->fiber2(), buffer.first(rope1Length), stackLimit);
        }

        StringView view0 = fiber0->valueInternal().impl();
        view0.getCharacters(buffer);
        StringView view1 = fiber1->valueInternal().impl();
        view1.getCharacters(buffer.subspan(view0.length()));
        return;
    }

    // 1 fiber.
    if (!fiber0->isRope()) {
        StringView view0 = fiber0->valueInternal().impl();
        view0.getCharacters(buffer);
        return;
    }

    auto* rope0 = static_cast<const JSRopeString*>(fiber0);
    auto rope0Length = rope0->length();
    if (rope0->isSubstring()) {
        StringView view0 = *rope0->substringBase()->valueInternal().impl();
        unsigned offset = rope0->substringOffset();
        view0.substring(offset, rope0Length).getCharacters(buffer);
        return;
    }
    MUST_TAIL_CALL return resolveToBuffer(rope0->fiber0(), rope0->fiber1(), rope0->fiber2(), buffer.first(rope0Length), stackLimit);
#else
    return JSRopeString::resolveToBufferSlow(fiber0, fiber1, fiber2, buffer, stackLimit);
#endif
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
            WTF::HashTranslatorCharBuffer<Latin1Character> buffer { string->valueInternal().span8(), string->valueInternal().hash() };
            return vm.keyAtomStringCache.make(vm, buffer, createFromNonRope);
        }

        WTF::HashTranslatorCharBuffer<char16_t> buffer { string->valueInternal().span16(), string->valueInternal().hash() };
        return vm.keyAtomStringCache.make(vm, buffer, createFromNonRope);
    }

    JSRopeString* ropeString = uncheckedDowncast<JSRopeString>(string);

    auto createFromRope = [&](VM& vm, auto& buffer) {
        auto impl = AtomStringImpl::add(buffer);
        size_t sizeToReport = impl->hasOneRef() ? impl->cost() : 0;
        ropeString->convertToNonRope(String { WTF::move(impl) });
        vm.heap.reportExtraMemoryAllocated(ropeString, sizeToReport);
        return ropeString;
    };

    if (!ropeString->isSubstring()) {
        uint8_t* stackLimit = std::bit_cast<uint8_t*>(vm.softStackLimit());
        JSString* fiber0 = ropeString->fiber0();
        JSString* fiber1 = ropeString->fiber1();
        JSString* fiber2 = ropeString->fiber2();
        if (ropeString->is8Bit()) {
            std::array<Latin1Character, KeyAtomStringCache::maxStringLengthForCache> characters;
            JSRopeString::resolveToBuffer(fiber0, fiber1, fiber2, std::span { characters }.first(length), stackLimit);
            WTF::HashTranslatorCharBuffer<Latin1Character> buffer { std::span { characters }.first(length) };
            return vm.keyAtomStringCache.make(vm, buffer, createFromRope);
        }
        std::array<char16_t, KeyAtomStringCache::maxStringLengthForCache> characters;
        JSRopeString::resolveToBuffer(fiber0, fiber1, fiber2, std::span { characters }.first(length), stackLimit);
        WTF::HashTranslatorCharBuffer<char16_t> buffer { std::span { characters }.first(length) };
        return vm.keyAtomStringCache.make(vm, buffer, createFromRope);
    }

    auto view = StringView { ropeString->substringBase()->valueInternal() }.substring(ropeString->substringOffset(), length);
    if (view.is8Bit()) {
        WTF::HashTranslatorCharBuffer<Latin1Character> buffer { view.span8() };
        return vm.keyAtomStringCache.make(vm, buffer, createFromRope);
    }
    WTF::HashTranslatorCharBuffer<char16_t> buffer { view.span16() };
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
    auto resolveWith2Fibers = [&](JSString* fiber0, JSString* fiber1, auto buffer) {
        uint8_t* stackLimit = std::bit_cast<uint8_t*>(vm.softStackLimit());
        if (fiber0->isRope()) {
            if (fiber1->isRope())
                return JSRopeString::resolveToBufferSlow(fiber0, fiber1, nullptr, buffer, stackLimit);

            auto* rope0 = static_cast<const JSRopeString*>(fiber0);
            StringView view1 = fiber1->valueInternal().impl();
            view1.getCharacters(buffer.subspan(rope0->length()));
            if (rope0->isSubstring()) {
                StringView view0 = *rope0->substringBase()->valueInternal().impl();
                unsigned offset = rope0->substringOffset();
                view0.substring(offset, rope0->length()).getCharacters(buffer);
                return;
            }
            return JSRopeString::resolveToBuffer(rope0->fiber0(), rope0->fiber1(), rope0->fiber2(), buffer.first(rope0->length()), stackLimit);
        }

        if (fiber1->isRope()) {
            StringView view0 = fiber0->valueInternal().impl();
            view0.getCharacters(buffer);
            auto* rope1 = static_cast<const JSRopeString*>(fiber1);
            if (rope1->isSubstring()) {
                StringView view1 = *rope1->substringBase()->valueInternal().impl();
                unsigned offset = rope1->substringOffset();
                view1.substring(offset, rope1->length()).getCharacters(buffer.subspan(view0.length()));
                return;
            }
            return JSRopeString::resolveToBuffer(rope1->fiber0(), rope1->fiber1(), rope1->fiber2(), buffer.subspan(view0.length(), rope1->length()), stackLimit);
        }

        StringView view0 = fiber0->valueInternal().impl();
        view0.getCharacters(buffer);
        StringView view1 = fiber1->valueInternal().impl();
        view1.getCharacters(buffer.subspan(view0.length()));
    };

    if (s1->is8Bit() && s2->is8Bit()) {
        Latin1Character characters[KeyAtomStringCache::maxStringLengthForCache];
        resolveWith2Fibers(s1, s2, std::span { characters }.first(length));
        WTF::HashTranslatorCharBuffer<Latin1Character> buffer { std::span(characters).first(length) };
        return vm.keyAtomStringCache.make(vm, buffer, createFromFibers);
    }
    char16_t characters[KeyAtomStringCache::maxStringLengthForCache];
    resolveWith2Fibers(s1, s2, std::span(characters).first(length));
    WTF::HashTranslatorCharBuffer<char16_t> buffer { std::span(characters).first(length) };
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

    auto resolveWith3Fibers = [&](JSString* fiber0, JSString* fiber1, JSString* fiber2, auto buffer) {
        if (fiber0->isRope() || fiber1->isRope() || fiber2->isRope())
            return JSRopeString::resolveToBufferSlow(fiber0, fiber1, fiber2, buffer, std::bit_cast<uint8_t*>(vm.softStackLimit()));

        StringView view0 = fiber0->valueInternal().impl();
        view0.getCharacters(buffer);
        StringView view1 = fiber1->valueInternal().impl();
        view1.getCharacters(buffer.subspan(view0.length()));
        StringView view2 = fiber2->valueInternal().impl();
        view2.getCharacters(buffer.subspan(view0.length() + view1.length()));
    };

    if (s1->is8Bit() && s2->is8Bit() && s3->is8Bit()) {
        Latin1Character characters[KeyAtomStringCache::maxStringLengthForCache];
        resolveWith3Fibers(s1, s2, s3, std::span { characters }.first(length));
        WTF::HashTranslatorCharBuffer<Latin1Character> buffer { std::span { characters }.first(length) };
        return vm.keyAtomStringCache.make(vm, buffer, createFromFibers);
    }
    char16_t characters[KeyAtomStringCache::maxStringLengthForCache];
    resolveWith3Fibers(s1, s2, s3, std::span { characters }.first(length));
    WTF::HashTranslatorCharBuffer<char16_t> buffer { std::span { characters }.first(length) };
    return vm.keyAtomStringCache.make(vm, buffer, createFromFibers);
}

inline JSString* jsSubstringOfResolved(VM& vm, GCDeferralContext* deferralContext, JSString* s, unsigned offset, unsigned length)
{
    ASSERT(offset <= s->length());
    ASSERT(length <= s->length());
    ASSERT(offset + length <= s->length());

    if (!length)
        return vm.smallStrings.emptyString();

    if (s->isSubstring()) {
        JSRopeString* baseRope = uncheckedDowncast<JSRopeString>(s);
        ASSERT(!baseRope->substringBase()->isRope());
        s = baseRope->substringBase();
        offset += baseRope->substringOffset();
    }

    ASSERT(!s->isRope());
    auto& base = s->valueInternal();
    if (!offset && length == base.length())
        return s;

    if (length == 1) {
        if (auto c = base.codeUnitAt(offset); c <= maxSingleCharacterString)
            return vm.smallStrings.singleCharacterString(c);
    } else if (length == 2) {
        char16_t first = base.codeUnitAt(offset);
        char16_t second = base.codeUnitAt(offset + 1);
        if ((first | second) < 0x80) {
            auto createFromSubstring = [&](VM& vm, auto& buffer) {
                auto impl = AtomStringImpl::add(buffer);
                return JSString::create(vm, deferralContext, impl.releaseNonNull());
            };
            Latin1Character buf[] = { static_cast<Latin1Character>(first), static_cast<Latin1Character>(second) };
            WTF::HashTranslatorCharBuffer<Latin1Character> buffer { unsafeMakeSpan(buf, length) };
            return vm.keyAtomStringCache.make(vm, buffer, createFromSubstring);
        }
    }
    return JSRopeString::createSubstringOfResolved(vm, deferralContext, s, offset, length, base.is8Bit());
}

template<typename CharacterType>
void JSString::resolveToBuffer(std::span<CharacterType> destination)
{
    if (isRope()) {
        auto* rope = uncheckedDowncast<JSRopeString>(this);
        if (rope->isSubstring()) {
            StringView view = *rope->substringBase()->valueInternal().impl();
            unsigned offset = rope->substringOffset();
            view.substring(offset, rope->length()).getCharacters(destination);
            return;
        }

        uint8_t* stackLimit = std::bit_cast<uint8_t*>(vm().softStackLimit());
        return JSRopeString::resolveToBuffer(rope->fiber0(), rope->fiber1(), rope->fiber2(), destination, stackLimit);
    }
    StringView(valueInternal().impl()).getCharacters(destination);
}

} // namespace JSC
