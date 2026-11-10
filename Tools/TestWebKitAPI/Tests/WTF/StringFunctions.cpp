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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/text/StringFunctions.h>

#include "Helpers/Test.h"
#include "StringFunctionsExpectations.h"
#include <sstream>
#include <wtf/CrossThreadCopier.h>
#include <wtf/FixedVector.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Hasher.h>
#include <wtf/Markable.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/StringPrintStream.h>
#include <wtf/Variant.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/SymbolImpl.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#include <wtf/RetainPtr.h>
#endif

namespace TestWebKitAPI {

using namespace std::literals;

namespace Expected = StringFunctionsExpectations;
using Expected::ExpectedString;

// Inputs: null, empty, ASCII, Latin-1 8-bit, 16-bit, atom, and some with interesting content.
// The expected values in StringFunctionsExpectations.h are indexed like this vector.
static Vector<RefPtr<StringImpl>> sampleStrings()
{
    return {
        nullptr,
        emptyString(),
        "Hello, World"_s,
        StringImpl::fromLatin1("Caf\xE9 cr\xE8me \xC9t\xE9"),
        u"Строка Hello é World"_str,
        AtomStringImpl::add("Atomic Hello World"_s).ptr(),
        u"İstanbul ISTANBUL istanbul"_str,
        "  Leading and trailing \t whitespace \n "_s,
        "a,b,,c,"_s,
        "1.5"_s,
        "  -42.25e1junk"_s,
        u"\U0001F600 emoji \U0001F600"_str,
        "lowercase only"_s,
        "UPPERCASE ONLY"_s,
    };
}

static RefPtr<StringImpl> createString(const ExpectedString& expected)
{
    if (!expected)
        return nullptr;
    return StringImpl::create(std::span<const char16_t> { expected->data(), expected->size() });
}

static std::span<const char16_t> expectedSpan(const ExpectedString& expected)
{
    if (!expected)
        return { };
    return { expected->data(), expected->size() };
}

static void expectString(const ExpectedString& expected, const RefPtr<StringImpl>& actual)
{
    if (!expected) {
        EXPECT_NULL(actual) << "actual: \"" << actual << '"';
        return;
    }
    EXPECT_NOT_NULL(actual);
    EXPECT_TRUE(equal(actual, expectedSpan(expected))) << "actual: \"" << actual << "\", expected: \"" << createString(expected) << '"';
    EXPECT_EQ(expected->size(), length(actual));
}

static void expectVector(const std::vector<ExpectedString>& expected, const Vector<RefPtr<StringImpl>>& actual)
{
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < expected.size(); ++i)
        expectString(expected[i], actual[i]);
}

static RefPtr<AtomStringImpl> expectedAtom(const ExpectedString& expected)
{
    if (!expected)
        return nullptr;
    return AtomStringImpl::add(expectedSpan(expected));
}

template<typename CStringType> static std::string_view cStringView(const CStringType& string)
{
    auto span = string.span();
    return { byteCast<char>(span).data(), span.size() };
}

static bool isSpaceCharacter(char16_t character)
{
    return isASCIIWhitespace(character);
}

TEST(WTF_StringFunctions, PointerLikeConcept)
{
    using WTF::StringImplPointerLike;
    using WTF::AtomStringImplPointerLike;
    static_assert(StringImplPointerLike<StringImpl*>);
    static_assert(StringImplPointerLike<const StringImpl*>);
    static_assert(StringImplPointerLike<AtomStringImpl*>);
    static_assert(StringImplPointerLike<RefPtr<StringImpl>>);
    static_assert(StringImplPointerLike<const RefPtr<StringImpl>&>);
    static_assert(StringImplPointerLike<RefPtr<AtomStringImpl>>);
    static_assert(StringImplPointerLike<Ref<StringImpl>>);
    static_assert(StringImplPointerLike<Ref<AtomStringImpl>>);
    static_assert(StringImplPointerLike<RefPtr<UniquedStringImpl>>);
    static_assert(!StringImplPointerLike<StringView>);
    static_assert(!StringImplPointerLike<ASCIILiteral>);
    static_assert(!StringImplPointerLike<const char*>);
    static_assert(!StringImplPointerLike<int*>);
    static_assert(AtomStringImplPointerLike<RefPtr<AtomStringImpl>>);
    static_assert(!AtomStringImplPointerLike<RefPtr<StringImpl>>);
}

TEST(WTF_StringFunctions, SampleStrings)
{
    auto samples = sampleStrings();
    ASSERT_EQ(Expected::sampleContents.size(), samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        SCOPED_TRACE(i);
        expectString(Expected::sampleContents[i], samples[i]);
    }
    EXPECT_TRUE(samples[1]->isStatic());
    EXPECT_TRUE(samples[3]->is8Bit());
    EXPECT_FALSE(samples[4]->is8Bit());
    EXPECT_TRUE(samples[5]->isAtom());
}

TEST(WTF_StringFunctions, Properties)
{
    auto samples = sampleStrings();
    for (size_t sample = 0; sample < samples.size(); ++sample) {
        SCOPED_TRACE(sample);
        auto& expected = Expected::properties[sample];
        auto content = expectedSpan(Expected::sampleContents[sample]);
        const RefPtr<StringImpl> pointer = samples[sample];
        EXPECT_EQ(expected.isEmpty, isEmpty(pointer));
        EXPECT_EQ(expected.isEmpty, isEmpty(pointer.get()));
        EXPECT_EQ(expected.isEmpty, isEmpty(static_cast<const StringImpl*>(pointer.get())));
        EXPECT_EQ(expected.length, length(pointer));
        EXPECT_EQ(expected.length, length(pointer.get()));
        EXPECT_EQ(expected.is8Bit, is8Bit(pointer));
        EXPECT_EQ(expected.sizeInBytes, sizeInBytes(pointer));
        EXPECT_EQ(expected.hash, hash(pointer));
        EXPECT_EQ(expected.hash, existingHash(pointer));
        EXPECT_EQ(expected.containsOnlyASCII, containsOnlyASCII(pointer));
        EXPECT_EQ(expected.containsOnlyLatin1, containsOnlyLatin1(pointer));
        EXPECT_EQ(expected.containsOnlySpaces, containsOnly<isSpaceCharacter>(pointer));
        auto direction = defaultWritingDirection(pointer);
        EXPECT_EQ(expected.defaultWritingDirection, direction ? static_cast<int>(*direction) : -1);
        if (expected.is8Bit) {
            EXPECT_EQ(pointer ? pointer->span8().data() : nullptr, span8(pointer).data());
            EXPECT_EQ(expected.length, span8(pointer).size());
            EXPECT_EQ(span8(pointer).data(), span<Latin1Character>(pointer).data());
            EXPECT_TRUE(std::ranges::equal(span8(pointer), content, [](Latin1Character a, char16_t b) {
                return static_cast<char16_t>(a) == b;
            }));
        } else {
            EXPECT_EQ(pointer->span16().data(), span16(pointer).data());
            EXPECT_EQ(expected.length, span16(pointer).size());
            EXPECT_EQ(span16(pointer).data(), span<char16_t>(pointer).data());
            EXPECT_TRUE(std::ranges::equal(span16(pointer), content));
        }
        ASSERT_EQ(expected.length, expected.codePointsAt.size());
        for (unsigned i = 0; i < expected.length + 2; ++i) {
            EXPECT_EQ(i < expected.length ? content[i] : 0, codeUnitAt(pointer, i));
            EXPECT_EQ(i < expected.length ? expected.codePointsAt[i] : 0, codePointAt(pointer, i));
        }
        if (pointer) {
            Ref<StringImpl> ref = *pointer;
            EXPECT_EQ(expected.length, length(ref));
            EXPECT_EQ(expected.hash, hash(ref));
        }
    }
    RefPtr<StringImpl> null;
    EXPECT_TRUE(isEmpty(null));
    EXPECT_EQ(0u, length(null));
    EXPECT_TRUE(is8Bit(null));
    EXPECT_EQ(0u, hash(null));
    EXPECT_EQ(0, codeUnitAt(null, 0));
    EXPECT_FALSE(defaultWritingDirection(null));
}

TEST(WTF_StringFunctions, CStrings)
{
    auto samples = sampleStrings();
    for (size_t sample = 0; sample < samples.size(); ++sample) {
        SCOPED_TRACE(sample);
        auto& expected = Expected::cStrings[sample];
        auto content = expectedSpan(Expected::sampleContents[sample]);
        RefPtr<StringImpl> pointer = samples[sample];
        EXPECT_EQ(expected.ascii, cStringView(ascii(pointer)));
        EXPECT_EQ(expected.latin1, cStringView(latin1(pointer)));
        EXPECT_EQ(expected.utf8, cStringView(utf8(pointer)));
        EXPECT_EQ(expected.utf8Strict, cStringView(utf8(pointer, StrictConversion)));
        EXPECT_EQ(expected.utf8, cStringView(tryGetUTF8(pointer).value()));
        EXPECT_EQ(expected.utf8Strict, cStringView(tryGetUTF8(pointer, StrictConversion).value()));
        auto sizeFunction = [](std::span<const char8_t> span) {
            return span.size();
        };
        EXPECT_EQ(expected.utf8.size(), tryGetUTF8(pointer, sizeFunction).value());
        auto withNullTermination = charactersWithNullTermination(pointer).value();
        ASSERT_EQ(content.size() + 1, withNullTermination.size());
        EXPECT_TRUE(std::ranges::equal(withNullTermination.span().first(content.size()), content));
        EXPECT_EQ(0, withNullTermination.last());
        EXPECT_TRUE(std::ranges::equal(charactersWithoutNullTermination(pointer).value().span(), content));
    }
    // Null: like a null String, an empty CString, and the function is called with an empty non-null span.
    RefPtr<StringImpl> null;
    EXPECT_EQ(0u, utf8(null).length());
    EXPECT_NOT_NULL(utf8(null).data());
    auto isEmptyNonNullSpan = [](std::span<const char8_t> span) {
        return !!span.data() && span.empty();
    };
    EXPECT_TRUE(tryGetUTF8(null, isEmptyNonNullSpan).value());
    EXPECT_TRUE(charactersWithoutNullTermination(null).value().isEmpty());
}

