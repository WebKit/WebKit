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

// Strings are pointers now (RefPtr<StringImpl>, StringImpl*), but must keep the value semantics that String had:
// comparisons, hashing, container lookups and printing use the contents. These tests pin down which pointer
// operations are unavailable (so they can't silently compare identity) and that the remaining ones use contents.

#include "config.h"

#include "Helpers/Test.h"
#include <functional>
#include <sstream>
#include <wtf/CrossThreadCopier.h>
#include <wtf/Deque.h>
#include <wtf/FixedVector.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StringPrintStream.h>
#include <wtf/Variant.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringFunctions.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/SymbolImpl.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using namespace std::literals;

namespace {

template<typename A, typename B> concept HasEqual = requires(const A& a, const B& b)
{
    a == b;
};
template<typename A, typename B> concept HasNotEqual = requires(const A& a, const B& b)
{
    a != b;
};
template<typename A, typename B> concept HasLess = requires(const A& a, const B& b)
{
    a < b;
};
template<typename A, typename B> concept HasGreater = requires(const A& a, const B& b)
{
    a > b;
};
template<typename A, typename B> concept HasLessOrEqual = requires(const A& a, const B& b)
{
    a <= b;
};
template<typename A, typename B> concept HasThreeWay = requires(const A& a, const B& b)
{
    a <=> b;
};

template<typename A, typename B> constexpr bool hasNoComparisons = !HasEqual<A, B> && !HasNotEqual<A, B> && !HasLess<A, B>
    && !HasGreater<A, B> && !HasLessOrEqual<A, B> && !HasThreeWay<A, B>;
template<typename A, typename B> constexpr bool hasNoComparisonsEitherWay = hasNoComparisons<A, B> && hasNoComparisons<B, A>;

template<typename A, typename B> constexpr bool hasEqualityEitherWay = HasEqual<A, B> && HasNotEqual<A, B> && HasEqual<B, A> && HasNotEqual<B, A>;

template<typename Map, typename Key> concept CanGet = requires(Map& map, const Key& key)
{
    map.get(key);
};
template<typename Set, typename Key> concept CanContain = requires(Set& set, const Key& key)
{
    set.contains(key);
};

using StringPointer = StringImpl*;
using StringRefPtr = RefPtr<StringImpl>;
using StringRef = Ref<StringImpl>;
using AtomPointer = AtomStringImpl*;
using AtomRefPtr = RefPtr<AtomStringImpl>;
using AtomRef = Ref<AtomStringImpl>;

// Distinct objects with the same contents.
static RefPtr<StringImpl> create(ASCIILiteral literal)
{
    return StringImpl::create(literal.span8());
}

} // namespace

