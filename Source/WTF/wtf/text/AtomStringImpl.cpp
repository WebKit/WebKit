/*
 * Copyright (C) 2004-2008, 2013-2014 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2015 Yusuke Suzuki<utatane.tea@gmail.com>. All rights reserved.
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

#include "config.h"
#include <wtf/text/AtomStringImpl.h>

#include <wtf/CommaPrinter.h>
#include <wtf/DataLog.h>
#include <wtf/HashSet.h>
#include <wtf/StringPrintStream.h>
#include <wtf/Threading.h>
#include <wtf/text/AtomStringTable.h>
#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/StringHash.h>
#include <wtf/unicode/UTF8Conversion.h>

#if USE(WEB_THREAD)
#include <wtf/Lock.h>
#endif

namespace WTF {

using namespace Unicode;

#if USE(WEB_THREAD)

class AtomStringTableLocker : public LockHolder {
    WTF_MAKE_NONCOPYABLE(AtomStringTableLocker);

    static Lock s_stringTableLock;
public:
    AtomStringTableLocker()
        : LockHolder(&s_stringTableLock)
    {
    }
};

Lock AtomStringTableLocker::s_stringTableLock;

#else

class AtomStringTableLocker {
    WTF_MAKE_NONCOPYABLE(AtomStringTableLocker);
public:
    AtomStringTableLocker() { }
};

#endif // USE(WEB_THREAD)

using StringTableImpl = HashSet<PackedPtr<StringImpl>>;

static ALWAYS_INLINE StringTableImpl& stringTable()
{
    return Thread::current().atomStringTable()->table();
}

template<typename T, typename HashTranslator>
static inline Ref<AtomStringImpl> addToStringTable(AtomStringTableLocker&, StringTableImpl& atomStringTable, const T& value)
{
    auto addResult = atomStringTable.add<HashTranslator>(value);

    // If the string is newly-translated, then we need to adopt it.
    // The boolean in the pair tells us if that is so.
    if (addResult.isNewEntry)
        return adoptRef(static_cast<AtomStringImpl&>(*addResult.iterator->get()));
    return *static_cast<AtomStringImpl*>(addResult.iterator->get());
}

template<typename T, typename HashTranslator>
static inline Ref<AtomStringImpl> addToStringTable(const T& value)
{
    AtomStringTableLocker locker;
    return addToStringTable<T, HashTranslator>(locker, stringTable(), value);
}

struct CStringTranslator {
    static unsigned hash(const LChar* characters)
    {
        return StringHasher::computeHashAndMaskTop8Bits(characters);
    }

    static inline bool equal(PackedPtr<StringImpl> str, const LChar* characters)
    {
        return WTF::equal(str.get(), characters);
    }

    static void translate(PackedPtr<StringImpl>& location, const LChar* const& characters, unsigned hash)
    {
        auto* pointer = &StringImpl::create(characters).leakRef();
        pointer->setHash(hash);
        pointer->setIsAtom(true);
        location = pointer;
    }
};

RefPtr<AtomStringImpl> AtomStringImpl::add(const LChar* characters)
{
    if (!characters)
        return nullptr;
    if (!*characters)
        return static_cast<AtomStringImpl*>(StringImpl::empty());

    return addToStringTable<const LChar*, CStringTranslator>(characters);
}

template<typename CharacterType>
struct HashTranslatorCharBuffer {
    const CharacterType* characters;
    unsigned length;
    unsigned hash;

    HashTranslatorCharBuffer(const CharacterType* characters, unsigned length)
        : characters(characters)
        , length(length)
        , hash(StringHasher::computeHashAndMaskTop8Bits(characters, length))
    {
    }

    HashTranslatorCharBuffer(const CharacterType* characters, unsigned length, unsigned hash)
        : characters(characters)
        , length(length)
        , hash(hash)
    {
    }
};

using UCharBuffer = HashTranslatorCharBuffer<UChar>;
struct UCharBufferTranslator {
    static unsigned hash(const UCharBuffer& buf)
    {
        return buf.hash;
    }

    static bool equal(PackedPtr<StringImpl> const& str, const UCharBuffer& buf)
    {
        return WTF::equal(str.get(), buf.characters, buf.length);
    }

    static void translate(PackedPtr<StringImpl>& location, const UCharBuffer& buf, unsigned hash)
    {
        auto* pointer = &StringImpl::create8BitIfPossible(buf.characters, buf.length).leakRef();
        pointer->setHash(hash);
        pointer->setIsAtom(true);
        location = pointer;
    }
};

struct HashAndUTF8Characters {
    unsigned hash;
    const char* characters;
    unsigned length;
    unsigned utf16Length;
};

struct HashAndUTF8CharactersTranslator {
    static unsigned hash(const HashAndUTF8Characters& buffer)
    {
        return buffer.hash;
    }

    static bool equal(PackedPtr<StringImpl> const& passedString, const HashAndUTF8Characters& buffer)
    {
        auto* string = passedString.get();
        if (buffer.utf16Length != string->length())
            return false;

        // If buffer contains only ASCII characters UTF-8 and UTF16 length are the same.
        if (buffer.utf16Length != buffer.length) {
            if (string->is8Bit())
                return equalLatin1WithUTF8(string->characters8(), buffer.characters, buffer.characters + buffer.length);

            return equalUTF16WithUTF8(string->characters16(), buffer.characters, buffer.characters + buffer.length);
        }

        if (string->is8Bit()) {
            const LChar* stringCharacters = string->characters8();

            for (unsigned i = 0; i < buffer.length; ++i) {
                ASSERT(isASCII(buffer.characters[i]));
                if (stringCharacters[i] != buffer.characters[i])
                    return false;
            }

            return true;
        }

        const UChar* stringCharacters = string->characters16();

        for (unsigned i = 0; i < buffer.length; ++i) {
            ASSERT(isASCII(buffer.characters[i]));
            if (stringCharacters[i] != buffer.characters[i])
                return false;
        }

        return true;
    }

    static void translate(PackedPtr<StringImpl>& location, const HashAndUTF8Characters& buffer, unsigned hash)
    {
        UChar* target;
        auto newString = StringImpl::createUninitialized(buffer.utf16Length, target);

        bool isAllASCII;
        const char* source = buffer.characters;
        if (!convertUTF8ToUTF16(source, source + buffer.length, &target, target + buffer.utf16Length, &isAllASCII))
            ASSERT_NOT_REACHED();

        if (isAllASCII)
            newString = StringImpl::create(buffer.characters, buffer.length);

        auto* pointer = &newString.leakRef();
        pointer->setHash(hash);
        pointer->setIsAtom(true);
        location = pointer;
    }
};

RefPtr<AtomStringImpl> AtomStringImpl::add(const UChar* characters, unsigned length)
{
    if (!characters)
        return nullptr;

    if (!length)
        return static_cast<AtomStringImpl*>(StringImpl::empty());

    UCharBuffer buffer { characters, length };
    return addToStringTable<UCharBuffer, UCharBufferTranslator>(buffer);
}

RefPtr<AtomStringImpl> AtomStringImpl::add(const UChar* characters)
{
    if (!characters)
        return nullptr;

    unsigned length = 0;
    while (characters[length] != UChar(0))
        ++length;

    if (!length)
        return static_cast<AtomStringImpl*>(StringImpl::empty());

    UCharBuffer buffer { characters, length };
    return addToStringTable<UCharBuffer, UCharBufferTranslator>(buffer);
}

struct SubstringLocation {
    StringImpl* baseString;
    unsigned start;
    unsigned length;
};

struct SubstringTranslator {
    static void translate(PackedPtr<StringImpl>& location, const SubstringLocation& buffer, unsigned hash)
    {
        auto* pointer = &StringImpl::createSubstringSharingImpl(*buffer.baseString, buffer.start, buffer.length).leakRef();
        pointer->setHash(hash);
        pointer->setIsAtom(true);
        location = pointer;
    }
};

struct SubstringTranslator8 : SubstringTranslator {
    static unsigned hash(const SubstringLocation& buffer)
    {
        return StringHasher::computeHashAndMaskTop8Bits(buffer.baseString->characters8() + buffer.start, buffer.length);
    }

    static bool equal(PackedPtr<StringImpl> const& string, const SubstringLocation& buffer)
    {
        return WTF::equal(string.get(), buffer.baseString->characters8() + buffer.start, buffer.length);
    }
};

struct SubstringTranslator16 : SubstringTranslator {
    static unsigned hash(const SubstringLocation& buffer)
    {
        return StringHasher::computeHashAndMaskTop8Bits(buffer.baseString->characters16() + buffer.start, buffer.length);
    }

    static bool equal(PackedPtr<StringImpl> const& string, const SubstringLocation& buffer)
    {
        return WTF::equal(string.get(), buffer.baseString->characters16() + buffer.start, buffer.length);
    }
};

RefPtr<AtomStringImpl> AtomStringImpl::add(StringImpl* baseString, unsigned start, unsigned length)
{
    if (!baseString)
        return nullptr;

    if (!length || start >= baseString->length())
        return static_cast<AtomStringImpl*>(StringImpl::empty());

    unsigned maxLength = baseString->length() - start;
    if (length >= maxLength) {
        if (!start)
            return add(baseString);
        length = maxLength;
    }

    SubstringLocation buffer = { baseString, start, length };
    if (baseString->is8Bit())
        return addToStringTable<SubstringLocation, SubstringTranslator8>(buffer);
    return addToStringTable<SubstringLocation, SubstringTranslator16>(buffer);
}
    
using LCharBuffer = HashTranslatorCharBuffer<LChar>;
struct LCharBufferTranslator {
    static unsigned hash(const LCharBuffer& buf)
    {
        return buf.hash;
    }

    static bool equal(PackedPtr<StringImpl> const& str, const LCharBuffer& buf)
    {
        return WTF::equal(str.get(), buf.characters, buf.length);
    }

    static void translate(PackedPtr<StringImpl>& location, const LCharBuffer& buf, unsigned hash)
    {
        auto* pointer = &StringImpl::create(buf.characters, buf.length).leakRef();
        pointer->setHash(hash);
        pointer->setIsAtom(true);
        location = pointer;
    }
};

template<typename CharType>
struct BufferFromStaticDataTranslator {
    using Buffer = HashTranslatorCharBuffer<CharType>;
    static unsigned hash(const Buffer& buf)
    {
        return buf.hash;
    }

    static bool equal(PackedPtr<StringImpl> const& str, const Buffer& buf)
    {
        return WTF::equal(str.get(), buf.characters, buf.length);
    }

    static void translate(PackedPtr<StringImpl>& location, const Buffer& buf, unsigned hash)
    {
        auto* pointer = &StringImpl::createWithoutCopying(buf.characters, buf.length).leakRef();
        pointer->setHash(hash);
        pointer->setIsAtom(true);
        location = pointer;
    }
};

RefPtr<AtomStringImpl> AtomStringImpl::add(const LChar* characters, unsigned length)
{
    if (!characters)
        return nullptr;

    if (!length)
        return static_cast<AtomStringImpl*>(StringImpl::empty());

    LCharBuffer buffer { characters, length };
    return addToStringTable<LCharBuffer, LCharBufferTranslator>(buffer);
}

Ref<AtomStringImpl> AtomStringImpl::addLiteral(const char* characters, unsigned length)
{
    ASSERT(characters);
    ASSERT(length);

    LCharBuffer buffer { reinterpret_cast<const LChar*>(characters), length };
    return addToStringTable<LCharBuffer, BufferFromStaticDataTranslator<LChar>>(buffer);
}

static Ref<AtomStringImpl> addSymbol(AtomStringTableLocker& locker, StringTableImpl& atomStringTable, StringImpl& base)
{
    ASSERT(base.length());
    ASSERT(base.isSymbol());

    SubstringLocation buffer = { &base, 0, base.length() };
    if (base.is8Bit())
        return addToStringTable<SubstringLocation, SubstringTranslator8>(locker, atomStringTable, buffer);
    return addToStringTable<SubstringLocation, SubstringTranslator16>(locker, atomStringTable, buffer);
}

static inline Ref<AtomStringImpl> addSymbol(StringImpl& base)
{
    AtomStringTableLocker locker;
    return addSymbol(locker, stringTable(), base);
}

static Ref<AtomStringImpl> addStatic(AtomStringTableLocker& locker, StringTableImpl& atomStringTable, const StringImpl& base)
{
    ASSERT(base.length());
    ASSERT(base.isStatic());

    if (base.is8Bit()) {
        LCharBuffer buffer { base.characters8(), base.length(), base.hash() };
        return addToStringTable<LCharBuffer, BufferFromStaticDataTranslator<LChar>>(locker, atomStringTable, buffer);
    }
    UCharBuffer buffer { base.characters16(), base.length(), base.hash() };
    return addToStringTable<UCharBuffer, BufferFromStaticDataTranslator<UChar>>(locker, atomStringTable, buffer);
}

static inline Ref<AtomStringImpl> addStatic(const StringImpl& base)
{
    AtomStringTableLocker locker;
    return addStatic(locker, stringTable(), base);
}

RefPtr<AtomStringImpl> AtomStringImpl::add(const StaticStringImpl* string)
{
    auto s = reinterpret_cast<const StringImpl*>(string);
    ASSERT(s->isStatic());
    return addStatic(*s);
}

Ref<AtomStringImpl> AtomStringImpl::addSlowCase(StringImpl& string)
{
    // This check is necessary for null symbols.
    // Their length is zero, but they are not AtomStringImpl.
    if (!string.length())
        return *static_cast<AtomStringImpl*>(StringImpl::empty());

    if (string.isStatic())
        return addStatic(string);

    if (string.isSymbol())
        return addSymbol(string);

    ASSERT_WITH_MESSAGE(!string.isAtom(), "AtomStringImpl should not hit the slow case if the string is already an atom.");

    AtomStringTableLocker locker;
    auto addResult = stringTable().add(&string);

    if (addResult.isNewEntry) {
        ASSERT(addResult.iterator->get() == &string);
        string.setIsAtom(true);
    }

    return *static_cast<AtomStringImpl*>(addResult.iterator->get());
}

Ref<AtomStringImpl> AtomStringImpl::addSlowCase(AtomStringTable& stringTable, StringImpl& string)
{
    // This check is necessary for null symbols.
    // Their length is zero, but they are not AtomStringImpl.
    if (!string.length())
        return *static_cast<AtomStringImpl*>(StringImpl::empty());

    if (string.isStatic()) {
        AtomStringTableLocker locker;
        return addStatic(locker, stringTable.table(), string);
    }

    if (string.isSymbol()) {
        AtomStringTableLocker locker;
        return addSymbol(locker, stringTable.table(), string);
    }

    ASSERT_WITH_MESSAGE(!string.isAtom(), "AtomStringImpl should not hit the slow case if the string is already an atom.");

    AtomStringTableLocker locker;
    auto addResult = stringTable.table().add(&string);

    if (addResult.isNewEntry) {
        ASSERT(addResult.iterator->get() == &string);
        string.setIsAtom(true);
    }

    return *static_cast<AtomStringImpl*>(addResult.iterator->get());
}

void AtomStringImpl::remove(AtomStringImpl* string)
{
    ASSERT(string->isAtom());
    AtomStringTableLocker locker;
    auto& atomStringTable = stringTable();
    auto iterator = atomStringTable.find(string);
    ASSERT_WITH_MESSAGE(iterator != atomStringTable.end(), "The string being removed is an atom in the string table of an other thread!");
    ASSERT(string == iterator->get());
    atomStringTable.remove(iterator);
}

RefPtr<AtomStringImpl> AtomStringImpl::lookUpSlowCase(StringImpl& string)
{
    ASSERT_WITH_MESSAGE(!string.isAtom(), "AtomStringImpl objects should return from the fast case.");

    if (!string.length())
        return static_cast<AtomStringImpl*>(StringImpl::empty());

    AtomStringTableLocker locker;
    auto& atomStringTable = stringTable();
    auto iterator = atomStringTable.find(&string);
    if (iterator != atomStringTable.end())
        return static_cast<AtomStringImpl*>(iterator->get());
    return nullptr;
}

RefPtr<AtomStringImpl> AtomStringImpl::addUTF8(const char* charactersStart, const char* charactersEnd)
{
    HashAndUTF8Characters buffer;
    buffer.characters = charactersStart;
    buffer.hash = calculateStringHashAndLengthFromUTF8MaskingTop8Bits(charactersStart, charactersEnd, buffer.length, buffer.utf16Length);

    if (!buffer.hash)
        return nullptr;

    return addToStringTable<HashAndUTF8Characters, HashAndUTF8CharactersTranslator>(buffer);
}

RefPtr<AtomStringImpl> AtomStringImpl::lookUp(const LChar* characters, unsigned length)
{
    AtomStringTableLocker locker;
    auto& table = stringTable();

    LCharBuffer buffer = { characters, length };
    auto iterator = table.find<LCharBufferTranslator>(buffer);
    if (iterator != table.end())
        return static_cast<AtomStringImpl*>(iterator->get());
    return nullptr;
}

RefPtr<AtomStringImpl> AtomStringImpl::lookUp(const UChar* characters, unsigned length)
{
    AtomStringTableLocker locker;
    auto& table = stringTable();

    UCharBuffer buffer { characters, length };
    auto iterator = table.find<UCharBufferTranslator>(buffer);
    if (iterator != table.end())
        return static_cast<AtomStringImpl*>(iterator->get());
    return nullptr;
}

#if ASSERT_ENABLED
bool AtomStringImpl::isInAtomStringTable(StringImpl* string)
{
    AtomStringTableLocker locker;
    return stringTable().contains(string);
}
#endif

} // namespace WTF