TEST(WTF_StringFunctions, Search)
{
    auto isComma = [](char16_t c) {
        return c == ',';
    };
    Vector<RefPtr<StringImpl>> targets;
    for (auto& target : Expected::searchTargets)
        targets.append(createString(target));
    auto samples = sampleStrings();
    for (size_t sample = 0; sample < samples.size(); ++sample) {
        SCOPED_TRACE(sample);
        auto& expected = Expected::search[sample];
        RefPtr<StringImpl> pointer = samples[sample];
        for (size_t startIndex = 0; startIndex < std::size(Expected::searchStarts); ++startIndex) {
            unsigned start = Expected::searchStarts[startIndex];
            auto& expectedFromStart = expected.fromStart[startIndex];
            EXPECT_EQ(expectedFromStart.findO, find(pointer, 'o', start));
            EXPECT_EQ(expectedFromStart.findO, find(pointer, u'o', start));
            EXPECT_EQ(expectedFromStart.findEAcute, find(pointer, Latin1Character { 0xE9 }, start));
            EXPECT_EQ(expectedFromStart.findWorld, find(pointer, "World"_s, start));
            EXPECT_EQ(expectedFromStart.findUpper, find(pointer, isASCIIUpper<char16_t>, start));
            EXPECT_EQ(expectedFromStart.findComma, find(pointer, isComma, start));
            EXPECT_EQ(expectedFromStart.reverseFindO, reverseFind(pointer, u'o', start));
            EXPECT_EQ(expectedFromStart.reverseFindOLiteral, reverseFind(pointer, "o"_s, start));
        }
        EXPECT_EQ(expected.other.reverseFindO, reverseFind(pointer, u'o'));
        EXPECT_EQ(expected.other.startsWithH, startsWith(pointer, u'H'));
        EXPECT_EQ(expected.other.endsWithD, endsWith(pointer, u'd'));
        EXPECT_EQ(expected.other.endsWithComma, endsWith(pointer, ','));
        EXPECT_EQ(expected.other.containsComma, contains(pointer, u','));
        EXPECT_EQ(expected.other.containsHello, contains(pointer, "Hello"_s));
        EXPECT_EQ(expected.other.containsDigit, contains(pointer, isASCIIDigit<char16_t>));
        for (size_t targetIndex = 0; targetIndex < targets.size(); ++targetIndex) {
            SCOPED_TRACE(targetIndex);
            auto& expectedForTarget = expected.forTarget[targetIndex];
            StringView target = targets[targetIndex];
            EXPECT_EQ(expectedForTarget.find, find(pointer, target));
            EXPECT_EQ(expectedForTarget.findFrom3, find(pointer, target, 3));
            EXPECT_EQ(expectedForTarget.findIgnoringASCIICase, findIgnoringASCIICase(pointer, target));
            EXPECT_EQ(expectedForTarget.findIgnoringASCIICaseFrom2, findIgnoringASCIICase(pointer, target, 2));
            EXPECT_EQ(expectedForTarget.reverseFind, reverseFind(pointer, target));
            EXPECT_EQ(expectedForTarget.reverseFindFrom4, reverseFind(pointer, target, 4));
            EXPECT_EQ(expectedForTarget.contains, contains(pointer, target));
            EXPECT_EQ(expectedForTarget.containsIgnoringASCIICase, containsIgnoringASCIICase(pointer, target));
            EXPECT_EQ(expectedForTarget.containsIgnoringASCIICaseFrom3, containsIgnoringASCIICase(pointer, target, 3));
            EXPECT_EQ(expectedForTarget.startsWith, startsWith(pointer, target));
            EXPECT_EQ(expectedForTarget.startsWithIgnoringASCIICase, startsWithIgnoringASCIICase(pointer, target));
            EXPECT_EQ(expectedForTarget.endsWith, endsWith(pointer, target));
            EXPECT_EQ(expectedForTarget.endsWithIgnoringASCIICase, endsWithIgnoringASCIICase(pointer, target));
            for (size_t positionIndex = 0; positionIndex < std::size(Expected::infixPositions); ++positionIndex) {
                unsigned position = Expected::infixPositions[positionIndex];
                EXPECT_EQ(expectedForTarget.hasInfixStartingAt[positionIndex], hasInfixStartingAt(pointer, target, position));
                EXPECT_EQ(expectedForTarget.hasInfixEndingAt[positionIndex], hasInfixEndingAt(pointer, target, position));
            }
            // Search for a string that is held by a RefPtr.
            RefPtr<StringImpl> targetPointer = targets[targetIndex];
            EXPECT_EQ(expectedForTarget.find, find(pointer, targetPointer));
            EXPECT_EQ(expectedForTarget.contains, contains(pointer, targetPointer));
        }
    }
}

TEST(WTF_StringFunctions, Transforms)
{
    RefPtr<AtomStringImpl> locales[] { nullptr, emptyAtom(), "tr"_atom, "lt-LT"_atom, "el"_atom };
    auto samples = sampleStrings();
    for (size_t sample = 0; sample < samples.size(); ++sample) {
        SCOPED_TRACE(sample);
        auto& expected = Expected::transforms[sample];
        RefPtr<StringImpl> pointer = samples[sample];
        for (size_t positionIndex = 0; positionIndex < std::size(Expected::transformPositions); ++positionIndex) {
            unsigned position = Expected::transformPositions[positionIndex];
            for (size_t lengthIndex = 0; lengthIndex < std::size(Expected::substringLengths); ++lengthIndex) {
                unsigned substringLength = Expected::substringLengths[lengthIndex];
                expectString(expected.substring[positionIndex][lengthIndex], substring(pointer, position, substringLength));
                expectString(expected.substring[positionIndex][lengthIndex], substringSharingImpl(pointer, position, substringLength));
            }
            expectString(expected.substringFrom[positionIndex], substring(pointer, position));
            expectString(expected.left[positionIndex], left(pointer, position));
            if (position <= length(pointer))
                expectString(expected.right[positionIndex], right(pointer, position));
        }
        expectString(expected.asciiLowercase, convertToASCIILowercase(pointer));
        expectString(expected.asciiUppercase, convertToASCIIUppercase(pointer));
        expectString(expected.lowercaseWithoutLocale, convertToLowercaseWithoutLocale(pointer));
        expectString(expected.uppercaseWithoutLocale, convertToUppercaseWithoutLocale(pointer));
        // These require the index of the first character that needs conversion.
        size_t firstUppercase = find(pointer, [](char16_t character) {
            return isASCIIUpper(character) || !isASCII(character);
        });
        size_t firstLowercase = find(pointer, [](char16_t character) {
            return isASCIILower(character) || !isASCII(character);
        });
        if (is8Bit(pointer)) {
            if (firstUppercase != notFound && isASCIIUpper(codeUnitAt(pointer, firstUppercase)))
                expectString(expected.lowercaseWithoutLocale, convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(pointer, firstUppercase));
            if (firstLowercase != notFound)
                expectString(expected.uppercaseWithoutLocale, convertToUppercaseWithoutLocaleStartingAtFailingIndex8Bit(pointer, firstLowercase));
        } else {
            if (firstUppercase != notFound)
                expectString(expected.lowercaseWithoutLocale, convertToLowercaseWithoutLocaleStartingAtFailingIndex16Bit(pointer, firstUppercase));
            if (firstLowercase != notFound)
                expectString(expected.uppercaseWithoutLocale, convertToUppercaseWithoutLocaleStartingAtFailingIndex16Bit(pointer, firstLowercase));
        }
        for (size_t localeIndex = 0; localeIndex < std::size(locales); ++localeIndex) {
            expectString(expected.lowercaseWithLocale[localeIndex], convertToLowercaseWithLocale(pointer, locales[localeIndex].get()));
            expectString(expected.uppercaseWithLocale[localeIndex], convertToUppercaseWithLocale(pointer, locales[localeIndex].get()));
        }
        expectString(expected.simplifiedWhiteSpace, simplifyWhiteSpace(pointer, isASCIIWhitespace));
        expectString(expected.trimmed, trim(pointer, isASCIIWhitespace));
        auto isSpace = [](char16_t character) {
            return isASCIIWhitespace(character);
        };
        auto isNotSpace = [](char16_t character) {
            return !isASCIIWhitespace(character);
        };
        expectString(expected.withoutSpaces, removeCharacters(pointer, isSpace));
        expectString(expected.onlySpaces, removeCharacters(pointer, isNotSpace));
        expectString(expected.foldedCase, foldCase(pointer));
        expectString(expected.replacedOWithZero, makeStringByReplacingAll(pointer, u'o', u'0'));
        expectString(expected.replacedOWithOO, makeStringByReplacingAll(pointer, u'o', "OO"_s));
        expectString(expected.replacedLWithEAcuteAMacron, makeStringByReplacingAll(pointer, u'l', StringView { u"éĀ"_str }));
        expectString(expected.replacedWorldWithThere, makeStringByReplacingAll(pointer, "World"_s, "There"_s));
        for (size_t positionIndex = 0; positionIndex < std::size(Expected::removalPositions); ++positionIndex) {
            unsigned position = Expected::removalPositions[positionIndex];
            for (size_t lengthIndex = 0; lengthIndex < std::size(Expected::removalLengths); ++lengthIndex) {
                unsigned lengthToRemove = Expected::removalLengths[lengthIndex];
                expectString(expected.removing[positionIndex][lengthIndex], makeStringByRemoving(pointer, position, lengthToRemove));
                if (pointer && position <= length(pointer))
                    expectString(expected.replacing[positionIndex][lengthIndex], makeStringByReplacing(pointer, position, std::min(lengthToRemove, length(pointer) - position), "<>"_s));
            }
        }
        RefPtr<StringImpl> actual16Bit = pointer;
        convertTo16Bit(actual16Bit);
        expectString(Expected::sampleContents[sample], actual16Bit);
        EXPECT_EQ(expected.is8BitAfterConvertTo16Bit, is8Bit(actual16Bit));
    }
}