TEST(WTF_StringImplPointerSemantics, ComparisonsBetweenStringPointersAreUnavailable)
{
    // A smart pointer to a plain StringImpl can't be compared with any other string pointer: that would compare
    // identity where String compared contents. Use equal() for contents and isSameImpl() for identity.
    static_assert(hasNoComparisonsEitherWay<StringRefPtr, StringRefPtr>);
    static_assert(hasNoComparisonsEitherWay<StringRefPtr, StringPointer>);
    static_assert(hasNoComparisonsEitherWay<StringRefPtr, const StringImpl*>);
    static_assert(hasNoComparisonsEitherWay<StringRefPtr, StringRef>);
    static_assert(hasNoComparisonsEitherWay<StringRefPtr, AtomPointer>);
    static_assert(hasNoComparisonsEitherWay<StringRefPtr, AtomRefPtr>);
    static_assert(hasNoComparisonsEitherWay<StringRefPtr, AtomRef>);
    static_assert(hasNoComparisonsEitherWay<StringRefPtr, RefPtr<UniquedStringImpl>>);
    static_assert(hasNoComparisonsEitherWay<StringRefPtr, RefPtr<SymbolImpl>>);
    static_assert(hasNoComparisonsEitherWay<StringRef, StringRef>);
    static_assert(hasNoComparisonsEitherWay<StringRef, StringPointer>);
    static_assert(hasNoComparisonsEitherWay<StringRef, AtomPointer>);
    static_assert(hasNoComparisonsEitherWay<StringRef, AtomRefPtr>);
    static_assert(hasNoComparisonsEitherWay<StringRef, AtomRef>);
    static_assert(hasNoComparisonsEitherWay<StringPointer, AtomRefPtr>);
    static_assert(hasNoComparisonsEitherWay<StringPointer, AtomRef>);

    // Not covered (built-in pointer comparisons after implicit conversions, which can't be deleted): StringImpl* == StringImpl*,
    // and wrappers that convert to raw pointers, e.g. NeverDestroyed<RefPtr<StringImpl>> == RefPtr<StringImpl>. They compare
    // identity; Tools/hazard_scan.py (ptr-compare) reports them.
    static_assert(HasEqual<NeverDestroyed<StringRefPtr>, StringRefPtr>);

    // Vectors of strings have no ==, since their elements don't; use contentEquals().
    static_assert(!HasEqual<Vector<StringRefPtr>, Vector<StringRefPtr>>);
    static_assert(!HasEqual<Vector<StringRef>, Vector<StringRef>>);

    // There is no std::hash for string smart pointers (it would hash the address), so std::unordered_map can't be keyed by them.
    static_assert(!std::is_default_constructible_v<std::hash<StringRefPtr>>);
    static_assert(!std::is_default_constructible_v<std::hash<StringRef>>);
}

TEST(WTF_StringImplPointerSemantics, AllowedPointerComparisons)
{
    // Atoms (and uniqued strings and symbols) are unique per contents, so identity comparisons are content comparisons.
    static_assert(hasEqualityEitherWay<AtomRefPtr, AtomRefPtr>);
    static_assert(hasEqualityEitherWay<AtomRefPtr, AtomPointer>);
    static_assert(hasEqualityEitherWay<AtomRef, AtomRef>);
    static_assert(hasEqualityEitherWay<AtomRef, AtomPointer>);
    static_assert(hasEqualityEitherWay<AtomRefPtr, AtomRef>);
    static_assert(hasEqualityEitherWay<RefPtr<UniquedStringImpl>, AtomRefPtr>);
    static_assert(hasEqualityEitherWay<RefPtr<UniquedStringImpl>, RefPtr<UniquedStringImpl>>);
    static_assert(hasEqualityEitherWay<RefPtr<SymbolImpl>, RefPtr<SymbolImpl>>);

    // Null checks.
    static_assert(HasEqual<StringRefPtr, std::nullptr_t> && HasNotEqual<StringRefPtr, std::nullptr_t>);
    static_assert(HasEqual<AtomRefPtr, std::nullptr_t> && HasNotEqual<AtomRefPtr, std::nullptr_t>);

    // StringView compares contents (ignoring nullity, as String == StringView did).
    static_assert(hasEqualityEitherWay<StringRefPtr, StringView>);

    // Literals compare contents. For smart pointers, nullity is respected like String == ASCIILiteral (a null string
    // isn't equal to ""_s); raw pointers compare through StringView, ignoring nullity, as StringImpl* == ASCIILiteral did.
    static_assert(hasEqualityEitherWay<StringRefPtr, ASCIILiteral>);
    static_assert(hasEqualityEitherWay<StringRef, ASCIILiteral>);
    static_assert(hasEqualityEitherWay<AtomRefPtr, ASCIILiteral>);
    static_assert(hasEqualityEitherWay<StringPointer, ASCIILiteral>);
    static_assert(!HasLess<StringRefPtr, ASCIILiteral>);
    RefPtr<StringImpl> null;
    RefPtr<StringImpl> empty = emptyString();
    RefPtr<StringImpl> text = create("text"_s);
    EXPECT_TRUE(text == "text"_s);
    EXPECT_TRUE("text"_s == text);
    EXPECT_FALSE(text != "text"_s);
    EXPECT_TRUE(text != "other"_s);
    EXPECT_TRUE(Ref { *text } == "text"_s);
    EXPECT_FALSE(null == ""_s);
    EXPECT_TRUE(null != ""_s);
    EXPECT_TRUE(empty == ""_s);
    EXPECT_FALSE(null == "text"_s);
    EXPECT_TRUE(AtomStringImpl::add("text"_s) == "text"_s);
    EXPECT_FALSE(RefPtr<AtomStringImpl> { } == ""_s);
    EXPECT_TRUE(null.get() == ""_s);
    EXPECT_TRUE(StringView { null } == StringView { empty });

    // Raw pointers compare identity and can't be overloaded; equal() is the content comparison.
    static_assert(HasEqual<StringPointer, StringPointer>);
    static_assert(HasEqual<StringPointer, AtomPointer>);
    auto a = create("same"_s);
    auto b = create("same"_s);
    EXPECT_FALSE(a.get() == b.get());
    EXPECT_TRUE(equal(a.get(), b.get()));
    EXPECT_FALSE(isSameImpl(a, b));
    EXPECT_TRUE(isSameImpl(a, a.get()));

    auto atom1 = AtomStringImpl::add("atom"_s);
    auto atom2 = AtomStringImpl::add(create("atom"_s).get());
    EXPECT_TRUE(atom1 == atom2);
    EXPECT_TRUE(atom1.get() == atom2.get());
}

