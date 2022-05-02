/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <wtf/text/UniquedStringImpl.h>

namespace WTF {

class AtomStringTable;

class AtomStringImpl final : public UniquedStringImpl {
public:
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> lookUp(const LChar*, unsigned length);
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> lookUp(const UChar*, unsigned length);
    static RefPtr<AtomStringImpl> lookUp(StringImpl* string)
    {
        if (!string || string->isAtom())
            return static_cast<AtomStringImpl*>(string);
        return lookUpSlowCase(*string);
    }

    static void remove(AtomStringImpl*);

    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> add(const LChar*, unsigned length);
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> add(const UChar*, unsigned length);
    ALWAYS_INLINE static RefPtr<AtomStringImpl> add(const char* s, unsigned length) { return add(reinterpret_cast<const LChar*>(s), length); }
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> add(StringImpl*, unsigned offset, unsigned length);
    ALWAYS_INLINE static RefPtr<AtomStringImpl> add(StringImpl* string)
    {
        if (!string)
            return nullptr;
        return add(*string);
    }
    ALWAYS_INLINE static RefPtr<AtomStringImpl> add(RefPtr<StringImpl>&& string)
    {
        if (!string)
            return nullptr;
        return add(string.releaseNonNull());
    }
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> add(const StaticStringImpl*);
    ALWAYS_INLINE static Ref<AtomStringImpl> add(ASCIILiteral literal) { return addLiteral(literal.characters(), literal.length()); }

    // Not using the add() naming to encourage developers to call add(ASCIILiteral) when they have a string literal.
    ALWAYS_INLINE static RefPtr<AtomStringImpl> addCString(const char* s) { return s ? add(s, strlen(s)) : nullptr; }

    // Returns null if the input data contains an invalid UTF-8 sequence.
    static RefPtr<AtomStringImpl> addUTF8(const char* start, const char* end);

#if USE(CF)
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> add(CFStringRef);
#endif

    template<typename StringTableProvider>
    ALWAYS_INLINE static RefPtr<AtomStringImpl> addWithStringTableProvider(StringTableProvider& stringTableProvider, StringImpl* string)
    {
        if (!string)
            return nullptr;
        return add(*stringTableProvider.atomStringTable(), *string);
    }

#if ASSERT_ENABLED
    WTF_EXPORT_PRIVATE static bool isInAtomStringTable(StringImpl*);
#endif

private:
    AtomStringImpl() = delete;

    ALWAYS_INLINE static Ref<AtomStringImpl> add(StringImpl& string)
    {
        if (string.isAtom()) {
            ASSERT_WITH_MESSAGE(!string.length() || isInAtomStringTable(&string), "The atom string comes from an other thread!");
            return static_cast<AtomStringImpl&>(string);
        }
        return addSlowCase(string);
    }

    ALWAYS_INLINE static Ref<AtomStringImpl> add(Ref<StringImpl>&& string)
    {
        if (string->isAtom()) {
            ASSERT_WITH_MESSAGE(!string->length() || isInAtomStringTable(string.ptr()), "The atom string comes from an other thread!");
            return static_reference_cast<AtomStringImpl>(WTFMove(string));
        }
        return addSlowCase(WTFMove(string));
    }

    WTF_EXPORT_PRIVATE static Ref<AtomStringImpl> addLiteral(const char* characters, unsigned length);

    ALWAYS_INLINE static Ref<AtomStringImpl> add(AtomStringTable& stringTable, StringImpl& string)
    {
        if (string.isAtom()) {
            ASSERT_WITH_MESSAGE(!string.length() || isInAtomStringTable(&string), "The atom string comes from an other thread!");
            return static_cast<AtomStringImpl&>(string);
        }
        return addSlowCase(stringTable, string);
    }

    WTF_EXPORT_PRIVATE static Ref<AtomStringImpl> addSlowCase(StringImpl&);
    WTF_EXPORT_PRIVATE static Ref<AtomStringImpl> addSlowCase(Ref<StringImpl>&&);
    WTF_EXPORT_PRIVATE static Ref<AtomStringImpl> addSlowCase(AtomStringTable&, StringImpl&);

    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> lookUpSlowCase(StringImpl&);
};

#if ASSERT_ENABLED

// AtomStringImpls created from StaticStringImpl will ASSERT in the generic ValueCheck<T>::checkConsistency,
// as they are not allocated by fastMalloc. We don't currently have any way to detect that case, so we don't
// do any consistency check for AtomStringImpl*.

template<> struct ValueCheck<AtomStringImpl*> {
    static void checkConsistency(const AtomStringImpl*) { }
};

template<> struct ValueCheck<const AtomStringImpl*> {
    static void checkConsistency(const AtomStringImpl*) { }
};

#endif // ASSERT_ENABLED

}

using WTF::AtomStringImpl;