TEST(WTF_StringFunctions, TransformsReturnSameImplWhenUnchanged)
{
    RefPtr<StringImpl> lowercase = StringImpl::create("abc def"_span8);
    EXPECT_EQ(lowercase.get(), substring(lowercase, 0).get());
    EXPECT_EQ(lowercase.get(), substring(lowercase, 0, 100).get());
    EXPECT_EQ(lowercase.get(), substringSharingImpl(lowercase, 0).get());
    EXPECT_EQ(lowercase.get(), convertToASCIILowercase(lowercase).get());
    EXPECT_EQ(lowercase.get(), trim(lowercase, isASCIIWhitespace).get());
    EXPECT_EQ(lowercase.get(), makeStringByRemoving(lowercase, 3, 0).get());
    EXPECT_EQ(lowercase.get(), makeStringByReplacingAll(lowercase, u'z', u'y').get());

    // The rvalue overload of substring steals the reference for the full range.
    const void* lowercaseIdentity = lowercase.get();
    RefPtr<StringImpl> moved = substring(WTF::move(lowercase), 0);
    EXPECT_EQ(lowercaseIdentity, moved.get());
    EXPECT_NULL(lowercase);
}

TEST(WTF_StringFunctions, AtomCaseConversion)
{
    auto samples = sampleStrings();
    for (size_t sample = 0; sample < samples.size(); ++sample) {
        SCOPED_TRACE(sample);
        auto& expected = Expected::atomCaseConversion[sample];
        RefPtr<AtomStringImpl> pointer = AtomStringImpl::add(samples[sample].get());
        EXPECT_EQ(!samples[sample], !pointer);
        auto lowercase = convertToASCIILowercase(pointer);
        auto uppercase = convertToASCIIUppercase(pointer);
        static_assert(std::is_same_v<decltype(lowercase), RefPtr<AtomStringImpl>>);
        static_assert(std::is_same_v<decltype(uppercase), RefPtr<AtomStringImpl>>);
        EXPECT_EQ(expectedAtom(expected.lowercase).get(), lowercase.get());
        EXPECT_EQ(expectedAtom(expected.uppercase).get(), uppercase.get());
        EXPECT_EQ(expectedAtom(expected.lowercase).get(), convertToASCIILowercase(pointer.get()).get());
        // Non-atom pointer-likes still return RefPtr<StringImpl>.
        static_assert(std::is_same_v<decltype(convertToASCIILowercase(RefPtr<StringImpl> { pointer })), RefPtr<StringImpl>>);
    }
    // Long strings take the non-local-buffer path.
    RefPtr<AtomStringImpl> longAtom = AtomStringImpl::add(makeString("A"_s, StringImpl::fromLatin1(std::string(150, 'b').c_str())).get());
    EXPECT_EQ(AtomStringImpl::add(makeString("a"_s, StringImpl::fromLatin1(std::string(150, 'b').c_str())).get()).get(), convertToASCIILowercase(longAtom).get());
    EXPECT_EQ(AtomStringImpl::add(makeString("A"_s, StringImpl::fromLatin1(std::string(150, 'B').c_str())).get()).get(), convertToASCIIUppercase(longAtom).get());
}

TEST(WTF_StringFunctions, Split)
{
    auto samples = sampleStrings();
    for (size_t sample = 0; sample < samples.size(); ++sample) {
        SCOPED_TRACE(sample);
        auto& expected = Expected::split[sample];
        RefPtr<StringImpl> pointer = samples[sample];
        for (size_t separatorIndex = 0; separatorIndex < std::size(Expected::splitCharacters); ++separatorIndex) {
            char16_t separator = Expected::splitCharacters[separatorIndex];
            expectVector(expected.byCharacter[separatorIndex], split(pointer, separator));
            expectVector(expected.byCharacterAllowingEmptyEntries[separatorIndex], splitAllowingEmptyEntries(pointer, separator));
            Vector<RefPtr<StringImpl>> actual;
            split(pointer, separator, [&](StringView item) {
                actual.append(item.toString());
            });
            expectVector(expected.byCharacter[separatorIndex], actual);
            actual.clear();
            splitAllowingEmptyEntries(pointer, separator, [&](StringView item) {
                actual.append(item.toString());
            });
            expectVector(expected.byCharacterAllowingEmptyEntries[separatorIndex], actual);
        }
        for (size_t separatorIndex = 0; separatorIndex < std::size(Expected::splitStrings); ++separatorIndex) {
            auto separatorString = createString(Expected::splitStrings[separatorIndex]);
            StringView separator { separatorString };
            expectVector(expected.byString[separatorIndex], split(pointer, separator));
            expectVector(expected.byStringAllowingEmptyEntries[separatorIndex], splitAllowingEmptyEntries(pointer, separator));
        }
    }
}

TEST(WTF_StringFunctions, NumberParsing)
{
    auto inputs = sampleStrings();
    for (auto& extra : Expected::numberParsingExtraInputs)
        inputs.append(createString(extra));
    ASSERT_EQ(Expected::numberParsing.size(), inputs.size());
    for (size_t input = 0; input < inputs.size(); ++input) {
        SCOPED_TRACE(input);
        auto& expected = Expected::numberParsing[input];
        RefPtr<StringImpl> pointer = inputs[input];
        bool ok = true;
        double actualDouble = toDouble(pointer, &ok);
        EXPECT_EQ(expected.toDoubleOK, ok);
        if (!std::isnan(expected.toDouble)) {
            EXPECT_EQ(expected.toDouble, actualDouble);
            EXPECT_EQ(expected.toDouble, toDouble(pointer));
        } else
            EXPECT_TRUE(std::isnan(actualDouble));
        ok = true;
        float actualFloat = toFloat(pointer, &ok);
        EXPECT_EQ(expected.toFloatOK, ok);
        if (!std::isnan(expected.toFloat))
            EXPECT_EQ(expected.toFloat, actualFloat);
        else
            EXPECT_TRUE(std::isnan(actualFloat));
    }
    bool ok = true;
    EXPECT_EQ(0.0, toDouble(RefPtr<StringImpl> { }, &ok));
    EXPECT_FALSE(ok);
}

TEST(WTF_StringFunctions, IsolatedCopy)
{
    // Null stays null.
    EXPECT_NULL(isolatedCopy(RefPtr<StringImpl> { }));
    RefPtr<StringImpl> null;
    EXPECT_NULL(isolatedCopy(null));
    EXPECT_TRUE(isSafeToSendToAnotherThread(null));

    // Empty is the static empty string.
    RefPtr<StringImpl> empty = emptyString();
    EXPECT_TRUE(isSafeToSendToAnotherThread(empty));
    static_assert(Expected::isolatedCopyOfEmptyStringIsSame);
    EXPECT_EQ(empty.get(), isolatedCopy(empty).get());

    // Unique, non-atom: the rvalue version steals.
    RefPtr<StringImpl> unique = StringImpl::create("unique string"_span8);
    EXPECT_TRUE(isSafeToSendToAnotherThread(unique));
    const void* uniqueIdentity = unique.get();
    auto stolen = isolatedCopy(WTF::move(unique));
    EXPECT_EQ(uniqueIdentity, stolen.get());
    EXPECT_NULL(unique);

    // Unique, const&: always copies, like String::isolatedCopy() const& did.
    auto copied = isolatedCopy(stolen);
    EXPECT_NE(stolen.get(), copied.get());
    EXPECT_TRUE(equal(stolen, copied));

    // Shared: the rvalue version copies.
    RefPtr<StringImpl> shared = StringImpl::create("shared string"_span8);
    RefPtr<StringImpl> otherReference = shared;
    EXPECT_FALSE(isSafeToSendToAnotherThread(shared));
    const void* sharedIdentity = shared.get();
    auto sharedCopy = isolatedCopy(WTF::move(shared));
    EXPECT_NE(sharedIdentity, sharedCopy.get());
    EXPECT_TRUE(equal(otherReference, sharedCopy));

    // Atom: never stolen, the copy is not an atom.
    RefPtr<StringImpl> atom = AtomStringImpl::add("atom string for isolatedCopy"_s).ptr();
    EXPECT_FALSE(isSafeToSendToAnotherThread(atom));
    const void* atomIdentity = atom.get();
    auto atomCopy = isolatedCopy(WTF::move(atom));
    EXPECT_NE(atomIdentity, atomCopy.get());
    EXPECT_FALSE(atomCopy->isAtom());

    // The sample strings.
    auto samples = sampleStrings();
    for (size_t sample = 0; sample < samples.size(); ++sample) {
        SCOPED_TRACE(sample);
        RefPtr<StringImpl> pointer = samples[sample];
        EXPECT_EQ(Expected::isSafeToSendToAnotherThread[sample], isSafeToSendToAnotherThread(pointer));
        auto copy = isolatedCopy(pointer);
        expectString(Expected::sampleContents[sample], copy);
        if (copy)
            EXPECT_TRUE(isSafeToSendToAnotherThread(copy));
    }
}