TEST(WTF_StringImplPointerSemantics, EqualComparesContents)
{
    auto a1 = create("abc"_s);
    auto a2 = create("abc"_s);
    auto b = create("abd"_s);
    RefPtr<StringImpl> null;
    RefPtr<StringImpl> empty = emptyString();
    auto otherEmpty = StringImpl::create(std::span<const char16_t> { });
    Ref<StringImpl> ref = *create("abc"_s);
    auto atom = AtomStringImpl::add("abc"_s);

    ASSERT_NE(a1.get(), a2.get());
    EXPECT_TRUE(equal(a1, a2));
    EXPECT_TRUE(equal(a1.get(), a2.get()));
    EXPECT_TRUE(equal(a1, a2.get()));
    EXPECT_TRUE(equal(a1, ref));
    EXPECT_TRUE(equal(ref, a1.get()));
    EXPECT_TRUE(equal(a1, atom));
    EXPECT_TRUE(equal(atom.get(), a1.get()));
    EXPECT_TRUE(equal(a1, "abc"_s));
    EXPECT_TRUE(equal("abc"_s, a1));
    EXPECT_TRUE(equal(a1, StringView { a2 }));
    EXPECT_FALSE(equal(a1, b));
    EXPECT_FALSE(equal(a1, "abd"_s));

    // Null and empty are different, like String() and emptyString(); two nulls are equal.
    EXPECT_FALSE(equal(null, empty));
    EXPECT_FALSE(equal(empty, null));
    EXPECT_FALSE(equal(null, ""_s));
    EXPECT_TRUE(equal(empty, ""_s));
    EXPECT_TRUE(equal(empty, otherEmpty));
    EXPECT_TRUE(equal(null, RefPtr<StringImpl> { }));
    EXPECT_TRUE(equal(null.get(), static_cast<StringImpl*>(nullptr)));
    EXPECT_FALSE(equal(null, a1));
    EXPECT_TRUE(equalIgnoringNullity(null, empty));
    EXPECT_TRUE(equal(StringView { null }, StringView { empty }));

    // Static strings are compared by contents too.
    EXPECT_TRUE(equal("abc"_si, a1));
    EXPECT_FALSE(isSameImpl("abc"_si, a1));

    // gtest's EXPECT_EQ compares contents (Test.h), like it did for String.
    EXPECT_EQ(a1, a2);
    EXPECT_EQ(a1.get(), a2);
    EXPECT_NE(a1, b);
    EXPECT_NE(null, empty);
}

