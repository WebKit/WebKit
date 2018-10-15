/*
 * Copyright (C) 2006 Apple Inc.
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

class AtomicStringTable;

class AtomicStringImpl : public UniquedStringImpl {
public:
    WTF_EXPORT_PRIVATE static RefPtr<AtomicStringImpl> lookUp(const LChar*, unsigned length);
    WTF_EXPORT_PRIVATE static RefPtr<AtomicStringImpl> lookUp(const UChar*, unsigned length);
    static RefPtr<AtomicStringImpl> lookUp(StringImpl* string)
    {
        if (!string || string->isAtomic())
            return static_cast<AtomicStringImpl*>(string);
        return lookUpSlowCase(*string);
    }

    static void remove(AtomicStringImpl*);

    WTF_EXPORT_PRIVATE static RefPtr<AtomicStringImpl> add(const LChar*);
    ALWAYS_INLINE static RefPtr<AtomicStringImpl> add(const char* s) { return add(reinterpret_cast<const LChar*>(s)); };
    WTF_EXPORT_PRIVATE static RefPtr<AtomicStringImpl> add(const LChar*, unsigned length);
    WTF_EXPORT_PRIVATE static RefPtr<AtomicStringImpl> add(const UChar*, unsigned length);
    ALWAYS_INLINE static RefPtr<AtomicStringImpl> add(const char* s, unsigned length) { return add(reinterpret_cast<const LChar*>(s), length); };
    WTF_EXPORT_PRIVATE static RefPtr<AtomicStringImpl> add(const UChar*);
    WTF_EXPORT_PRIVATE static RefPtr<AtomicStringImpl> add(StringImpl*, unsigned offset, unsigned length);
    ALWAYS_INLINE static RefPtr<AtomicStringImpl> add(StringImpl* string)
    {
        if (!string)
            return static_cast<AtomicStringImpl*>(string);
        return add(*string);
    }
    WTF_EXPORT_PRIVATE static RefPtr<AtomicStringImpl> add(const StaticStringImpl*);
    WTF_EXPORT_PRIVATE static Ref<AtomicStringImpl> addLiteral(const char* characters, unsigned length);

    // Returns null if the input data contains an invalid UTF-8 sequence.
    WTF_EXPORT_PRIVATE static RefPtr<AtomicStringImpl> addUTF8(const char* start, const char* end);
#if USE(CF)
    WTF_EXPORT_PRIVATE static RefPtr<AtomicStringImpl> add(CFStringRef);
#endif

    template<typename StringTableProvider>
    ALWAYS_INLINE static RefPtr<AtomicStringImpl> addWithStringTableProvider(StringTableProvider& stringTableProvider, StringImpl* string)
    {
        if (!string)
            return nullptr;
        return add(*stringTableProvider.atomicStringTable(), *string);
    }

#if !ASSERT_DISABLED
    WTF_EXPORT_PRIVATE static bool isInAtomicStringTable(StringImpl*);
#endif

private:
    AtomicStringImpl() = delete;

    ALWAYS_INLINE static Ref<AtomicStringImpl> add(StringImpl& string)
    {
        if (string.isAtomic()) {
            ASSERT_WITH_MESSAGE(!string.length() || isInAtomicStringTable(&string), "The atomic string comes from an other thread!");
            return static_cast<AtomicStringImpl&>(string);
        }
        return addSlowCase(string);
    }

    ALWAYS_INLINE static Ref<AtomicStringImpl> add(AtomicStringTable& stringTable, StringImpl& string)
    {
        if (string.isAtomic()) {
            ASSERT_WITH_MESSAGE(!string.length() || isInAtomicStringTable(&string), "The atomic string comes from an other thread!");
            return static_cast<AtomicStringImpl&>(string);
        }
        return addSlowCase(stringTable, string);
    }

    WTF_EXPORT_PRIVATE static Ref<AtomicStringImpl> addSlowCase(StringImpl&);
    WTF_EXPORT_PRIVATE static Ref<AtomicStringImpl> addSlowCase(AtomicStringTable&, StringImpl&);

    WTF_EXPORT_PRIVATE static RefPtr<AtomicStringImpl> lookUpSlowCase(StringImpl&);
};

#if !ASSERT_DISABLED
// AtomicStringImpls created from StaticStringImpl will ASSERT
// in the generic ValueCheck<T>::checkConsistency
// as they are not allocated by fastMalloc.
// We don't currently have any way to detect that case
// so we ignore the consistency check for all AtomicStringImpls*.
template<> struct
ValueCheck<AtomicStringImpl*> {
    static void checkConsistency(const AtomicStringImpl*) { }
};

template<> struct
ValueCheck<const AtomicStringImpl*> {
    static void checkConsistency(const AtomicStringImpl*) { }
};
#endif

}

using WTF::AtomicStringImpl;