TEST(WTF_StringFunctions, InPlaceOperations)
{
    RefPtr<StringImpl> string = StringImpl::create("abc"_span8);
    convertTo16Bit(string);
    EXPECT_FALSE(string->is8Bit());
    EXPECT_TRUE(equal(string, "abc"_s));
    RefPtr<StringImpl> null;
    convertTo16Bit(null);
    EXPECT_NULL(null);

    clearImplIfNotShared(string);
    EXPECT_NULL(string);
    RefPtr<StringImpl> shared = StringImpl::create("shared"_span8);
    RefPtr<StringImpl> other = shared;
    clearImplIfNotShared(shared);
    EXPECT_NOT_NULL(shared);
}

TEST(WTF_StringFunctions, ToExistingAtomString)
{
    auto samples = sampleStrings();
    for (size_t sample = 0; sample < samples.size(); ++sample) {
        SCOPED_TRACE(sample);
        RefPtr<StringImpl> pointer = samples[sample];
        // Which strings are atoms depends on what ran before; the result is the atom with the same contents, if any.
        RefPtr<AtomStringImpl> expected;
        if (isEmpty(pointer))
            expected = pointer ? emptyAtom() : nullptr;
        else
            expected = AtomStringImpl::lookUp(expectedSpan(Expected::sampleContents[sample]));
        EXPECT_EQ(expected.get(), toExistingAtomString(pointer).get());
    }
    EXPECT_EQ(samples[5].get(), toExistingAtomString(samples[5]).get());
    EXPECT_EQ(emptyAtom().get(), toExistingAtomString(samples[1]).get());
    RefPtr<StringImpl> neverAtomized = StringImpl::create("this string is never atomized 0123456789"_span8);
    EXPECT_NULL(toExistingAtomString(neverAtomized));
    RefPtr<AtomStringImpl> atom = AtomStringImpl::add("this string is atomized 0123456789"_s).ptr();
    RefPtr<StringImpl> nonAtomCopy = StringImpl::create("this string is atomized 0123456789"_span8);
    EXPECT_EQ(atom.get(), toExistingAtomString(nonAtomCopy).get());
}

static bool expectedRelation(std::string_view row, size_t column)
{
    return row[column] == '1';
}

TEST(WTF_StringFunctions, Equality)
{
    auto samples = sampleStrings();
    for (size_t aIndex = 0; aIndex < samples.size(); ++aIndex) {
        SCOPED_TRACE(aIndex);
        auto& expected = Expected::equality[aIndex];
        for (size_t bIndex = 0; bIndex < samples.size(); ++bIndex) {
            SCOPED_TRACE(bIndex);
            RefPtr<StringImpl> aPointer = samples[aIndex];
            RefPtr<StringImpl> bPointer = isolatedCopy(samples[bIndex]);
            EXPECT_EQ(expectedRelation(expected.equal, bIndex), equal(aPointer, bPointer));
            EXPECT_EQ(expectedRelation(expected.equal, bIndex), equal(aPointer.get(), bPointer));
            EXPECT_EQ(expectedRelation(expected.equal, bIndex), equal(aPointer, bPointer.get()));
            EXPECT_EQ(expectedRelation(expected.viewsEqual, bIndex), equal(aPointer, StringView(bPointer)));
            EXPECT_EQ(expectedRelation(expected.viewsEqual, bIndex), equal(StringView(aPointer), bPointer));
            EXPECT_EQ(expectedRelation(expected.equalIgnoringNullity, bIndex), equalIgnoringNullity(aPointer, bPointer));
            EXPECT_EQ(expectedRelation(expected.equalIgnoringASCIICase, bIndex), equalIgnoringASCIICase(aPointer, bPointer));
            EXPECT_EQ(expectedRelation(expected.viewsEqualIgnoringASCIICase, bIndex), equalIgnoringASCIICase(aPointer, StringView(bPointer)));
            EXPECT_EQ(expectedRelation(expected.viewsEqualIgnoringASCIICase, bIndex), equalIgnoringASCIICase(StringView(aPointer), bPointer));
            EXPECT_EQ(aIndex == bIndex, isSameImpl(aPointer, samples[bIndex]));
        }
        RefPtr<StringImpl> pointer = samples[aIndex];
        EXPECT_EQ(expected.equalsHelloWorld, equal(pointer, "Hello, World"_s));
        EXPECT_EQ(expected.equalsHelloWorld, equal("Hello, World"_s, pointer));
        EXPECT_EQ(expected.equalsEmptyLiteral, equal(pointer, ""_s));
        EXPECT_EQ(expected.equalsHelloWorld, equal(pointer, "Hello, World"));
        EXPECT_EQ(expected.equalsHelloWorld, equal(pointer.get(), "Hello, World"));
        EXPECT_EQ(expected.equalsHelloWorldIgnoringASCIICase, equalIgnoringASCIICase(pointer, "hello, world"_s));
        EXPECT_EQ(expected.equalsLowercaseOnlyLetters, equalLettersIgnoringASCIICase(pointer, "lowercase only"_s));
        EXPECT_EQ(expected.startsWithHelloLetters, startsWithLettersIgnoringASCIICase(pointer, "hello"_s));
        if (is8Bit(pointer))
            EXPECT_EQ(expected.equalsOwnSpan, equal(pointer, span8(pointer)));
        else
            EXPECT_EQ(expected.equalsOwnSpan, equal(pointer, span16(pointer)));
    }

    // Nullity is respected, like String.
    RefPtr<StringImpl> null;
    RefPtr<StringImpl> empty = emptyString();
    EXPECT_FALSE(equal(null, empty));
    EXPECT_TRUE(equal(null, RefPtr<StringImpl> { }));
    EXPECT_TRUE(equalIgnoringNullity(null, empty));
    EXPECT_FALSE(equal(null, ""_s));

    // Mixed types.
    RefPtr<AtomStringImpl> atomPointer = AtomStringImpl::add("Hello, World"_s).ptr();
    Ref<StringImpl> ref = StringImpl::create("Hello, World"_span8);
    EXPECT_TRUE(equal(atomPointer, ref));
    EXPECT_TRUE(equal(ref, atomPointer.get()));
    EXPECT_FALSE(isSameImpl(atomPointer, ref));
    EXPECT_TRUE(isSameImpl(atomPointer, AtomStringImpl::add("Hello, World"_s).ptr()));
}