namespace {

struct Named {
    RefPtr<StringImpl> name;
    int value { 0 };

    friend bool operator==(const Named& a, const Named& b) { return contentEquals(a.name, b.name) && a.value == b.value; }
};

} // namespace

TEST(WTF_StringImplPointerSemantics, ContentEqualsOnAggregates)
{
    auto a1 = create("a"_s);
    auto a2 = create("a"_s);
    auto b = create("b"_s);
    RefPtr<StringImpl> null;

    // Structs with string members compare contents through contentEquals().
    EXPECT_TRUE((Named { a1, 1 } == Named { a2, 1 }));
    EXPECT_FALSE((Named { a1, 1 } == Named { b, 1 }));
    EXPECT_FALSE((Named { null, 1 } == Named { emptyString(), 1 }));
    EXPECT_TRUE(contentEquals(Named { a1, 1 }, Named { a2, 1 }));
    EXPECT_TRUE((Vector<Named> { { a1, 1 } } == Vector<Named> { { a2, 1 } }));

    EXPECT_TRUE(contentEquals(std::optional<RefPtr<StringImpl>> { a1 }, std::optional<RefPtr<StringImpl>> { a2 }));
    EXPECT_FALSE(contentEquals(std::optional<RefPtr<StringImpl>> { a1 }, std::optional<RefPtr<StringImpl>> { b }));
    EXPECT_FALSE(contentEquals(std::optional<RefPtr<StringImpl>> { null }, std::optional<RefPtr<StringImpl>> { }));
    EXPECT_TRUE(contentEquals(Variant<int, RefPtr<StringImpl>> { a1 }, Variant<int, RefPtr<StringImpl>> { a2 }));
    EXPECT_FALSE(contentEquals(Variant<int, RefPtr<StringImpl>> { a1 }, Variant<int, RefPtr<StringImpl>> { b }));
    EXPECT_TRUE(contentEquals(Vector<RefPtr<StringImpl>> { a1, null, b }, Vector<RefPtr<StringImpl>> { a2, null, create("b"_s) }));
    EXPECT_FALSE(contentEquals(Vector<RefPtr<StringImpl>> { a1, null }, Vector<RefPtr<StringImpl>> { a2, emptyString() }));
    EXPECT_TRUE(contentEquals(std::pair { a1, b }, std::pair { a2, create("b"_s) }));
    EXPECT_FALSE(contentEquals(std::pair { a1, b }, std::pair { b, a1 }));
    EXPECT_TRUE(contentEquals(Vector<Ref<StringImpl>> { *a1 }, Vector<Ref<StringImpl>> { *a2 }));

    // Atoms compare identity, which is the same as contents for atoms.
    EXPECT_TRUE(contentEquals(Vector<RefPtr<AtomStringImpl>> { AtomStringImpl::add(a1.get()) }, Vector<RefPtr<AtomStringImpl>> { AtomStringImpl::add(a2.get()) }));
}

