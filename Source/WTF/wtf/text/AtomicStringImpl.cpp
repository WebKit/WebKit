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
#include <wtf/text/AtomicStringImpl.h>

#include <wtf/CommaPrinter.h>
#include <wtf/DataLog.h>
#include <wtf/HashSet.h>
#include <wtf/StringPrintStream.h>
#include <wtf/Threading.h>
#include <wtf/text/AtomicStringTable.h>
#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/StringHash.h>
#include <wtf/unicode/UTF8.h>

#if USE(WEB_THREAD)
#include <wtf/Lock.h>
#endif

namespace WTF {

using namespace Unicode;

#if USE(WEB_THREAD)

class AtomicStringTableLocker : public LockHolder {
    WTF_MAKE_NONCOPYABLE(AtomicStringTableLocker);

    static Lock s_stringTableLock;
public:
    AtomicStringTableLocker()
        : LockHolder(&s_stringTableLock)
    {
    }
};

Lock AtomicStringTableLocker::s_stringTableLock;

#else

class AtomicStringTableLocker {
    WTF_MAKE_NONCOPYABLE(AtomicStringTableLocker);
public:
    AtomicStringTableLocker() { }
};

#endif // USE(WEB_THREAD)

using StringTableImpl = HashSet<StringImpl*>;

static ALWAYS_INLINE StringTableImpl& stringTable()
{
    return Thread::current().atomicStringTable()->table();
}

template<typename T, typename HashTranslator>
static inline Ref<AtomicStringImpl> addToStringTable(AtomicStringTableLocker&, StringTableImpl& atomicStringTable, const T& value)
{
    auto addResult = atomicStringTable.add<HashTranslator>(value);

    // If the string is newly-translated, then we need to adopt it.
    // The boolean in the pair tells us if that is so.
    if (addResult.isNewEntry)
        return adoptRef(static_cast<AtomicStringImpl&>(**addResult.iterator));
    return *static_cast<AtomicStringImpl*>(*addResult.iterator);
}

template<typename T, typename HashTranslator>
static inline Ref<AtomicStringImpl> addToStringTable(const T& value)
{
    AtomicStringTableLocker locker;
    return addToStringTable<T, HashTranslator>(locker, stringTable(), value);
}

struct CStringTranslator {
    static unsigned hash(const LChar* characters)
    {
        return StringHasher::computeHashAndMaskTop8Bits(characters);
    }

    static inline bool equal(StringImpl* str, const LChar* characters)
    {
        return WTF::equal(str, characters);
    }