TEST(WTF_StringFunctions, StringImplFactories)
{
    std::span<const char8_t> nullSpan;
    expectString(Expected::N, StringImpl::fromUTF8(nullSpan));
    expectString(Expected::N, StringImpl::fromUTF8(byteCast<Latin1Character>(nullSpan)));
    expectString(Expected::N, StringImpl::fromUTF8(byteCast<uint8_t>(nullSpan)));
    expectString(Expected::N, StringImpl::fromUTF8(byteCast<char>(nullSpan)));
    expectString(Expected::N, StringImpl::fromUTF8ReplacingInvalidSequences(nullSpan));
    expectString(Expected::N, StringImpl::fromUTF8WithLatin1Fallback(nullSpan));
    using UTF8Case = std::pair<std::span<const char8_t>, std::u16string_view>;
    for (auto [input, expected] : std::initializer_list<UTF8Case> { { u8""_span, u""sv }, { u8"plain ASCII"_span, u"plain ASCII"sv }, { u8"café \U0001F600"_span, u"café \U0001F600"sv } }) {
        expectString(expected, StringImpl::fromUTF8(input));
        expectString(expected, StringImpl::fromUTF8(byteCast<Latin1Character>(input)));
        expectString(expected, StringImpl::fromUTF8(byteCast<uint8_t>(input)));
        expectString(expected, StringImpl::fromUTF8(byteCast<char>(input)));
        expectString(expected, StringImpl::fromUTF8ReplacingInvalidSequences(input));
        expectString(expected, StringImpl::fromUTF8WithLatin1Fallback(input));
    }
    std::array<uint8_t, 4> invalidUTF8 { 'a', 0xFF, 0xC3, 'b' };
    std::span<const uint8_t> invalid { invalidUTF8 };
    EXPECT_NULL(StringImpl::fromUTF8(invalid));
    expectString(u"a\xFFFD\xFFFD" u"b"sv, StringImpl::fromUTF8ReplacingInvalidSequences(invalid));
    expectString(u"a\x00FF\x00C3" u"b"sv, StringImpl::fromUTF8WithLatin1Fallback(invalid));
    expectString(u"c string"sv, StringImpl::fromUTF8("c string"));
    expectString(Expected::N, StringImpl::fromUTF8(static_cast<const char*>(nullptr)));

    expectString(u"caf\x00E9"sv, StringImpl::fromLatin1("caf\xE9"));
    expectString(u"caf\x00E9"sv, StringImpl::fromLatin1(std::span<const char> { "caf\xE9", 4 }));
    expectString(Expected::N, StringImpl::fromLatin1(static_cast<const char*>(nullptr)));
    expectString(Expected::N, StringImpl::fromLatin1(std::span<const char> { }));
    expectString(u""sv, StringImpl::fromLatin1(""_span));

    expectString(u"A"sv, StringImpl::fromCodePoint(0x41));
    expectString(u"\x00E9"sv, StringImpl::fromCodePoint(0xE9));
    expectString(u"\U0001F600"sv, StringImpl::fromCodePoint(0x1F600));
    EXPECT_NULL(StringImpl::fromCodePoint(0x110000));

    RefPtr<StringImpl> sixteenBit = u"abcé"_str;
    expectString(u"abc\x00E9"sv, StringImpl::make8Bit(span16(sixteenBit)));
    EXPECT_TRUE(StringImpl::make8Bit(span16(sixteenBit))->is8Bit());
    expectString(u""sv, StringImpl::make8Bit(std::span<const char16_t> { }));

    struct NumberExpectations {
        double number;
        std::u16string_view fixedPrecision;
        std::u16string_view fixedPrecision3KeepingTrailingZeros;
        std::u16string_view floatFixedPrecision;
        std::u16string_view fixedWidth2;
    };
    for (auto& expected : std::initializer_list<NumberExpectations> {
        { 0.0, u"0"sv, u"0.00"sv, u"0"sv, u"0.00"sv },
        { 1.5, u"1.5"sv, u"1.50"sv, u"1.5"sv, u"1.50"sv },
        { -1234.5678, u"-1234.57"sv, u"-1.23e+3"sv, u"-1234.57"sv, u"-1234.57"sv },
        { 1e21, u"1e+21"sv, u"1.00e+21"sv, u"1e+21"sv, u""sv },
        { 3.14159265358979, u"3.14159"sv, u"3.14"sv, u"3.14159"sv, u"3.14"sv },
    }) {
        SCOPED_TRACE(expected.number);
        expectString(expected.fixedPrecision, StringImpl::numberToStringFixedPrecision(expected.number));
        expectString(expected.fixedPrecision3KeepingTrailingZeros, StringImpl::numberToStringFixedPrecision(expected.number, 3, TrailingZerosPolicy::Keep));
        expectString(expected.floatFixedPrecision, StringImpl::numberToStringFixedPrecision(static_cast<float>(expected.number)));
        expectString(expected.fixedWidth2, StringImpl::numberToStringFixedWidth(expected.number, 2));
    }

#if USE(CF)
    EXPECT_NULL(StringImpl::create(static_cast<CFStringRef>(nullptr)));
    expectString(u""sv, StringImpl::create(CFSTR("")));
    expectString(u"ASCII CF string"sv, StringImpl::create(CFSTR("ASCII CF string")));
    RetainPtr nonLatin1 = adoptCF(CFStringCreateWithCharacters(nullptr, reinterpret_cast<const UniChar*>(u"Стр"), 3));
    expectString(u"Стр"sv, StringImpl::create(nonLatin1.get()));
    RetainPtr latin1 = adoptCF(CFStringCreateWithCString(nullptr, "caf\xE9", kCFStringEncodingISOLatin1));
    expectString(u"caf\x00E9"sv, StringImpl::create(latin1.get()));

    auto samples = sampleStrings();
    for (size_t sample = 0; sample < samples.size(); ++sample) {
        SCOPED_TRACE(sample);
        // Null converts to an empty CFString, like String::createCFString() did.
        auto content = expectedSpan(Expected::sampleContents[sample]);
        RetainPtr expected = adoptCF(CFStringCreateWithCharacters(nullptr, reinterpret_cast<const UniChar*>(content.data()), content.size()));
        auto actual = createCFString(samples[sample]);
        EXPECT_NOT_NULL(actual.get());
        EXPECT_TRUE(CFEqual(expected.get(), actual.get()));
    }
#endif
}

TEST(WTF_StringFunctions, AtomStringImplFactories)
{
    EXPECT_EQ(AtomStringImpl::add("42"_s).ptr(), AtomStringImpl::number(42).get());
    EXPECT_EQ(AtomStringImpl::add("-42"_s).ptr(), AtomStringImpl::number(-42).get());
    EXPECT_EQ(AtomStringImpl::add("42"_s).ptr(), AtomStringImpl::number(42u).get());
    EXPECT_EQ(AtomStringImpl::add("42"_s).ptr(), AtomStringImpl::number(42ul).get());
    EXPECT_EQ(AtomStringImpl::add("42"_s).ptr(), AtomStringImpl::number(42ull).get());
    EXPECT_EQ(AtomStringImpl::add("1.5"_s).ptr(), AtomStringImpl::number(1.5f).get());
    EXPECT_EQ(AtomStringImpl::add("-2.25"_s).ptr(), AtomStringImpl::number(-2.25).get());
    EXPECT_EQ(AtomStringImpl::add("-2147483648"_s).ptr(), AtomStringImpl::number(std::numeric_limits<int>::min()).get());
    EXPECT_TRUE(AtomStringImpl::number(42)->isAtom());

    EXPECT_EQ(AtomStringImpl::add("hello"_s).ptr(), AtomStringImpl::fromUTF8("hello").get());
    EXPECT_EQ(emptyAtom().get(), AtomStringImpl::fromUTF8("").get());
    EXPECT_NULL(AtomStringImpl::fromUTF8(static_cast<const char*>(nullptr)));
    EXPECT_NULL(AtomStringImpl::fromUTF8(std::span<const char> { }));
    std::span<const char> validUTF8 { "caf\xC3\xA9", 5 };
    std::span<const char> invalidUTF8 { "bad \xFF", 5 };
    EXPECT_EQ(AtomStringImpl::add(std::span<const char16_t> { u"caf\x00E9"sv }).get(), AtomStringImpl::fromUTF8(validUTF8).get());
    EXPECT_NULL(AtomStringImpl::fromUTF8(invalidUTF8));
}

TEST(WTF_StringFunctions, StringViewFromPointers)
{
    RefPtr<StringImpl> null;
    StringView nullView = null;
    EXPECT_TRUE(nullView.isNull());

    RefPtr<StringImpl> string = StringImpl::create("view me"_span8);
    StringView view = string;
    EXPECT_FALSE(view.isNull());
    EXPECT_EQ(view.span8().data(), string->span8().data());

    Ref<StringImpl> ref = *string;
    StringView refView = ref;
    EXPECT_EQ(refView.span8().data(), string->span8().data());

    RefPtr<AtomStringImpl> atom = AtomStringImpl::add("atom view"_s);
    StringView atomView = atom;
    EXPECT_TRUE(atomView == "atom view"_s);

    RefPtr<StringImpl> empty = emptyString();
    StringView emptyView = empty;
    EXPECT_FALSE(emptyView.isNull());
    EXPECT_TRUE(emptyView.isEmpty());

    // toString() has the same nullness as toString() had.
    EXPECT_NULL(StringView().toString());
    EXPECT_NOT_NULL(StringView(""_s).toString());
    RefPtr<StringImpl> sixteenBit = u"été"_str;
    expectString(u"été"sv, StringView(sixteenBit).toString());
    EXPECT_EQ(AtomStringImpl::add("atomized view"_s).ptr(), StringView("atomized view"_s).toAtomString().get());
}

TEST(WTF_StringFunctions, StaticStringImplLiteral)
{
    auto first = "static literal"_si;
    auto second = "static literal"_si;
    static_assert(std::is_same_v<decltype(first), RefPtr<StringImpl>>);
    EXPECT_EQ(first.get(), second.get());
    EXPECT_TRUE(first->isStatic());
    EXPECT_FALSE(first->isAtom());
    EXPECT_TRUE(first->is8Bit());
    EXPECT_EQ(14u, first->length());
    EXPECT_TRUE(equal(first, "static literal"_s));
    EXPECT_TRUE(first->hasHash());
    EXPECT_EQ(StringHasher::computeHashAndMaskTop8Bits("static literal"_span8), first->existingHash());
    EXPECT_EQ(StringImpl::create("static literal"_span8)->hash(), first->hash());

    auto other = "another literal"_si;
    EXPECT_NE(first.get(), other.get());

    auto empty = ""_si;
    EXPECT_NOT_NULL(empty);
    EXPECT_TRUE(isEmpty(empty));
    EXPECT_TRUE(empty->isStatic());

    // Static strings don't get destroyed when the last RefPtr goes away.
    {
        auto scoped = "scoped literal"_si;
    }
    EXPECT_TRUE(equal("scoped literal"_si, "scoped literal"_s));

    // Can be passed to StringImpl* parameters.
    auto takesPointer = [](StringImpl* string) {
        return length(string);
    };
    EXPECT_EQ(3u, takesPointer("abc"_si));
}

TEST(WTF_StringFunctions, AtomLiteral)
{
    auto atom = "atom literal"_atom;
    static_assert(std::is_same_v<decltype(atom), RefPtr<AtomStringImpl>>);
    EXPECT_TRUE(atom->isAtom());
    EXPECT_TRUE(equal(atom, "atom literal"_s));
    EXPECT_EQ(AtomStringImpl::add("atom literal"_s).ptr(), atom.get());
    EXPECT_EQ(atom.get(), "atom literal"_atom.get());
    EXPECT_EQ(emptyAtom().get(), ""_atom.get());
}

static RefPtr<StringImpl> returnLiteralAsStringImpl() { return "returned"_s; }
static std::optional<RefPtr<StringImpl>> returnLiteralAsOptional() { return "optional"_s; }