TEST(WTF_StringImplPointerSemantics, HashContainersAreKeyedByContents)
{
    auto key1 = create("key"_s);
    auto key2 = create("key"_s);
    ASSERT_NE(key1.get(), key2.get());

    HashSet<RefPtr<StringImpl>> set;
    EXPECT_TRUE(set.add(key1).isNewEntry);
    EXPECT_FALSE(set.add(key2).isNewEntry);
    EXPECT_EQ(1u, set.size());
    EXPECT_TRUE(set.contains(key2));
    EXPECT_TRUE(set.contains(key2.get()));
    EXPECT_TRUE(set.contains(*key2));
    EXPECT_TRUE(set.contains("key"_si));
    EXPECT_TRUE(set.contains(AtomStringImpl::add("key"_s).get()));
    EXPECT_FALSE(set.contains(create("other"_s)));
    EXPECT_TRUE(set.add(emptyString()).isNewEntry);
    EXPECT_TRUE(set.contains(StringImpl::create(std::span<const char16_t> { })));
    EXPECT_TRUE(set.remove(key2.get()));
    EXPECT_FALSE(set.contains(key1));

    // Adding and looking up through raw pointers hashes contents too.
    HashSet<RefPtr<StringImpl>> setFromRawPointers;
    setFromRawPointers.add(key1.get());
    EXPECT_TRUE(setFromRawPointers.contains(key2.get()));

    // So do Ref keys.
    HashSet<Ref<StringImpl>> refSet;
    refSet.add(*key1);
    EXPECT_FALSE(refSet.add(*key2).isNewEntry);
    EXPECT_TRUE(refSet.contains(key2.get()));

    HashMap<RefPtr<StringImpl>, int> map;
    map.set(key1, 1);
    map.set(key2, 2);
    EXPECT_EQ(1u, map.size());
    EXPECT_EQ(2, map.get(key1));
    EXPECT_EQ(2, map.get(key2.get()));
    EXPECT_EQ(2, map.get("key"_si));
    EXPECT_EQ(0, map.get(create("missing"_s)));
    EXPECT_TRUE(map.contains(*key1));
    auto iterator = map.find(key2.get());
    ASSERT_TRUE(iterator != map.end());
    EXPECT_EQ(key1.get(), iterator->key.get());

    HashCountedSet<RefPtr<StringImpl>> countedSet;
    countedSet.add(key1);
    countedSet.add(key2);
    EXPECT_EQ(2u, countedSet.count(create("key"_s)));

    ListHashSet<RefPtr<StringImpl>> listSet;
    listSet.add(key1);
    listSet.add(key2);
    EXPECT_EQ(1u, listSet.size());
    EXPECT_TRUE(listSet.contains(key2.get()));

    // Custom string hashes keep working with string pointer keys.
    HashSet<RefPtr<StringImpl>, ASCIICaseInsensitiveHash> caseInsensitiveSet;
    caseInsensitiveSet.add(create("MiXeD"_s));
    EXPECT_TRUE(caseInsensitiveSet.contains(create("mixed"_s)));
}

TEST(WTF_StringImplPointerSemantics, HashMapGetReturnsOwningPointers)
{
    // Like HashMap<..., String>::get(), get() returns an owning value (a null one for missing keys), so the result
    // stays valid when the map changes.
    HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> map;
    map.add(create("key"_s), create("value"_s));
    static_assert(std::is_same_v<decltype(map.get(create("key"_s))), RefPtr<StringImpl>>);
    static_assert(std::is_same_v<HashTraits<RefPtr<StringImpl>>::PeekType, RefPtr<StringImpl>>);
    static_assert(std::is_same_v<HashTraits<RefPtr<AtomStringImpl>>::PeekType, RefPtr<AtomStringImpl>>);

    RefPtr<StringImpl> value = map.get(create("key"_s));
    EXPECT_FALSE(value->hasOneRef());
    map.clear();
    EXPECT_TRUE(value->hasOneRef());
    EXPECT_TRUE(equal(value, "value"_s));
    EXPECT_NULL(map.get(create("key"_s)));

    HashMap<int, RefPtr<StringImpl>> intMap;
    intMap.add(1, emptyString());
    EXPECT_NOT_NULL(intMap.get(1));
    EXPECT_TRUE(isEmpty(intMap.get(1)));
    EXPECT_NULL(intMap.get(2));
    EXPECT_TRUE(isEmpty(intMap.get(2)));
    EXPECT_NULL(intMap.take(2));
    EXPECT_TRUE(equal(intMap.take(1), ""_s));
}

