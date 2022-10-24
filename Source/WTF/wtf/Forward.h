/*
 *  Copyright (C) 2006-2021 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <stddef.h>
#include <wtf/Platform.h>

namespace WTF {

class ASCIILiteral;
class AbstractLocker;
class AtomString;
class AtomStringImpl;
class BinarySemaphore;
class CString;
class CrashOnOverflow;
class DefaultWeakPtrImpl;
class FunctionDispatcher;
class Hasher;
class Lock;
class Logger;
class MonotonicTime;
class OrdinalNumber;
class PrintStream;
class SHA1;
class Seconds;
class String;
class StringBuilder;
class StringImpl;
class StringView;
class SuspendableWorkQueue;
class TextPosition;
class TextStream;
class URL;
class UniquedStringImpl;
class WallTime;

struct AnyThreadsAccessTraits;
struct FastMalloc;
struct MainThreadAccessTraits;

#if ENABLE(MALLOC_HEAP_BREAKDOWN)
struct VectorMalloc;
#else
using VectorMalloc = FastMalloc;
#endif

template<typename> struct DefaultRefDerefTraits;

template<typename> class CompactPtr;
template<typename> class CompletionHandler;
template<typename Key, typename, Key> class EnumeratedArray;
template<typename> class FixedVector;
template<typename> class Function;
template<typename, typename = AnyThreadsAccessTraits> class LazyNeverDestroyed;
template<typename, typename = AnyThreadsAccessTraits> class NeverDestroyed;
template<typename> class ObjectIdentifier;
template<typename> class OptionSet;
template<typename> class Packed;
template<typename T, size_t = alignof(T)> class PackedAlignedPtr;
template<typename> struct RawPtrTraits;
template<typename T, typename = RawPtrTraits<T>> class CheckedRef;
template<typename T, typename = RawPtrTraits<T>> class CheckedPtr;
template<typename T, typename = RawPtrTraits<T>> class Ref;
template<typename T, typename = RawPtrTraits<T>, typename = DefaultRefDerefTraits<T>> class RefPtr;
template<typename> class RetainPtr;
template<typename> class ScopedLambda;
template<typename> class StringBuffer;
template<typename> class StringParsingBuffer;
template<typename, typename = void> class StringTypeAdapter;
template<typename> class UniqueRef;
template<typename, size_t = 0, typename = CrashOnOverflow, size_t = 16, typename Malloc = VectorMalloc> class Vector;
template<typename, typename = DefaultWeakPtrImpl> class WeakPtr;

template<typename> struct DefaultHash;
template<> struct DefaultHash<AtomString>;
template<typename T> struct DefaultHash<OptionSet<T>>;
template<> struct DefaultHash<String>;
template<> struct DefaultHash<StringImpl*>;
template<> struct DefaultHash<URL>;
template<typename T, size_t inlineCapacity> struct DefaultHash<Vector<T, inlineCapacity>>;

template<typename> struct RawValueTraits;
template<typename> struct EnumTraits;
template<typename E, E...> struct EnumValues;
template<typename> struct HashTraits;

struct HashTableTraits;
struct IdentityExtractor;
template<typename T> struct KeyValuePairKeyExtractor;
template<typename KeyTraits, typename MappedTraits> struct KeyValuePairTraits;
template<typename KeyTypeArg, typename ValueTypeArg> struct KeyValuePair;
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits> class HashTable;
template<typename Value, typename = DefaultHash<Value>, typename = HashTraits<Value>> class HashCountedSet;
template<typename KeyArg, typename MappedArg, typename = DefaultHash<KeyArg>, typename = HashTraits<KeyArg>, typename = HashTraits<MappedArg>, typename = HashTableTraits> class HashMap;
template<typename ValueArg, typename = DefaultHash<ValueArg>, typename = HashTraits<ValueArg>, typename = HashTableTraits> class HashSet;

}

namespace std {
namespace experimental {
inline namespace fundamentals_v3 {
template<class, class> class expected;
template<class> class unexpected;
}}} // namespace std::experimental::fundamentals_v3

using WTF::ASCIILiteral;
using WTF::AbstractLocker;
using WTF::AtomString;
using WTF::AtomStringImpl;
using WTF::BinarySemaphore;
using WTF::CString;
using WTF::CompletionHandler;
using WTF::EnumeratedArray;
using WTF::FixedVector;
using WTF::Function;
using WTF::FunctionDispatcher;
using WTF::HashCountedSet;
using WTF::HashMap;
using WTF::HashSet;
using WTF::Hasher;
using WTF::LazyNeverDestroyed;
using WTF::Lock;
using WTF::Logger;
using WTF::NeverDestroyed;
using WTF::ObjectIdentifier;
using WTF::OptionSet;
using WTF::OrdinalNumber;
using WTF::PrintStream;
using WTF::RawPtrTraits;
using WTF::RawValueTraits;
using WTF::Ref;
using WTF::RefPtr;
using WTF::RetainPtr;
using WTF::SHA1;
using WTF::ScopedLambda;
using WTF::String;
using WTF::StringBuffer;
using WTF::StringBuilder;
using WTF::StringImpl;
using WTF::StringParsingBuffer;
using WTF::StringView;
using WTF::SuspendableWorkQueue;
using WTF::TextPosition;
using WTF::TextStream;
using WTF::URL;
using WTF::UniqueRef;
using WTF::Vector;
using WTF::WeakPtr;

template<class T, class E> using Expected = std::experimental::expected<T, E>;
template<class E> using Unexpected = std::experimental::unexpected<E>;

// Sometimes an inline method simply forwards to another one and does nothing else. If it were
// just a forward declaration of that method then you would only need a forward declaration of
// its return types and parameter types too, but because it's inline and it actually needs to
// return / pass these types (even though it's just passing through whatever it called) you
// now find yourself having to actually have a full declaration of these types. That might be
// an include you'd rather avoid.
//
// No more. Enter template magic to lazily instantiate that method!
//
// This macro makes the method work as if you'd declared the return / parameter types as normal,
// but forces lazy instantiation of the method at the call site, at which point the caller (not
// the declaration) had better have a full declaration of the return / parameter types.
//
// Simply pass the forward-declared types to the macro, with an alias for each, and then define
// your function as you otherwise would have but using the aliased name. Why the alias? So you
// can be lazy on templated types! Sample usage:
//
// struct Foo; // No need to define Foo!
// template<typename T>
// struct A {
//     Foo declared(Bar); // Forward declarations of Foo and Bar are sufficient here.
//     // The below code would normally require a definition of Foo and Bar.
//     WTF_LAZY_INSTANTIATE(Foo=Foo, Bar=Bar) Foo forwarder(Bar b) { return declared(b); }
// };
#define WTF_LAZY_JOIN_UNLAZE(A, B) A##B
#define WTF_LAZY_JOIN(A, B) WTF_LAZY_JOIN_UNLAZE(A, B)
#define WTF_LAZY_ARGUMENT_NUMBER(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define WTF_LAZY_AUGMENT(...) unused, __VA_ARGS__
#define WTF_LAZY_EXPAND(x) x
#define WTF_LAZY_NUM_ARGS_(...) WTF_LAZY_EXPAND(WTF_LAZY_ARGUMENT_NUMBER(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0))
#define WTF_LAZY_NUM_ARGS(...) WTF_LAZY_NUM_ARGS_(WTF_LAZY_AUGMENT(__VA_ARGS__))
#define WTF_LAZY_FOR_EACH_TERM(F, ...) \
    WTF_LAZY_JOIN(WTF_LAZY_FOR_EACH_TERM_, WTF_LAZY_NUM_ARGS(__VA_ARGS__))(F, (__VA_ARGS__))
#define WTF_LAZY_FIRST(_1, ...) _1
#define WTF_LAZY_REST(_1, ...) (__VA_ARGS__)
#define WTF_LAZY_REST_(_1, ...) __VA_ARGS__
#define WTF_LAZY_CALL(F, ARG) F(ARG)
#define WTF_LAZY_FOR_EACH_TERM_0(...)
#define WTF_LAZY_FOR_EACH_TERM_1(F, ARGS) WTF_LAZY_CALL(F, WTF_LAZY_FIRST ARGS) WTF_LAZY_FOR_EACH_TERM_0(F, WTF_LAZY_REST ARGS)
#define WTF_LAZY_FOR_EACH_TERM_2(F, ARGS) WTF_LAZY_CALL(F, WTF_LAZY_FIRST ARGS) WTF_LAZY_FOR_EACH_TERM_1(F, WTF_LAZY_REST ARGS)
#define WTF_LAZY_FOR_EACH_TERM_3(F, ARGS) WTF_LAZY_CALL(F, WTF_LAZY_FIRST ARGS) WTF_LAZY_FOR_EACH_TERM_2(F, WTF_LAZY_REST ARGS)
#define WTF_LAZY_FOR_EACH_TERM_4(F, ARGS) WTF_LAZY_CALL(F, WTF_LAZY_FIRST ARGS) WTF_LAZY_FOR_EACH_TERM_3(F, WTF_LAZY_REST ARGS)
#define WTF_LAZY_FOR_EACH_TERM_5(F, ARGS) WTF_LAZY_CALL(F, WTF_LAZY_FIRST ARGS) WTF_LAZY_FOR_EACH_TERM_4(F, WTF_LAZY_REST ARGS)
#define WTF_LAZY_FOR_EACH_TERM_6(F, ARGS) WTF_LAZY_CALL(F, WTF_LAZY_FIRST ARGS) WTF_LAZY_FOR_EACH_TERM_5(F, WTF_LAZY_REST ARGS)
#define WTF_LAZY_FOR_EACH_TERM_7(F, ARGS) WTF_LAZY_CALL(F, WTF_LAZY_FIRST ARGS) WTF_LAZY_FOR_EACH_TERM_6(F, WTF_LAZY_REST ARGS)
#define WTF_LAZY_DECLARE_ALIAS_AND_TYPE(ALIAS_AND_TYPE) typename ALIAS_AND_TYPE,
#define WTF_LAZY_INSTANTIATE(...)                                        \
    template<                                                            \
    WTF_LAZY_FOR_EACH_TERM(WTF_LAZY_DECLARE_ALIAS_AND_TYPE, __VA_ARGS__) \
    typename = void>

#define WTF_LAZY_HAS_REST_0(...)
#define WTF_LAZY_HAS_REST_1(...)
#define WTF_LAZY_HAS_REST_2 WTF_LAZY_EXPAND
#define WTF_LAZY_HAS_REST_3 WTF_LAZY_EXPAND
#define WTF_LAZY_HAS_REST_4 WTF_LAZY_EXPAND
#define WTF_LAZY_HAS_REST_5 WTF_LAZY_EXPAND
#define WTF_LAZY_HAS_REST_6 WTF_LAZY_EXPAND
#define WTF_LAZY_HAS_REST_7 WTF_LAZY_EXPAND
#define WTF_LAZY_HAS_REST_8 WTF_LAZY_EXPAND
#define WTF_LAZY_HAS_REST(...) \
    WTF_LAZY_JOIN(WTF_LAZY_HAS_REST_, WTF_LAZY_NUM_ARGS(__VA_ARGS__))