TEST(WTF_StringFunctions, ASCIILiteralConversions)
{
    RefPtr<StringImpl> converted = "converted"_s;
    EXPECT_TRUE(equal(converted, "converted"_s));
    EXPECT_FALSE(converted->isAtom());
    RefPtr<StringImpl> nullConverted = ASCIILiteral { };
    EXPECT_NULL(nullConverted);
    RefPtr<StringImpl> emptyConverted = ""_s;
    EXPECT_NOT_NULL(emptyConverted);
    EXPECT_TRUE(isEmpty(emptyConverted));
    expectString(u""sv, emptyConverted);

    Vector<RefPtr<StringImpl>> vector;
    vector.append("appended"_s);
    EXPECT_TRUE(equal(vector[0], "appended"_s));
    EXPECT_TRUE(equal(returnLiteralAsStringImpl(), "returned"_s));
    EXPECT_TRUE(equal(*returnLiteralAsOptional(), "optional"_s));

    // The atom conversion is explicit, so it works in direct-initialization only.
    static_assert(!std::is_convertible_v<ASCIILiteral, RefPtr<AtomStringImpl>>);
    static_assert(std::is_constructible_v<RefPtr<AtomStringImpl>, ASCIILiteral>);
    static_assert(std::is_convertible_v<ASCIILiteral, RefPtr<StringImpl>>);
    RefPtr<AtomStringImpl> atom("atomized"_s);
    EXPECT_TRUE(atom->isAtom());
    EXPECT_EQ(AtomStringImpl::add("atomized"_s).ptr(), atom.get());
    RefPtr<AtomStringImpl> emptyAtomPointer(""_s);
    EXPECT_EQ(emptyAtom().get(), emptyAtomPointer.get());
    RefPtr<AtomStringImpl> nullAtomPointer(ASCIILiteral { });
    EXPECT_NULL(nullAtomPointer);
    Vector<RefPtr<AtomStringImpl>> atomVector;
    atomVector.append("appended atom"_s);
    EXPECT_EQ(AtomStringImpl::add("appended atom"_s).ptr(), atomVector[0].get());

    static NeverDestroyed<const RefPtr<AtomStringImpl>> staticAtom("static atom"_s);
    EXPECT_EQ(AtomStringImpl::add("static atom"_s).ptr(), staticAtom.get().get());

    HashSet<RefPtr<StringImpl>> set;
    set.add("in set"_s);
    EXPECT_TRUE(set.contains(StringImpl::create("in set"_span8).ptr()));
}

TEST(WTF_StringFunctions, Joining)
{
    RefPtr<StringImpl> sixteenBit = u"é"_str;
    Vector<RefPtr<StringImpl>> pieces { "a"_si, nullptr, sixteenBit, emptyString(), "z"_si };
    expectString(u"a, , \x00E9, , z"sv, makeStringByJoining(pieces.span(), ", "_si.get()));
    expectString(u"a\x00E9z"sv, makeStringByJoining(pieces.span(), nullptr));
    expectString(u""sv, makeStringByJoining(std::span<const RefPtr<StringImpl>> { }, ","_si.get()));
    EXPECT_NOT_NULL(makeStringByJoining(std::span<const RefPtr<StringImpl>> { }, nullptr));
    Vector<RefPtr<StringImpl>> latin1Pieces { "abc"_si, "def"_si };
    EXPECT_TRUE(makeStringByJoining(latin1Pieces.span(), "-"_si.get())->is8Bit());
}

TEST(WTF_StringFunctions, Hashing)
{
    // Content semantics for RefPtr<StringImpl> keys.
    HashMap<RefPtr<StringImpl>, int> map;
    map.add(StringImpl::create("key"_span8), 1);
    EXPECT_EQ(1, map.get(RefPtr { StringImpl::create("key"_span8) }));
    EXPECT_TRUE(map.contains(StringImpl::create("key"_span8).ptr()));
    EXPECT_EQ(1, map.get("key"_si));
    EXPECT_FALSE(map.contains(RefPtr { StringImpl::create("other"_span8) }));

    // Identity semantics for RefPtr<AtomStringImpl> keys, with the content-derived existing hash.
    static_assert(std::is_base_of_v<WTF::AtomStringImplHash, DefaultHash<RefPtr<AtomStringImpl>>>);
    static_assert(DefaultHash<RefPtr<AtomStringImpl>>::hasHashInValue);
    HashMap<RefPtr<AtomStringImpl>, int> atomMap;
    RefPtr<AtomStringImpl> atom = AtomStringImpl::add("atom key"_s);
    atomMap.add(atom, 2);
    EXPECT_EQ(2, atomMap.get(RefPtr { AtomStringImpl::add("atom key"_s) }));
    EXPECT_EQ(2, atomMap.get(atom.get()));
    EXPECT_EQ(atom->existingHash(), DefaultHash<RefPtr<AtomStringImpl>>::hash(atom));
    EXPECT_EQ(StringHasher::computeHashAndMaskTop8Bits("atom key"_span8), DefaultHash<RefPtr<AtomStringImpl>>::hash(atom));
    HashSet<Ref<AtomStringImpl>> atomRefSet;
    atomRefSet.add(*atom);
    EXPECT_TRUE(atomRefSet.contains(atom.get()));
    MemoryCompactRobinHoodHashMap<RefPtr<AtomStringImpl>, int> robinHoodMap;
    robinHoodMap.add(atom, 3);
    EXPECT_EQ(3, robinHoodMap.get(atom));

    // Get on maps with string values returns an owning pointer.
    HashMap<int, RefPtr<StringImpl>> valueMap;
    valueMap.add(1, StringImpl::create("value"_span8));
    static_assert(std::is_same_v<decltype(valueMap.get(1)), RefPtr<StringImpl>>);
    EXPECT_TRUE(equal(valueMap.get(1), "value"_s));
    EXPECT_NULL(valueMap.get(2));
    HashMap<int, RefPtr<AtomStringImpl>> atomValueMap;
    atomValueMap.add(1, atom);
    static_assert(std::is_same_v<decltype(atomValueMap.get(1)), RefPtr<AtomStringImpl>>);
    EXPECT_EQ(atom.get(), atomValueMap.get(1).get());
    EXPECT_NULL(atomValueMap.get(2));

    // StringHash, AtomStringHash and ASCIICaseInsensitiveHash with string pointer keys.
    EXPECT_EQ(StringHasher::computeHashAndMaskTop8Bits("atom key"_span8), StringHash::hash(atom));
    EXPECT_EQ(StringHash::hash(StringImpl::create("atom key"_span8).ptr()), StringHash::hash(atom));
    EXPECT_TRUE(StringHash::equal(atom, RefPtr<AtomStringImpl> { atom }));
    EXPECT_EQ(atom->existingHash(), AtomStringHash::hash(atom));
    EXPECT_TRUE(AtomStringHash::equal(atom, atom.get()));
    EXPECT_EQ(ASCIICaseInsensitiveHash::hash(StringImpl::create("ATOM KEY"_span8).ptr()), ASCIICaseInsensitiveHash::hash(atom));
    HashSet<RefPtr<StringImpl>, ASCIICaseInsensitiveHash> caseInsensitiveSet;
    caseInsensitiveSet.add(StringImpl::create("MiXeD"_span8));
    EXPECT_TRUE(caseInsensitiveSet.contains(RefPtr { StringImpl::create("mixed"_span8) }));
    HashSet<RefPtr<AtomStringImpl>, ASCIICaseInsensitiveHash> caseInsensitiveAtomSet;
    caseInsensitiveAtomSet.add(AtomStringImpl::add("MiXeD"_s));
    EXPECT_TRUE(caseInsensitiveAtomSet.contains(AtomStringImpl::add("mixed"_s)));

    // Hasher: strings hash their characters, with the same values as for String.
    auto samples = sampleStrings();
    for (size_t sample = 0; sample < samples.size(); ++sample) {
        SCOPED_TRACE(sample);
        unsigned expected = Expected::computedHashes[sample];
        EXPECT_EQ(expected, computeHash(samples[sample]));
        if (samples[sample])
            EXPECT_EQ(expected, computeHash(Ref { *samples[sample] }));
        // Atoms hash their address.
        RefPtr<AtomStringImpl> atomString = AtomStringImpl::add(samples[sample].get());
        EXPECT_EQ(computeHash(std::bit_cast<uintptr_t>(atomString.get())), computeHash(atomString));
    }
    EXPECT_EQ(computeHash(StringImpl::create("same"_span8)), computeHash(RefPtr<StringImpl> { StringImpl::create("same"_span8) }));
}