TEST(WTF_StringImplPointerSemantics, AtomKeysUseIdentity)
{
    HashMap<RefPtr<AtomStringImpl>, int> map;
    RefPtr<AtomStringImpl> atom = AtomStringImpl::add("atom key"_s);
    map.add(atom, 1);
    // Every atom with these contents is the same object, so identity lookups find it.
    EXPECT_EQ(1, map.get(AtomStringImpl::add(create("atom key"_s).get())));
    EXPECT_EQ(1, map.get(atom.get()));
    EXPECT_EQ(atom->existingHash(), DefaultHash<RefPtr<AtomStringImpl>>::hash(atom));
    EXPECT_TRUE(DefaultHash<RefPtr<AtomStringImpl>>::equal(atom, atom));

    // A plain StringImpl can't be looked up in an atom-keyed map (that would compare it by identity).
    static_assert(!CanGet<HashMap<RefPtr<AtomStringImpl>, int>, StringImpl*>);
    static_assert(!CanGet<HashMap<RefPtr<AtomStringImpl>, int>, RefPtr<StringImpl>>);
    static_assert(!CanContain<HashSet<RefPtr<AtomStringImpl>>, StringImpl*>);
    static_assert(CanGet<HashMap<RefPtr<AtomStringImpl>, int>, AtomStringImpl*>);
    static_assert(CanContain<HashSet<RefPtr<AtomStringImpl>>, AtomStringImpl*>);
}

TEST(WTF_StringImplPointerSemantics, VectorLookupsCompareContents)
{
    auto a1 = create("a"_s);
    auto a2 = create("a"_s);
    auto b = create("b"_s);
    RefPtr<StringImpl> null;

    // Like Vector<String>, lookups compare contents, also for raw pointer arguments (the smart pointer overloads
    // taking raw pointers would otherwise compare identity).
    Vector<RefPtr<StringImpl>> vector { a1, b, nullptr, emptyString() };
    EXPECT_TRUE(vector.contains(a2.get()));
    EXPECT_TRUE(vector.contains(a2));
    EXPECT_TRUE(vector.contains(Ref { *a2 }));
    EXPECT_TRUE(vector.contains("a"_si));
    EXPECT_TRUE(vector.contains("b"_s));
    EXPECT_TRUE(vector.contains(AtomStringImpl::add("b"_s).get()));
    EXPECT_FALSE(vector.contains(create("c"_s).get()));
    EXPECT_EQ(0u, vector.find(a2.get()));
    EXPECT_EQ(0u, vector.reverseFind(a2));
    EXPECT_EQ(1u, vector.find("b"_s));
    EXPECT_EQ(2u, vector.find(null));
    EXPECT_EQ(2u, vector.find(static_cast<StringImpl*>(nullptr)));
    EXPECT_EQ(2u, vector.find(nullptr));
    EXPECT_TRUE(vector.contains(nullptr));
    EXPECT_EQ(3u, vector.find(StringImpl::create(std::span<const char16_t> { })));
    EXPECT_EQ(3u, vector.find(""_s));
    EXPECT_FALSE(vector.appendIfNotContains(a2));
    EXPECT_TRUE(vector.appendIfNotContains(create("c"_s)));
    EXPECT_TRUE(vector.removeFirst(create("c"_s).get()));
    EXPECT_TRUE(vector.removeFirst(a2));
    EXPECT_FALSE(vector.contains(a1));
    vector.appendList({ create("x"_s), create("x"_s) });
    EXPECT_EQ(2u, vector.removeAll(create("x"_s)));
    EXPECT_TRUE(vector.removeLast(b.get()));
    EXPECT_EQ(2u, vector.size());

    Vector<Ref<StringImpl>> refVector { *a1 };
    EXPECT_TRUE(refVector.contains(a2.get()));
    EXPECT_TRUE(refVector.contains(a2));

    Vector<RefPtr<StringImpl>> vectorFromRawPointers { a1.get() };
    EXPECT_TRUE(vectorFromRawPointers.contains(a2.get()));

    FixedVector<RefPtr<StringImpl>> fixedVector { a1, b };
    EXPECT_EQ(1u, fixedVector.find(create("b"_s)));
    EXPECT_TRUE(fixedVector.contains(a2.get()));

    // Atom vectors compare contents with plain strings (like AtomString == String) and identity among atoms.
    Vector<RefPtr<AtomStringImpl>> atomVector { AtomStringImpl::add("a"_s) };
    EXPECT_TRUE(atomVector.contains(a2.get()));
    EXPECT_TRUE(atomVector.contains(AtomStringImpl::add("a"_s).get()));
    EXPECT_TRUE(atomVector.contains("a"_s));
    EXPECT_FALSE(atomVector.contains(b));
}