    static void translate(StringImpl*& location, const LChar* const& characters, unsigned hash)
    {
        location = &StringImpl::create(characters).leakRef();
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

RefPtr<AtomicStringImpl> AtomicStringImpl::add(const LChar* characters)
{
    if (!characters)
        return nullptr;
    if (!*characters)
        return static_cast<AtomicStringImpl*>(StringImpl::empty());

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

    static bool equal(StringImpl* const& str, const UCharBuffer& buf)
    {
        return WTF::equal(str, buf.characters, buf.length);
    }

    static void translate(StringImpl*& location, const UCharBuffer& buf, unsigned hash)
    {
        location = &StringImpl::create8BitIfPossible(buf.characters, buf.length).leakRef();
        location->setHash(hash);
        location->setIsAtomic(true);
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

    static bool equal(StringImpl* const& string, const HashAndUTF8Characters& buffer)
    {
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

    static void translate(StringImpl*& location, const HashAndUTF8Characters& buffer, unsigned hash)
    {
        UChar* target;
        auto newString = StringImpl::createUninitialized(buffer.utf16Length, target);

        bool isAllASCII;
        const char* source = buffer.characters;
        if (convertUTF8ToUTF16(&source, source + buffer.length, &target, target + buffer.utf16Length, &isAllASCII) != conversionOK)
            ASSERT_NOT_REACHED();

        if (isAllASCII)
            newString = StringImpl::create(buffer.characters, buffer.length);

        location = &newString.leakRef();
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

RefPtr<AtomicStringImpl> AtomicStringImpl::add(const UChar* characters, unsigned length)
{
    if (!characters)
        return nullptr;

    if (!length)
        return static_cast<AtomicStringImpl*>(StringImpl::empty());

    UCharBuffer buffer { characters, length };
    return addToStringTable<UCharBuffer, UCharBufferTranslator>(buffer);
}

RefPtr<AtomicStringImpl> AtomicStringImpl::add(const UChar* characters)
{
    if (!characters)
        return nullptr;

    unsigned length = 0;
    while (characters[length] != UChar(0))
        ++length;

    if (!length)
        return static_cast<AtomicStringImpl*>(StringImpl::empty());

    UCharBuffer buffer { characters, length };
    return addToStringTable<UCharBuffer, UCharBufferTranslator>(buffer);
}

struct SubstringLocation {
    StringImpl* baseString;
    unsigned start;
    unsigned length;
};

struct SubstringTranslator {
    static void translate(StringImpl*& location, const SubstringLocation& buffer, unsigned hash)
    {
        location = &StringImpl::createSubstringSharingImpl(*buffer.baseString, buffer.start, buffer.length).leakRef();
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

struct SubstringTranslator8 : SubstringTranslator {
    static unsigned hash(const SubstringLocation& buffer)
    {
        return StringHasher::computeHashAndMaskTop8Bits(buffer.baseString->characters8() + buffer.start, buffer.length);
    }

    static bool equal(StringImpl* const& string, const SubstringLocation& buffer)
    {
        return WTF::equal(string, buffer.baseString->characters8() + buffer.start, buffer.length);
    }
};

struct SubstringTranslator16 : SubstringTranslator {
    static unsigned hash(const SubstringLocation& buffer)
    {
        return StringHasher::computeHashAndMaskTop8Bits(buffer.baseString->characters16() + buffer.start, buffer.length);
    }

    static bool equal(StringImpl* const& string, const SubstringLocation& buffer)
    {
        return WTF::equal(string, buffer.baseString->characters16() + buffer.start, buffer.length);
    }
};

RefPtr<AtomicStringImpl> AtomicStringImpl::add(StringImpl* baseString, unsigned start, unsigned length)
{
    if (!baseString)
        return nullptr;

    if (!length || start >= baseString->length())
        return static_cast<AtomicStringImpl*>(StringImpl::empty());

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

    static bool equal(StringImpl* const& str, const LCharBuffer& buf)
    {
        return WTF::equal(str, buf.characters, buf.length);
    }

    static void translate(StringImpl*& location, const LCharBuffer& buf, unsigned hash)
    {
        location = &StringImpl::create(buf.characters, buf.length).leakRef();
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

template<typename CharType>
struct BufferFromStaticDataTranslator {
    using Buffer = HashTranslatorCharBuffer<CharType>;
    static unsigned hash(const Buffer& buf)
    {
        return buf.hash;
    }

    static bool equal(StringImpl* const& str, const Buffer& buf)
    {
        return WTF::equal(str, buf.characters, buf.length);
    }

    static void translate(StringImpl*& location, const Buffer& buf, unsigned hash)
    {
        location = &StringImpl::createWithoutCopying(buf.characters, buf.length).leakRef();
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

RefPtr<AtomicStringImpl> AtomicStringImpl::add(const LChar* characters, unsigned length)
{
    if (!characters)
        return nullptr;

    if (!length)
        return static_cast<AtomicStringImpl*>(StringImpl::empty());

    LCharBuffer buffer { characters, length };
    return addToStringTable<LCharBuffer, LCharBufferTranslator>(buffer);
}

Ref<AtomicStringImpl> AtomicStringImpl::addLiteral(const char* characters, unsigned length)
{
    ASSERT(characters);
    ASSERT(length);

    LCharBuffer buffer { reinterpret_cast<const LChar*>(characters), length };
    return addToStringTable<LCharBuffer, BufferFromStaticDataTranslator<LChar>>(buffer);
}

static Ref<AtomicStringImpl> addSymbol(AtomicStringTableLocker& locker, StringTableImpl& atomicStringTable, StringImpl& base)
{
    ASSERT(base.length());
    ASSERT(base.isSymbol());

    SubstringLocation buffer = { &base, 0, base.length() };
    if (base.is8Bit())
        return addToStringTable<SubstringLocation, SubstringTranslator8>(locker, atomicStringTable, buffer);
    return addToStringTable<SubstringLocation, SubstringTranslator16>(locker, atomicStringTable, buffer);
}

static inline Ref<AtomicStringImpl> addSymbol(StringImpl& base)
{
    AtomicStringTableLocker locker;
    return addSymbol(locker, stringTable(), base);
}

static Ref<AtomicStringImpl> addStatic(AtomicStringTableLocker& locker, StringTableImpl& atomicStringTable, const StringImpl& base)
{
    ASSERT(base.length());
    ASSERT(base.isStatic());

    if (base.is8Bit()) {
        LCharBuffer buffer { base.characters8(), base.length(), base.hash() };
        return addToStringTable<LCharBuffer, BufferFromStaticDataTranslator<LChar>>(locker, atomicStringTable, buffer);
    }
    UCharBuffer buffer { base.characters16(), base.length(), base.hash() };
    return addToStringTable<UCharBuffer, BufferFromStaticDataTranslator<UChar>>(locker, atomicStringTable, buffer);
}

static inline Ref<AtomicStringImpl> addStatic(const StringImpl& base)
{
    AtomicStringTableLocker locker;
    return addStatic(locker, stringTable(), base);
}

RefPtr<AtomicStringImpl> AtomicStringImpl::add(const StaticStringImpl* string)
{
    auto s = reinterpret_cast<const StringImpl*>(string);
    ASSERT(s->isStatic());
    return addStatic(*s);
}

Ref<AtomicStringImpl> AtomicStringImpl::addSlowCase(StringImpl& string)
{
    // This check is necessary for null symbols.
    // Their length is zero, but they are not AtomicStringImpl.
    if (!string.length())
        return *static_cast<AtomicStringImpl*>(StringImpl::empty());

    if (string.isStatic())
        return addStatic(string);

    if (string.isSymbol())
        return addSymbol(string);

    ASSERT_WITH_MESSAGE(!string.isAtomic(), "AtomicStringImpl should not hit the slow case if the string is already atomic.");

    AtomicStringTableLocker locker;
    auto addResult = stringTable().add(&string);

    if (addResult.isNewEntry) {
        ASSERT(*addResult.iterator == &string);
        string.setIsAtomic(true);
    }

    return *static_cast<AtomicStringImpl*>(*addResult.iterator);
}

Ref<AtomicStringImpl> AtomicStringImpl::addSlowCase(AtomicStringTable& stringTable, StringImpl& string)
{
    // This check is necessary for null symbols.
    // Their length is zero, but they are not AtomicStringImpl.
    if (!string.length())
        return *static_cast<AtomicStringImpl*>(StringImpl::empty());

    if (string.isStatic()) {
        AtomicStringTableLocker locker;
        return addStatic(locker, stringTable.table(), string);
    }

    if (string.isSymbol()) {
        AtomicStringTableLocker locker;
        return addSymbol(locker, stringTable.table(), string);
    }

    ASSERT_WITH_MESSAGE(!string.isAtomic(), "AtomicStringImpl should not hit the slow case if the string is already atomic.");

    AtomicStringTableLocker locker;
    auto addResult = stringTable.table().add(&string);

    if (addResult.isNewEntry) {
        ASSERT(*addResult.iterator == &string);
        string.setIsAtomic(true);
    }

    return *static_cast<AtomicStringImpl*>(*addResult.iterator);
}

void AtomicStringImpl::remove(AtomicStringImpl* string)
{
    ASSERT(string->isAtomic());
    AtomicStringTableLocker locker;
    auto& atomicStringTable = stringTable();
    auto iterator = atomicStringTable.find(string);
    ASSERT_WITH_MESSAGE(iterator != atomicStringTable.end(), "The string being removed is atomic in the string table of an other thread!");
    ASSERT(string == *iterator);
    atomicStringTable.remove(iterator);
}

RefPtr<AtomicStringImpl> AtomicStringImpl::lookUpSlowCase(StringImpl& string)
{
    ASSERT_WITH_MESSAGE(!string.isAtomic(), "AtomicStringImpls should return from the fast case.");

    if (!string.length())
        return static_cast<AtomicStringImpl*>(StringImpl::empty());

    AtomicStringTableLocker locker;
    auto& atomicStringTable = stringTable();
    auto iterator = atomicStringTable.find(&string);
    if (iterator != atomicStringTable.end())
        return static_cast<AtomicStringImpl*>(*iterator);
    return nullptr;
}

RefPtr<AtomicStringImpl> AtomicStringImpl::addUTF8(const char* charactersStart, const char* charactersEnd)
{
    HashAndUTF8Characters buffer;
    buffer.characters = charactersStart;
    buffer.hash = calculateStringHashAndLengthFromUTF8MaskingTop8Bits(charactersStart, charactersEnd, buffer.length, buffer.utf16Length);

    if (!buffer.hash)
        return nullptr;

    return addToStringTable<HashAndUTF8Characters, HashAndUTF8CharactersTranslator>(buffer);
}

RefPtr<AtomicStringImpl> AtomicStringImpl::lookUp(const LChar* characters, unsigned length)
{
    AtomicStringTableLocker locker;
    auto& table = stringTable();

    LCharBuffer buffer = { characters, length };
    auto iterator = table.find<LCharBufferTranslator>(buffer);
    if (iterator != table.end())
        return static_cast<AtomicStringImpl*>(*iterator);
    return nullptr;
}

RefPtr<AtomicStringImpl> AtomicStringImpl::lookUp(const UChar* characters, unsigned length)
{
    AtomicStringTableLocker locker;
    auto& table = stringTable();

    UCharBuffer buffer { characters, length };
    auto iterator = table.find<UCharBufferTranslator>(buffer);
    if (iterator != table.end())
        return static_cast<AtomicStringImpl*>(*iterator);
    return nullptr;
}

#if !ASSERT_DISABLED
bool AtomicStringImpl::isInAtomicStringTable(StringImpl* string)
{
    AtomicStringTableLocker locker;
    return stringTable().contains(string);
}
#endif

} // namespace WTF