TEST(WTF_StringFunctions, Translators)
{
    HashSet<RefPtr<StringImpl>> set;
    auto result = set.add<StringViewHashTranslator>(StringView { "from view"_s });
    EXPECT_TRUE(result.isNewEntry);
    EXPECT_TRUE(equal(*result.iterator, "from view"_s));
    EXPECT_TRUE((*result.iterator)->hasHash());
    EXPECT_TRUE(set.contains<StringViewHashTranslator>(StringView { "from view"_s }));
    EXPECT_FALSE(set.contains<StringViewHashTranslator>(StringView { "other"_s }));
    EXPECT_TRUE(set.contains(RefPtr { StringImpl::create("from view"_span8) }));
    EXPECT_FALSE(set.add<StringViewHashTranslator>(StringView { "from view"_s }).isNewEntry);

    auto literalResult = set.add<HashTranslatorASCIILiteral>("from literal"_s);
    EXPECT_TRUE(literalResult.isNewEntry);
    EXPECT_TRUE(set.contains<HashTranslatorASCIILiteral>("from literal"_s));
    EXPECT_TRUE(set.contains(RefPtr { StringImpl::create("from literal"_span8) }));

    HashSet<RefPtr<StringImpl>, ASCIICaseInsensitiveHash> caseInsensitiveSet;
    caseInsensitiveSet.add<ASCIICaseInsensitiveStringViewHashTranslator>(StringView { "Case Insensitive"_s });
    EXPECT_TRUE(caseInsensitiveSet.contains<ASCIICaseInsensitiveStringViewHashTranslator>(StringView { "CASE insensitive"_s }));
    EXPECT_TRUE(caseInsensitiveSet.contains<HashTranslatorASCIILiteralCaseInsensitive>("case INSENSITIVE"_s));
    EXPECT_FALSE(caseInsensitiveSet.contains<HashTranslatorASCIILiteralCaseInsensitive>("case sensitive"_s));

    HashMap<RefPtr<StringImpl>, int> map;
    map.add(StringImpl::create("mapped"_span8), 5);
    EXPECT_TRUE(map.find<StringViewHashTranslator>(StringView { "mapped"_s }) != map.end());
    EXPECT_TRUE(map.contains<HashTranslatorASCIILiteral>("mapped"_s));
}

TEST(WTF_StringFunctions, CrossThreadCopier)
{
    static_assert(std::is_same_v<CrossThreadCopier<RefPtr<StringImpl>>::Type, RefPtr<StringImpl>>);
    static_assert(std::is_same_v<CrossThreadCopier<RefPtr<AtomStringImpl>>::Type, RefPtr<StringImpl>>);
    static_assert(CrossThreadCopier<RefPtr<StringImpl>>::IsNeeded);

    EXPECT_NULL(crossThreadCopy(RefPtr<StringImpl> { }));
    EXPECT_NULL(crossThreadCopy(RefPtr<AtomStringImpl> { }));

    RefPtr<StringImpl> shared = StringImpl::create("shared"_span8);
    auto copy = crossThreadCopy(shared);
    EXPECT_NE(shared.get(), copy.get());
    EXPECT_TRUE(equal(shared, copy));

    RefPtr<StringImpl> unique = StringImpl::create("unique"_span8);
    const void* uniqueIdentity = unique.get();
    auto moved = crossThreadCopy(WTF::move(unique));
    EXPECT_EQ(uniqueIdentity, moved.get());

    RefPtr<AtomStringImpl> atom = AtomStringImpl::add("cross thread atom"_s);
    RefPtr<StringImpl> atomCopy = crossThreadCopy(atom);
    EXPECT_NE(static_cast<StringImpl*>(atom.get()), atomCopy.get());
    EXPECT_FALSE(atomCopy->isAtom());
    EXPECT_TRUE(isSafeToSendToAnotherThread(atomCopy));
    expectString(u"cross thread atom"sv, atomCopy);

    Ref<StringImpl> ref = StringImpl::create("ref"_span8);
    auto refCopy = crossThreadCopy(ref);
    EXPECT_NE(ref.ptr(), refCopy.ptr());
    EXPECT_TRUE(equal(ref, refCopy));

    Vector<RefPtr<StringImpl>> vector { StringImpl::create("element"_span8) };
    auto vectorCopy = crossThreadCopy(vector);
    EXPECT_NE(vector[0].get(), vectorCopy[0].get());
    EXPECT_TRUE(equal(vector[0], vectorCopy[0]));
}

TEST(WTF_StringFunctions, StringTypeAdapters)
{
    RefPtr<StringImpl> hello = StringImpl::create("Hello"_span8);
    RefPtr<StringImpl> null;
    RefPtr<StringImpl> sixteenBit = u"Wörld"_str;
    Ref<StringImpl> world = *sixteenBit;
    RefPtr<AtomStringImpl> atom = AtomStringImpl::add("!"_s);
    expectString(u"Hello, W\x00F6rld!Hello"sv, makeString(hello, null, ", "_s, world, atom, static_cast<const StringImpl*>(hello.get())));
    expectString(u"Hello"sv, makeString(null, hello, null));
    expectString(u""sv, makeString(null));
    expectString(u"Hello"sv, tryMakeString(hello));
    expectString(u"Hello"sv, makeString("Hello"_si));
    StringBuilder builder;
    builder.append(hello, null, world);
    expectString(u"HelloW\x00F6rld"sv, builder.toString());
    EXPECT_EQ(AtomStringImpl::add("Hello!"_s).ptr(), makeAtomString(hello, atom).get());
}

TEST(WTF_StringFunctions, Printing)
{
    RefPtr<StringImpl> string = u"Prïnt"_str;
    RefPtr<StringImpl> null;
    Ref<StringImpl> ref = StringImpl::create("ref"_span8);
    RefPtr<AtomStringImpl> atom = AtomStringImpl::add("atom"_s);

    auto textStreamString = [](auto&& value) {
        TextStream stream;
        stream << value;
        return stream.release();
    };
    expectString(u"Pr\x00EFnt"sv, textStreamString(string));
    expectString(u"Pr\x00EFnt"sv, textStreamString(string.get()));
    expectString(u""sv, textStreamString(null));
    expectString(u""sv, textStreamString(null.get()));
    expectString(u"ref"sv, textStreamString(ref));
    expectString(u"atom"sv, textStreamString(atom));
    expectString(u"atom"sv, textStreamString(atom.get()));
    expectString(u"Pr\x00EFnt"sv, textStreamString(static_cast<const StringImpl*>(string.get())));

    auto printStreamString = [](auto&& value) {
        StringPrintStream stream;
        stream.print(value);
        return stream.toString();
    };
    expectString(u"Pr\x00EFnt"sv, printStreamString(string));
    expectString(u"Pr\x00EFnt"sv, printStreamString(string.get()));
    expectString(u""sv, printStreamString(null));
    expectString(u""sv, printStreamString(null.get()));
    expectString(u"ref"sv, printStreamString(ref));
    expectString(u"atom"sv, printStreamString(atom));
    expectString(u"atom"sv, printStreamString(atom.get()));

    std::ostringstream ostream;
    ostream << string << '|' << null << '|' << ref;
    EXPECT_EQ("Pr\xC3\xAFnt||ref"sv, ostream.str());
}

TEST(WTF_StringFunctions, Markable)
{
    Markable<RefPtr<StringImpl>> markable;
    EXPECT_FALSE(markable);
    markable = RefPtr<StringImpl> { "markable"_si };
    EXPECT_TRUE(markable);
    EXPECT_TRUE(equal(*markable, "markable"_s));
    Markable<RefPtr<AtomStringImpl>> atomMarkable;
    EXPECT_FALSE(atomMarkable);
    atomMarkable = RefPtr<AtomStringImpl> { AtomStringImpl::add("markable"_s) };
    EXPECT_TRUE(atomMarkable);

    static_assert(VectorTraits<RefPtr<StringImpl>>::canMoveWithMemcpy);
    static_assert(VectorTraits<RefPtr<StringImpl>>::canInitializeWithMemset);
    static_assert(!VectorTraits<RefPtr<StringImpl>>::canCompareWithMemcmp);
    static_assert(VectorTraits<RefPtr<AtomStringImpl>>::canMoveWithMemcpy);
    static_assert(VectorTraits<Ref<StringImpl>>::canMoveWithMemcpy);
}