TEST(WTF_StringImplPointerSemantics, VariantFromRawPointerHoldsString)
{
    // A pointer converts to bool with a standard conversion, but Variant doesn't pick bool alternatives for
    // non-bool arguments, so a former String argument still selects the string alternative.
    auto string = create("text"_s);
    Variant<bool, RefPtr<StringImpl>> fromPointer = string.get();
    ASSERT_TRUE(std::holds_alternative<RefPtr<StringImpl>>(fromPointer));
    EXPECT_EQ(string.get(), std::get<RefPtr<StringImpl>>(fromPointer).get());
    Variant<bool, RefPtr<StringImpl>> fromRefPtr = string;
    EXPECT_TRUE(std::holds_alternative<RefPtr<StringImpl>>(fromRefPtr));
    Variant<bool, RefPtr<StringImpl>> fromBool = true;
    EXPECT_TRUE(std::holds_alternative<bool>(fromBool));
    Variant<bool, RefPtr<StringImpl>> fromNull = static_cast<StringImpl*>(nullptr);
    ASSERT_TRUE(std::holds_alternative<RefPtr<StringImpl>>(fromNull));
    EXPECT_NULL(std::get<RefPtr<StringImpl>>(fromNull));

    std::variant<bool, RefPtr<StringImpl>> stdVariant = string.get();
    EXPECT_TRUE(std::holds_alternative<RefPtr<StringImpl>>(stdVariant));

    // Overload sets taking bool and a string prefer the string for string pointers.
    auto overloaded = [](auto&& argument) {
        struct {
            int operator()(bool) const { return 1; }
            int operator()(StringImpl*) const { return 2; }
        } overloads;
        return overloads(argument);
    };
    EXPECT_EQ(2, overloaded(string.get()));
    EXPECT_EQ(1, overloaded(true));
}

TEST(WTF_StringImplPointerSemantics, PrintingShowsContents)
{
    RefPtr<StringImpl> string = create("contents"_s);
    RefPtr<StringImpl> null;
    Ref<StringImpl> ref = *create("ref"_s);
    RefPtr<AtomStringImpl> atom = AtomStringImpl::add("atom"_s);

    TextStream textStream;
    textStream << string << '|' << string.get() << '|' << null << '|' << null.get() << '|' << ref << '|' << atom << '|' << atom.get();
    EXPECT_TRUE(equal(textStream.release(), "contents|contents|||ref|atom|atom"_s));

    StringPrintStream printStream;
    printStream.print(string, "|"_s, string.get(), "|"_s, null, "|"_s, ref, "|"_s, atom);
    EXPECT_TRUE(equal(printStream.toString(), "contents|contents||ref|atom"_s));

    EXPECT_TRUE(equal(makeString(string, '|', string.get(), '|', null, '|', null.get(), '|', ref, '|', atom.get()), "contents|contents|||ref|atom"_s));
    StringBuilder builder;
    builder.append(string, '|', string.get(), '|', null, '|', ref);
    EXPECT_TRUE(equal(builder.toString(), "contents|contents||ref"_s));

    // std::ostream prints contents with TestWebKitAPI's printers (Test.h), as for String.
    std::ostringstream ostream;
    ostream << string << '|' << string.get() << '|' << null << '|' << ref;
    EXPECT_EQ("contents|contents||ref"sv, ostream.str());
}

TEST(WTF_StringImplPointerSemantics, IsolatedCopiesAreDistinct)
{
    RefPtr<StringImpl> string = create("shared"_s);
    RefPtr<StringImpl> otherReference = string;
    ASSERT_FALSE(string->hasOneRef());

    auto copy = isolatedCopy(string);
    EXPECT_NE(string.get(), copy.get());
    EXPECT_TRUE(copy->hasOneRef());
    EXPECT_TRUE(equal(string, copy));
    EXPECT_TRUE(isSafeToSendToAnotherThread(copy));

    auto crossThread = crossThreadCopy(string);
    EXPECT_NE(string.get(), crossThread.get());
    EXPECT_TRUE(equal(string, crossThread));

    Vector<RefPtr<StringImpl>> vector { string, string };
    auto vectorCopy = crossThreadCopy(vector);
    EXPECT_NE(vector[0].get(), vectorCopy[0].get());
    EXPECT_NE(vectorCopy[0].get(), vectorCopy[1].get());
    EXPECT_TRUE(equal(vectorCopy[1], "shared"_s));

    HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> map;
    map.add(string, string);
    auto mapCopy = crossThreadCopy(map);
    ASSERT_EQ(1u, mapCopy.size());
    EXPECT_NE(string.get(), mapCopy.begin()->key.get());
    EXPECT_NE(string.get(), mapCopy.begin()->value.get());
    EXPECT_TRUE(equal(mapCopy.get(string), "shared"_s));

    // Atoms are copied into plain strings, which can be sent to other threads.
    RefPtr<AtomStringImpl> atom = AtomStringImpl::add("atom"_s);
    RefPtr<StringImpl> atomCopy = crossThreadCopy(atom);
    EXPECT_FALSE(atomCopy->isAtom());
    EXPECT_TRUE(equal(atomCopy, atom));

    EXPECT_NULL(isolatedCopy(RefPtr<StringImpl> { }));
    EXPECT_NULL(crossThreadCopy(RefPtr<StringImpl> { }));
}

TEST(WTF_StringImplPointerSemantics, StaticStringLiterals)
{
    // "x"_si strings are immortal statics shared by all uses of the literal: never hasOneRef(), so code that needs a
    // fresh, uniquely owned string must use StringImpl::create(). They are not "safe to send to another thread" by the
    // hasOneRef() rule, so isolated copies are new strings that share the immortal characters (like isolated copies of
    // String("x"_s)).
    RefPtr<StringImpl> literal = "static"_si;
    EXPECT_TRUE(literal->isStatic());
    EXPECT_FALSE(literal->hasOneRef());
    RefPtr<StringImpl> again = "static"_si;
    EXPECT_EQ(literal.get(), again.get());
    EXPECT_FALSE(isSafeToSendToAnotherThread(literal));
    RefPtr<StringImpl> copy = isolatedCopy(literal);
    EXPECT_NE(literal.get(), copy.get());
    EXPECT_FALSE(copy->isStatic());
    EXPECT_TRUE(copy->hasOneRef());
    EXPECT_EQ(literal->span8().data(), copy->span8().data());
    EXPECT_TRUE(equal(copy, literal));
    RefPtr<StringImpl> moved = isolatedCopy(RefPtr { literal });
    EXPECT_NE(literal.get(), moved.get());
    RefPtr<StringImpl> crossThread = crossThreadCopy(literal);
    EXPECT_NE(literal.get(), crossThread.get());
    EXPECT_TRUE(equal(crossThread, "static"_s));
    EXPECT_TRUE(equal(literal, "static"_s));

    auto fresh = StringImpl::create("static"_s);
    EXPECT_FALSE(fresh->isStatic());
    EXPECT_TRUE(fresh->hasOneRef());
    EXPECT_NE(literal.get(), fresh.ptr());
    EXPECT_TRUE(equal(literal, fresh));

    // An atom of a static string has the same contents but is a different object.
    auto atom = AtomStringImpl::add(literal.get());
    EXPECT_TRUE(equal(atom, literal));
}

} // namespace TestWebKitAPI