TEST(WTF_StringFunctions, ContentEquals)
{
    RefPtr<StringImpl> a1 = StringImpl::create("a"_span8);
    RefPtr<StringImpl> a2 = StringImpl::create("a"_span8);
    RefPtr<StringImpl> b = StringImpl::create("b"_span8);
    RefPtr<StringImpl> null;
    EXPECT_TRUE(contentEquals(a1, a2));
    EXPECT_FALSE(contentEquals(a1, b));
    EXPECT_FALSE(contentEquals(null, RefPtr<StringImpl> { emptyString() }));
    EXPECT_TRUE(contentEquals(null, RefPtr<StringImpl> { }));
    EXPECT_TRUE(contentEquals(a1.get(), a2.get()));
    EXPECT_TRUE(contentEquals(Ref { *a1 }, Ref { *a2 }));

    // Atoms compare by identity. Two different atoms never have the same content.
    RefPtr<AtomStringImpl> atom1 = AtomStringImpl::add("atom"_s);
    RefPtr<AtomStringImpl> atom2 = AtomStringImpl::add("atom"_s);
    RefPtr<AtomStringImpl> atom3 = AtomStringImpl::add("other atom"_s);
    EXPECT_TRUE(contentEquals(atom1, atom2));
    EXPECT_FALSE(contentEquals(atom1, atom3));
    static_assert(WTF::ContentEqualsUsesIdentity<RefPtr<AtomStringImpl>>);
    static_assert(WTF::ContentEqualsUsesIdentity<RefPtr<UniquedStringImpl>>);
    static_assert(WTF::ContentEqualsUsesIdentity<SymbolImpl*>);
    static_assert(!WTF::ContentEqualsUsesIdentity<RefPtr<StringImpl>>);

    // Containers. Vectors of non-atom strings have no ==, since the element comparison is deleted.
    Vector<RefPtr<StringImpl>> v1 { a1, b, null };
    Vector<RefPtr<StringImpl>> v2 { a2, StringImpl::create("b"_span8), nullptr };
    Vector<RefPtr<StringImpl>> v3 { a2, b };
    EXPECT_TRUE(contentEquals(v1, v2));
    static_assert(!std::equality_comparable<Vector<RefPtr<StringImpl>>>);
    static_assert(std::equality_comparable<Vector<RefPtr<AtomStringImpl>>>);
    EXPECT_FALSE(contentEquals(v1, v3));
    EXPECT_TRUE(contentEquals(FixedVector<RefPtr<StringImpl>> { a1, b }, FixedVector<RefPtr<StringImpl>> { a2, b }));
    EXPECT_TRUE(contentEquals(std::array<RefPtr<StringImpl>, 2> { a1, b }, std::array<RefPtr<StringImpl>, 2> { a2, b }));
    EXPECT_TRUE(contentEquals(v1.span(), v2.span()));
    EXPECT_TRUE(contentEquals(std::optional<RefPtr<StringImpl>> { a1 }, std::optional<RefPtr<StringImpl>> { a2 }));
    EXPECT_FALSE(contentEquals(std::optional<RefPtr<StringImpl>> { }, std::optional<RefPtr<StringImpl>> { null }));
    EXPECT_TRUE(contentEquals(std::optional<RefPtr<StringImpl>> { null }, std::optional<RefPtr<StringImpl>> { null }));
    EXPECT_TRUE(contentEquals(std::pair { a1, 1 }, std::pair { a2, 1 }));
    EXPECT_FALSE(contentEquals(std::pair { a1, 1 }, std::pair { a2, 2 }));
    EXPECT_TRUE(contentEquals(KeyValuePair<RefPtr<StringImpl>, int> { a1, 1 }, KeyValuePair<RefPtr<StringImpl>, int> { a2, 1 }));
    EXPECT_FALSE(contentEquals(KeyValuePair<RefPtr<StringImpl>, int> { a1, 1 }, KeyValuePair<RefPtr<StringImpl>, int> { b, 1 }));
    EXPECT_TRUE(contentEquals(std::tuple { a1, 1, b }, std::tuple { a2, 1, b }));
    EXPECT_TRUE(contentEquals(Variant<int, RefPtr<StringImpl>> { a1 }, Variant<int, RefPtr<StringImpl>> { a2 }));
    EXPECT_FALSE(contentEquals(Variant<int, RefPtr<StringImpl>> { a1 }, Variant<int, RefPtr<StringImpl>> { 1 }));
    EXPECT_TRUE(contentEquals(std::variant<int, RefPtr<StringImpl>> { a1 }, std::variant<int, RefPtr<StringImpl>> { a2 }));
    EXPECT_TRUE(contentEquals(Markable<RefPtr<StringImpl>> { a1 }, Markable<RefPtr<StringImpl>> { a2 }));
    EXPECT_TRUE(contentEquals(std::expected<RefPtr<StringImpl>, int> { a1 }, std::expected<RefPtr<StringImpl>, int> { a2 }));
    EXPECT_FALSE(contentEquals(std::expected<RefPtr<StringImpl>, int> { a1 }, std::expected<RefPtr<StringImpl>, int> { std::unexpected(1) }));
    EXPECT_TRUE(contentEquals(std::expected<void, RefPtr<StringImpl>> { std::unexpected(a1) }, std::expected<void, RefPtr<StringImpl>> { std::unexpected(a2) }));

    HashMap<int, RefPtr<StringImpl>> map1;
    HashMap<int, RefPtr<StringImpl>> map2;
    map1.add(1, a1);
    map2.add(1, a2);
    EXPECT_TRUE(contentEquals(map1, map2));
    map2.set(1, b);
    EXPECT_FALSE(contentEquals(map1, map2));
    HashSet<RefPtr<StringImpl>> set1 { a1, b };
    HashSet<RefPtr<StringImpl>> set2 { a2, StringImpl::create("b"_span8) };
    EXPECT_TRUE(contentEquals(set1, set2));

    // Nested.
    Vector<std::pair<RefPtr<StringImpl>, Vector<RefPtr<StringImpl>>>> nested1 { { a1, { b } } };
    Vector<std::pair<RefPtr<StringImpl>, Vector<RefPtr<StringImpl>>>> nested2 { { a2, { StringImpl::create("b"_span8) } } };
    EXPECT_TRUE(contentEquals(nested1, nested2));

    // Everything else uses ==.
    EXPECT_TRUE(contentEquals(1, 1));
    EXPECT_FALSE(contentEquals(1, 2));
    EXPECT_TRUE(contentEquals(Vector<int> { 1, 2 }, Vector<int> { 1, 2 }));
    EXPECT_TRUE(contentEquals(Vector<RefPtr<AtomStringImpl>> { atom1 }, Vector<RefPtr<AtomStringImpl>> { atom2 }));
}

template<typename T> static Vector<uint8_t> encodeForPersistence(const T& value)
{
    WTF::Persistence::Encoder encoder;
    encoder << value;
    return encoder.span();
}

template<typename T> static std::optional<T> decodeForPersistence(std::span<const uint8_t> bytes)
{
    WTF::Persistence::Decoder decoder(bytes);
    std::optional<T> result;
    decoder >> result;
    return result;
}

TEST(WTF_StringFunctions, PersistentCoders)
{
    // The same bytes as the former String and AtomString coders produced: the length (UINT_MAX for null), whether the
    // string is 8-bit, and the characters.
    EXPECT_EQ((Vector<uint8_t> { 0xFF, 0xFF, 0xFF, 0xFF }), encodeForPersistence(RefPtr<StringImpl> { }));
    EXPECT_EQ((Vector<uint8_t> { 0x00, 0x00, 0x00, 0x00, 0x01 }), encodeForPersistence(RefPtr<StringImpl> { emptyString() }));
    EXPECT_EQ((Vector<uint8_t> { 0x03, 0x00, 0x00, 0x00, 0x01, 0x61, 0x62, 0x63 }), encodeForPersistence(RefPtr<StringImpl> { "abc"_s }));
    EXPECT_EQ((Vector<uint8_t> { 0x02, 0x00, 0x00, 0x00, 0x00, 0xE9, 0x00, 0xAC, 0x20 }), encodeForPersistence(RefPtr<StringImpl> { u"é€"_str }));
    EXPECT_EQ((Vector<uint8_t> { 0x04, 0x00, 0x00, 0x00, 0x01, 0x61, 0x74, 0x6F, 0x6D }), encodeForPersistence("atom"_atom));
    EXPECT_EQ((Vector<uint8_t> { 0x03, 0x00, 0x00, 0x00, 0x01, 0x61, 0x62, 0x63 }), encodeForPersistence(StringImpl::create("abc"_s).ptr()));

    auto null = decodeForPersistence<RefPtr<StringImpl>>(std::array<uint8_t, 4> { 0xFF, 0xFF, 0xFF, 0xFF });
    ASSERT_TRUE(null);
    EXPECT_NULL(*null);
    auto empty = decodeForPersistence<RefPtr<StringImpl>>(std::array<uint8_t, 5> { 0x00, 0x00, 0x00, 0x00, 0x01 });
    ASSERT_TRUE(empty);
    expectString(u""sv, *empty);
    auto sixteenBit = decodeForPersistence<RefPtr<StringImpl>>(std::array<uint8_t, 9> { 0x02, 0x00, 0x00, 0x00, 0x00, 0xE9, 0x00, 0xAC, 0x20 });
    ASSERT_TRUE(sixteenBit);
    expectString(u"é€"sv, *sixteenBit);
    EXPECT_FALSE(is8Bit(*sixteenBit));
    auto atom = decodeForPersistence<RefPtr<AtomStringImpl>>(std::array<uint8_t, 9> { 0x04, 0x00, 0x00, 0x00, 0x01, 0x61, 0x74, 0x6F, 0x6D });
    ASSERT_TRUE(atom);
    EXPECT_EQ("atom"_atom.get(), atom->get());
    auto nullAtom = decodeForPersistence<RefPtr<AtomStringImpl>>(std::array<uint8_t, 4> { 0xFF, 0xFF, 0xFF, 0xFF });
    ASSERT_TRUE(nullAtom);
    EXPECT_NULL(*nullAtom);
    // Truncated input fails to decode.
    EXPECT_FALSE(decodeForPersistence<RefPtr<StringImpl>>(std::array<uint8_t, 6> { 0x03, 0x00, 0x00, 0x00, 0x01, 0x61 }));
}

TEST(WTF_StringFunctions, GTestComparesStringContents)
{
    // EXPECT_EQ and EXPECT_NE compare strings by content (see Test.h), as they did with String.
    RefPtr<StringImpl> a1 = StringImpl::create("a"_span8);
    RefPtr<StringImpl> a2 = StringImpl::create("a"_span8);
    EXPECT_EQ(a1, a2);
    EXPECT_EQ(a1, "a"_s);
    EXPECT_EQ("a"_s, a1);
    EXPECT_EQ(a1, a2.get());
    EXPECT_EQ(a1, StringView { "a"_s });
    EXPECT_NE(a1, "b"_s);
    EXPECT_NE(a1, RefPtr<StringImpl> { });
    EXPECT_NE(RefPtr<StringImpl> { emptyString() }, RefPtr<StringImpl> { });
    EXPECT_EQ((Vector<RefPtr<StringImpl>> { a1, nullptr }), (Vector<RefPtr<StringImpl>> { a2, nullptr }));
    EXPECT_NE((Vector<RefPtr<StringImpl>> { a1 }), (Vector<RefPtr<StringImpl>> { a2, nullptr }));
    // Raw pointers keep comparing by identity.
    EXPECT_NE(a1.get(), a2.get());
}

} // namespace TestWebKitAPI
