/*
 * Copyright (C) 2008-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2013 Patrick Gansterer <paroga@paroga.com>
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

#include <algorithm>
#include <bit>
#include <climits>
#include <concepts>
#include <cstring>
#include <errno.h>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <wtf/Assertions.h>
#include <wtf/Brigand.h>
#include <wtf/Compiler.h>
#include <wtf/GetPtr.h>
#include <wtf/IterationStatus.h>
#include <wtf/NotFound.h>
#include <wtf/StringExtras.h>
#include <wtf/TypeCasts.h>
#include <wtf/TypeTraits.h>
#include <wtf/Variant.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#define SINGLE_ARG(...) __VA_ARGS__ // useful when a macro argument includes a comma

// Use this macro to declare and define a debug-only global variable that may have a
// non-trivial constructor and destructor. When building with clang, this will suppress
// warnings about global constructors and exit-time destructors.
#define DEFINE_GLOBAL_FOR_LOGGING(type, name, arguments) \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wglobal-constructors\"") \
    _Pragma("clang diagnostic ignored \"-Wexit-time-destructors\"") \
    static type name arguments; \
    _Pragma("clang diagnostic pop")

#ifndef NDEBUG
#if COMPILER(CLANG)
#define DEFINE_DEBUG_ONLY_GLOBAL(type, name, arguments) DEFINE_GLOBAL_FOR_LOGGING(type, name, arguments)
#else
#define DEFINE_DEBUG_ONLY_GLOBAL(type, name, arguments) \
    static type name arguments;
#endif // COMPILER(CLANG)
#else
#define DEFINE_DEBUG_ONLY_GLOBAL(type, name, arguments)
#endif // NDEBUG

#if COMPILER(CLANG)
// We have to use __builtin_offsetof directly here instead of offsetof because otherwise Clang will drop
// our pragma and we'll still get the warning.
#define OBJECT_OFFSETOF(class, field) \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Winvalid-offsetof\"") \
    __builtin_offsetof(class, field) \
    _Pragma("clang diagnostic pop")
#elif COMPILER(GCC)
// It would be nice to silence this warning locally like we do on Clang but GCC complains about `error: ‘#pragma’ is not allowed here`
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#define OBJECT_OFFSETOF(class, field) offsetof(class, field)
#endif

// The magic number 0x4000 is insignificant. We use it to avoid using NULL, since
// NULL can cause compiler problems, especially in cases of multiple inheritance.
#define CAST_OFFSET(from, to) (reinterpret_cast<uintptr_t>(static_cast<to>((reinterpret_cast<from>(0x4000)))) - 0x4000)

// STRINGIZE: Can convert any value to quoted string, even expandable macros
#define STRINGIZE(exp) #exp
#define STRINGIZE_VALUE_OF(exp) STRINGIZE(exp)

// WTF_CONCAT: concatenate two symbols into one, even expandable macros
#define WTF_CONCAT_INTERNAL_DONT_USE(a, b) a ## b
#define WTF_CONCAT(a, b) WTF_CONCAT_INTERNAL_DONT_USE(a, b)


/*
 * The reinterpret_cast<Type1*>([pointer to Type2]) expressions - where
 * sizeof(Type1) > sizeof(Type2) - cause the following warning on ARM with GCC:
 * increases required alignment of target type.
 *
 * An implicit or an extra static_cast<void*> bypasses the warning.
 * For more info see the following bugzilla entries:
 * - https://bugs.webkit.org/show_bug.cgi?id=38045
 * - http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43976
 */
#if CPU(ARM) || CPU(MIPS) || CPU(RISCV64)
template<typename Type>
inline bool isPointerTypeAlignmentOkay(Type* ptr)
{
    return !(reinterpret_cast<intptr_t>(ptr) % __alignof__(Type));
}

template<typename TypePtr>
inline TypePtr reinterpret_cast_ptr(void* ptr)
{
    ASSERT(isPointerTypeAlignmentOkay(reinterpret_cast<TypePtr>(ptr)));
    return reinterpret_cast<TypePtr>(ptr);
}

template<typename TypePtr>
inline TypePtr reinterpret_cast_ptr(const void* ptr)
{
    ASSERT(isPointerTypeAlignmentOkay(reinterpret_cast<TypePtr>(ptr)));
    return reinterpret_cast<TypePtr>(ptr);
}
#else
template<typename Type>
inline bool isPointerTypeAlignmentOkay(Type*)
{
    return true;
}
#define reinterpret_cast_ptr reinterpret_cast
#endif

namespace WTF {

enum CheckMoveParameterTag { CheckMoveParameter };

static constexpr size_t KB = 1024;
static constexpr size_t MB = 1024 * 1024;
static constexpr size_t GB = 1024 * 1024 * 1024;

inline bool isPointerAligned(void* p)
{
    return !((intptr_t)(p) & (sizeof(char*) - 1));
}

inline bool is8ByteAligned(void* p)
{
    return !((uintptr_t)(p) & (sizeof(double) - 1));
}

inline std::byte* alignedBytes(std::byte* pointer, size_t alignment)
{
    return reinterpret_cast<std::byte*>((reinterpret_cast<uintptr_t>(pointer) - 1u + alignment) & -alignment);
}

inline const std::byte* alignedBytes(const std::byte* pointer, size_t alignment)
{
    return reinterpret_cast<const std::byte*>((reinterpret_cast<uintptr_t>(pointer) - 1u + alignment) & -alignment);
}

inline size_t alignedBytesCorrection(std::span<std::byte> buffer, size_t alignment)
{
    return reinterpret_cast<std::byte*>((reinterpret_cast<uintptr_t>(buffer.data()) - 1u + alignment) & -alignment) - buffer.data();
}

inline size_t alignedBytesCorrection(std::span<const std::byte> buffer, size_t alignment)
{
    return reinterpret_cast<const std::byte*>((reinterpret_cast<uintptr_t>(buffer.data()) - 1u + alignment) & -alignment) - buffer.data();
}

inline std::span<std::byte> alignedBytes(std::span<std::byte> buffer, size_t alignment)
{
    return buffer.subspan(alignedBytesCorrection(buffer, alignment));
}

inline std::span<const std::byte> alignedBytes(std::span<const std::byte> buffer, size_t alignment)
{
    return buffer.subspan(alignedBytesCorrection(buffer, alignment));
}

// Returns a count of the number of bits set in 'bits'.
inline size_t bitCount(unsigned bits)
{
    bits = bits - ((bits >> 1) & 0x55555555);
    bits = (bits & 0x33333333) + ((bits >> 2) & 0x33333333);
    return (((bits + (bits >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}

inline size_t bitCount(uint64_t bits)
{
    return bitCount(static_cast<unsigned>(bits)) + bitCount(static_cast<unsigned>(bits >> 32));
}

template<typename T> constexpr T mask(T value, uintptr_t mask)
{
    static_assert(sizeof(T) == sizeof(uintptr_t), "sizeof(T) must be equal to sizeof(uintptr_t).");
    return static_cast<T>(static_cast<uintptr_t>(value) & mask);
}

template<typename T> inline T* mask(T* value, uintptr_t mask)
{
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(value) & mask);
}

template<typename IntType>
constexpr IntType toTwosComplement(IntType integer)
{
    using UnsignedIntType = typename std::make_unsigned_t<IntType>;
    return static_cast<IntType>((~static_cast<UnsignedIntType>(integer)) + static_cast<UnsignedIntType>(1));
}

enum BinarySearchMode {
    KeyMustBePresentInArray,
    KeyMightNotBePresentInArray,
    ReturnAdjacentElementIfKeyIsNotPresent
};

template<typename ArrayElementType, typename KeyType, typename ArrayType, typename ExtractKey, BinarySearchMode mode>
inline ArrayElementType* binarySearchImpl(ArrayType& array, size_t size, KeyType key, const ExtractKey& extractKey = ExtractKey())
{
    size_t offset = 0;
    while (size > 1) {
        size_t pos = (size - 1) >> 1;
        auto val = extractKey(&array[offset + pos]);
        
        if (val == key)
            return &array[offset + pos];
        // The item we are looking for is smaller than the item being check; reduce the value of 'size',
        // chopping off the right hand half of the array.
        if (key < val)
            size = pos;
        // Discard all values in the left hand half of the array, up to and including the item at pos.
        else {
            size -= (pos + 1);
            offset += (pos + 1);
        }

        ASSERT(mode != KeyMustBePresentInArray || size);
    }
    
    if (mode == KeyMightNotBePresentInArray && !size)
        return 0;
    
    ArrayElementType* result = &array[offset];

    if (mode == KeyMightNotBePresentInArray && key != extractKey(result))
        return 0;

    if (mode == KeyMustBePresentInArray) {
        ASSERT(size == 1);
        ASSERT(key == extractKey(result));
    }

    return result;
}

// If the element is not found, crash if asserts are enabled, and behave like approximateBinarySearch in release builds.
template<typename ArrayElementType, typename KeyType, typename ArrayType, typename ExtractKey>
inline ArrayElementType* binarySearch(ArrayType& array, size_t size, KeyType key, ExtractKey extractKey = ExtractKey())
{
    return binarySearchImpl<ArrayElementType, KeyType, ArrayType, ExtractKey, KeyMustBePresentInArray>(array, size, key, extractKey);
}

// Return zero if the element is not found.
template<typename ArrayElementType, typename KeyType, typename ArrayType, typename ExtractKey>
inline ArrayElementType* tryBinarySearch(ArrayType& array, size_t size, KeyType key, ExtractKey extractKey = ExtractKey())
{
    return binarySearchImpl<ArrayElementType, KeyType, ArrayType, ExtractKey, KeyMightNotBePresentInArray>(array, size, key, extractKey);
}

// Return the element that is either to the left, or the right, of where the element would have been found.
template<typename ArrayElementType, typename KeyType, typename ArrayType, typename ExtractKey>
inline ArrayElementType* approximateBinarySearch(ArrayType& array, size_t size, KeyType key, ExtractKey extractKey = ExtractKey())
{
    return binarySearchImpl<ArrayElementType, KeyType, ArrayType, ExtractKey, ReturnAdjacentElementIfKeyIsNotPresent>(array, size, key, extractKey);
}

// Variants of the above that use const.
template<typename ArrayElementType, typename KeyType, typename ArrayType, typename ExtractKey>
inline ArrayElementType* binarySearch(const ArrayType& array, size_t size, KeyType key, ExtractKey extractKey = ExtractKey())
{
    return binarySearchImpl<ArrayElementType, KeyType, ArrayType, ExtractKey, KeyMustBePresentInArray>(const_cast<ArrayType&>(array), size, key, extractKey);
}
template<typename ArrayElementType, typename KeyType, typename ArrayType, typename ExtractKey>
inline ArrayElementType* tryBinarySearch(const ArrayType& array, size_t size, KeyType key, ExtractKey extractKey = ExtractKey())
{
    return binarySearchImpl<ArrayElementType, KeyType, ArrayType, ExtractKey, KeyMightNotBePresentInArray>(const_cast<ArrayType&>(array), size, key, extractKey);
}
template<typename ArrayElementType, typename KeyType, typename ArrayType, typename ExtractKey>
inline ArrayElementType* approximateBinarySearch(const ArrayType& array, size_t size, KeyType key, ExtractKey extractKey = ExtractKey())
{
    return binarySearchImpl<ArrayElementType, KeyType, ArrayType, ExtractKey, ReturnAdjacentElementIfKeyIsNotPresent>(const_cast<ArrayType&>(array), size, key, extractKey);
}

template<typename VectorType, typename ElementType>
inline void insertIntoBoundedVector(VectorType& vector, size_t size, const ElementType& element, size_t index)
{
    for (size_t i = size; i-- > index + 1;)
        vector[i] = vector[i - 1];
    vector[index] = element;
}

// This is here instead of CompilationThread.h to prevent that header from being included
// everywhere. The fact that this method, and that header, exist outside of JSC is a bug.
// https://bugs.webkit.org/show_bug.cgi?id=131815
WTF_EXPORT_PRIVATE bool isCompilationThread();

template<typename Func>
constexpr bool isStatelessLambda()
{
    return std::is_empty<Func>::value;
}

template<typename ResultType, typename Func, typename... ArgumentTypes>
ResultType callStatelessLambda(ArgumentTypes&&... arguments)
{
    uint64_t data[(sizeof(Func) + sizeof(uint64_t) - 1) / sizeof(uint64_t)];
    memset(data, 0, sizeof(data));
    return (*reinterpret_cast<Func*>(data))(std::forward<ArgumentTypes>(arguments)...);
}

template<typename T, typename U>
bool checkAndSet(T& left, U right)
{
    if (left == right)
        return false;
    left = right;
    return true;
}

template<typename T>
inline unsigned ctz(T value); // Clients will also need to #include MathExtras.h

template<typename T>
bool findBitInWord(T word, size_t& startOrResultIndex, size_t endIndex, bool value)
{
    static_assert(std::is_unsigned<T>::value, "Type used in findBitInWord must be unsigned");

    constexpr size_t bitsInWord = sizeof(word) * CHAR_BIT;
    ASSERT_UNUSED(bitsInWord, startOrResultIndex <= bitsInWord && endIndex <= bitsInWord);

    size_t index = startOrResultIndex;
    word >>= index;

#if CPU(X86_64) || CPU(ARM64)
    // We should only use ctz() when we know that ctz() is implementated using
    // a fast hardware instruction. Otherwise, this will actually result in
    // worse performance.

    word ^= (static_cast<T>(value) - 1);
    index += ctz(word);
    if (index < endIndex) {
        startOrResultIndex = index;
        return true;
    }
#else
    while (index < endIndex) {
        if ((word & 1) == static_cast<T>(value)) {
            startOrResultIndex = index;
            return true;
        }
        index++;
        word >>= 1;
    }
#endif

    startOrResultIndex = endIndex;
    return false;
}

// Used to check if a variadic list of compile time predicates are all true.
template<bool... Bs> inline constexpr bool all =
    std::is_same_v<std::integer_sequence<bool, true, Bs...>,
                   std::integer_sequence<bool, Bs..., true>>;

// Visitor adapted from http://stackoverflow.com/questions/25338795/is-there-a-name-for-this-tuple-creation-idiom

template<class A, class... B> struct Visitor : Visitor<A>, Visitor<B...> {
    Visitor(A a, B... b)
        : Visitor<A>(a)
        , Visitor<B...>(b...)
    {
    }

    using Visitor<A>::operator ();
    using Visitor<B...>::operator ();
};
  
template<class A> struct Visitor<A> : A {
    Visitor(A a)
        : A(a)
    {
    }

    using A::operator();
};
 
template<class... F> ALWAYS_INLINE Visitor<F...> makeVisitor(F... f)
{
    return Visitor<F...>(f...);
}

// Macros to implement switching over an integer range in chunks of 32.
// Useful for efficient implementations of variant and tuple type visiting.
// Adapted from https://www.reddit.com/r/cpp/comments/kst2pu/comment/giilcxv/.

#define WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, Min, Max, N)      \
    case Min + N:                                                   \
    {                                                               \
        if constexpr (Min + N < Max) {                              \
            return CASE(Min, Max, N);                               \
        } else {                                                    \
            WTF_UNREACHABLE();                                      \
        }                                                           \
    }                                                               \

#define WTF_UNROLLED_32_CASE_VISIT_SWITCH(INDEX, MIN, MAX, CASE, NEXT) \
    switch (INDEX) {                                                \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 0)          \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 1)          \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 2)          \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 3)          \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 4)          \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 5)          \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 6)          \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 7)          \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 8)          \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 9)          \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 10)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 11)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 12)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 13)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 14)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 15)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 16)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 17)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 18)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 19)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 20)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 21)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 22)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 23)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 24)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 25)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 26)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 27)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 28)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 29)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 30)         \
    WTF_UNROLLED_CASE_VISIT_SWITCH_CASE(CASE, MIN, MAX, 31)         \
    }                                                               \
                                                                    \
    constexpr auto nextMin = std::min(MIN + 32, MAX);               \
    if constexpr (nextMin < MAX)                                    \
        return NEXT(nextMin, MAX);                                  \
    WTF_UNREACHABLE();


// Calls a zero argument functor with a non-type template argument set to the index.
//
// e.g.
//   visitAtIndex<0 /* minimum */, 10 /* maximum */>(7,
//       []<size_t I>() {
//           if constexpr (I == 7) {
//               print("will be called");
//           } else {
//               print("will not be called");
//           }
//       }
//   );
//
template<size_t Minimum, size_t Maximum, class F> ALWAYS_INLINE decltype(auto) visitAtIndex(size_t index, NOESCAPE F&& f)
{
#define WTF_INDEX_VISIT_CASE(Min, Max, N) f.template operator()<Min + N>()
#define WTF_INDEX_VISIT_NEXT(Min, Max)    visitAtIndex<Min, Max>(index, std::forward<F>(f))

    WTF_UNROLLED_32_CASE_VISIT_SWITCH(index, Minimum, Maximum, WTF_INDEX_VISIT_CASE, WTF_INDEX_VISIT_NEXT)

#undef WTF_INDEX_VISIT_NEXT
#undef WTF_INDEX_VISIT_CASE
}

// `asVariant` is used to allow subclasses of Variant to work with `switchOn`.

template<class... Ts> ALWAYS_INLINE constexpr Variant<Ts...>& asVariant(Variant<Ts...>& v)
{
    return v;
}

template<class... Ts> ALWAYS_INLINE constexpr const Variant<Ts...>& asVariant(const Variant<Ts...>& v)
{
    return v;
}

template<class... Ts> ALWAYS_INLINE constexpr Variant<Ts...>&& asVariant(Variant<Ts...>&& v)
{
    return std::move(v);
}

template<class... Ts> ALWAYS_INLINE constexpr const Variant<Ts...>&& asVariant(const Variant<Ts...>&& v)
{
    return std::move(v);
}

template<typename T> concept HasSwitchOn = requires(T t) {
    t.switchOn([](const auto&) {});
};

#ifdef _LIBCPP_VERSION

// Single-variant switch-based visit function adapted from https://www.reddit.com/r/cpp/comments/kst2pu/comment/giilcxv/.
// Works around bad code generation for WTF::visit with one Variant by some standard library / compilers that
// lead to excessive binary size growth. Currently only needed by libc++. See https://webkit.org/b/279498.


template<size_t Minimum = 0, class F, class V> ALWAYS_INLINE decltype(auto) visitOneVariant(NOESCAPE F&& f, V&& v)
{
    constexpr auto Maximum = VariantSizeV<std::remove_cvref_t<V>>;

#define WTF_INDEX_VISIT_CASE(Min, Max, N) f(std::get<Min + N>(std::forward<V>(v)))
#define WTF_INDEX_VISIT_NEXT(Min, Max)    visitOneVariant<Min>(std::forward<F>(f), std::forward<V>(v))

    WTF_UNROLLED_32_CASE_VISIT_SWITCH(v.index(), Minimum, Maximum, WTF_INDEX_VISIT_CASE, WTF_INDEX_VISIT_NEXT)

#undef WTF_INDEX_VISIT_NEXT
#undef WTF_INDEX_VISIT_CASE
}

template<class V, class... F> requires (!HasSwitchOn<V>) ALWAYS_INLINE auto switchOn(V&& v, F&&... f) -> decltype(visitOneVariant(makeVisitor(std::forward<F>(f)...), asVariant(std::forward<V>(v))))
{
    return visitOneVariant(makeVisitor(std::forward<F>(f)...), asVariant(std::forward<V>(v)));
}

#else

template<class V, class... F> requires (!HasSwitchOn<V>) ALWAYS_INLINE auto switchOn(V&& v, F&&... f) -> decltype(WTF::visit(makeVisitor(std::forward<F>(f)...), asVariant(std::forward<V>(v))))
{
    return WTF::visit(makeVisitor(std::forward<F>(f)...), asVariant(std::forward<V>(v)));
}

#endif

template<class V, class... F> requires (HasSwitchOn<V>) ALWAYS_INLINE auto switchOn(V&& v, F&&... f) -> decltype(v.switchOn(std::forward<F>(f)...))
{
    return v.switchOn(std::forward<F>(f)...);
}

// Implementation of std::variant_alternative_index from https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p2527r3.html.

namespace detail {

template<size_t, class, class> struct alternative_index_helper;

template<size_t index, class Type, class T>
struct alternative_index_helper<index, Type, Variant<T>> {
    static constexpr size_t count = std::is_same_v<Type, T>;
    static constexpr size_t value = index;
};

template<size_t index, class Type, class T, class... Types>
struct alternative_index_helper<index, Type, Variant<T, Types...>> {
    static constexpr size_t count = std::is_same_v<Type, T> + alternative_index_helper<index + 1, Type, Variant<Types...>>::count;
    static constexpr size_t value = std::is_same_v<Type, T> ? index : alternative_index_helper<index + 1, Type, Variant<Types...>>::value;
};

} // namespace detail

template<class T, class Variant> struct variant_alternative_index;

template<class T, class... Types> struct variant_alternative_index<T, Variant<Types...>>
    : std::integral_constant<size_t, detail::alternative_index_helper<0, T, Variant<Types...>>::value> {
    static_assert(detail::alternative_index_helper<0, T, std::remove_cv_t<Variant<Types...>>>::count == 1);
};

template<class T, class Variant> constexpr std::size_t alternativeIndexV = variant_alternative_index<T, Variant>::value;

// `holdsAlternative<T/I>` are WTF namespaced versions of `std::holds_alternative<T/I>` that work with any "variant-like".

// Default implementation expects "variant-like" to have "holdsAlternative" member functions.
template<typename V> struct HoldsAlternative {
    template<typename T> static constexpr bool holdsAlternative(const V& v)
    {
        return v.template holdsAlternative<T>();
    }
    template<size_t I> static constexpr bool holdsAlternative(const V& v)
    {
        return v.template holdsAlternative<I>();
    }
};

// Specialization for `Variant`.
template<typename... Ts> struct HoldsAlternative<Variant<Ts...>> {
    template<typename T> static constexpr bool holdsAlternative(const Variant<Ts...>& v)
    {
        return std::holds_alternative<T>(v);
    }
    template<size_t I> static constexpr bool holdsAlternative(const Variant<Ts...>& v)
    {
        return std::holds_alternative<I>(v);
    }
};

template<typename T, typename V> bool holdsAlternative(const V& v)
{
    return HoldsAlternative<V>::template holdsAlternative<T>(v);
}

template<size_t I, typename V> bool holdsAlternative(const V& v)
{
    return HoldsAlternative<V>::template holdsAlternative<I>(v);
}

// MARK: - Utility macro for wrapping a variant in a struct

#define FORWARD_VARIANT_FUNCTIONS(Self, name)                                        \
    size_t index() const                                                             \
    {                                                                                \
        return name.index();                                                         \
    }                                                                                \
    template<typename... F> decltype(auto) switchOn(F&&... f) const                  \
    {                                                                                \
        return WTF::switchOn(name, std::forward<F>(f)...);                           \
    }                                                                                \
    template<typename... F> decltype(auto) switchOn(F&&... f)                        \
    {                                                                                \
        return WTF::switchOn(name, std::forward<F>(f)...);                           \
    }                                                                                \
    template<typename T> bool holdsAlternative() const                               \
    {                                                                                \
        return WTF::holdsAlternative<T>(value);                                      \
    }                                                                                \
    template<typename T> friend T& get(Self& self)                                   \
    {                                                                                \
        return std::get<T>(self.name);                                               \
    }                                                                                \
    template<typename T> friend T&& get(Self&& self)                                 \
    {                                                                                \
        return std::get<T>(WTFMove(self.name));                                      \
    }                                                                                \
    template<typename T> friend const T& get(const Self& self)                       \
    {                                                                                \
        return std::get<T>(self.name);                                               \
    }                                                                                \
    template<typename T> friend const T&& get(const Self&& self)                     \
    {                                                                                \
        return std::get<T>(WTFMove(self.name));                                      \
    }                                                                                \
    template<typename T> friend std::add_pointer_t<T> get_if(Self* self)             \
    {                                                                                \
        return std::get_if<T>(&self->name);                                          \
    }                                                                                \
    template<typename T> friend std::add_pointer_t<const T> get_if(const Self* self) \
    {                                                                                \
        return std::get_if<T>(&self->name);                                          \
    }

// MARK: - Utility types for working with Variants in generic contexts

// Wraps a type list using a Variant.
template<typename... Ts> using VariantWrapper = Variant<Ts...>;

// Is conditionally either a single type, if the type list only has a single element, or a Variant of the type list's contents.
template<typename TypeList> using VariantOrSingle = std::conditional_t<
    brigand::size<TypeList>::value == 1,
    brigand::front<TypeList>,
    brigand::wrap<TypeList, VariantWrapper>
>;

// Concepts / traits for data structures that use std::in_place_type_t/std::in_place_index_t so that they can
// check that generic arguments in overloads are not std::in_place_type_t/std::in_place_index_t.
//
// e.g.
//
//    struct Foo {
//        template<typename U> constexpr Foo(U&& value)
//            requires (!IsStdInPlaceTypeV<std::remove_cvref_t<U>>)
//                  && (!IsStdInPlaceIndexV<std::remove_cvref_t<U>>)
//        {
//            ...
//        }
//
//        template<typename T, typename... Args> constexpr Foo(std::in_place_type_t<T>, Args&&... args)
//        {
//            ...
//        }
//
//        template<size_t I, typename... Args> constexpr Foo(std::in_place_index_t<I>, Args&&... args)
//        {
//            ...
//        }
//
//        ...
//   };

template<typename T> struct IsStdInPlaceTypeImpl : std::false_type {};
template<typename T> struct IsStdInPlaceTypeImpl<WTF::InPlaceTypeT<T>> : std::true_type { };
template<typename T> using IsStdInPlaceType = IsStdInPlaceTypeImpl<std::remove_cvref_t<T>>;
template<typename T> constexpr bool IsStdInPlaceTypeV = IsStdInPlaceType<T>::value;

template<typename T> struct IsStdInPlaceIndexImpl : std::false_type { };
template<size_t I>   struct IsStdInPlaceIndexImpl<WTF::InPlaceIndexT<I>> : std::true_type { };
template<typename T> using IsStdInPlaceIndex = IsStdInPlaceIndexImpl<std::remove_cvref_t<T>>;
template<typename T> constexpr bool IsStdInPlaceIndexV = IsStdInPlaceIndex<T>::value;

// MARK: - Runtime get<> for std::tuple and "Tuple-like" types

// Example usage:
//
//   std::tuple<int, float> foo = std::make_tuple(1, 2.0f);
//   switchOnTupleAtIndex(0,
//       [](const int& value) {
//           print("we got an int");  <--- this will get called
//       },
//       [](const int& value) {
//           print("we got an int");  <--- this will NOT get called
//       },
//   );

template<class F, class Tuple> ALWAYS_INLINE constexpr decltype(auto) visitTupleElementAtIndex(F&& f, size_t index, Tuple&& tuple)
{
    return visitAtIndex<0, std::tuple_size_v<std::remove_cvref_t<Tuple>>>(
        index,
        [&]<size_t I>() ALWAYS_INLINE_LAMBDA {
            return std::invoke(std::forward<F>(f), std::get<I>(std::forward<Tuple>(tuple)));
        }
    );
}

template<typename Tuple, typename... F> ALWAYS_INLINE constexpr auto switchOnTupleAtIndex(size_t index, Tuple&& tuple, F&&... f) -> decltype(visitTupleElementAtIndex(index, WTF::makeVisitor(std::forward<F>(f)...), std::forward<Tuple>(tuple)))
{
    return visitTupleElementAtIndex(WTF::makeVisitor(std::forward<F>(f)...), index, std::forward<Tuple>(tuple));
}

namespace Detail
{
    template <typename, template <typename...> class>
    struct IsTemplate_ : std::false_type
    {
    };

    template <typename... Ts, template <typename...> class C>
    struct IsTemplate_<C<Ts...>, C> : std::true_type
    {
    };
}

template <typename T, template <typename...> class Template>
struct IsTemplate : public std::integral_constant<bool, Detail::IsTemplate_<T, Template>::value> {};

namespace Detail
{
    template <template <typename...> class Base, typename Derived>
    struct IsBaseOfTemplateImpl
    {
        template <typename... Args>
        static std::true_type test(Base<Args...>*);
        static std::false_type test(void*);

        static constexpr const bool value = decltype(test(std::declval<typename std::remove_cv<Derived>::type*>()))::value;
    };
}

template <template <typename...> class Base, typename Derived>
struct IsBaseOfTemplate : public std::integral_constant<bool, Detail::IsBaseOfTemplateImpl<Base, Derived>::value> {};

// Based on 'Detecting in C++ whether a type is defined, part 3: SFINAE and incomplete types'
// <https://devblogs.microsoft.com/oldnewthing/20190710-00/?p=102678>
template<typename, typename = void> inline constexpr bool IsTypeComplete = false;
template<typename T> inline constexpr bool IsTypeComplete<T, std::void_t<decltype(sizeof(T))>> = true;

template<typename IteratorTypeLeft, typename IteratorTypeRight, typename IteratorTypeDst>
IteratorTypeDst mergeDeduplicatedSorted(IteratorTypeLeft leftBegin, IteratorTypeLeft leftEnd, IteratorTypeRight rightBegin, IteratorTypeRight rightEnd, IteratorTypeDst dstBegin)
{
    IteratorTypeLeft leftIter = leftBegin;
    IteratorTypeRight rightIter = rightBegin;
    IteratorTypeDst dstIter = dstBegin;
    
    if (leftIter < leftEnd && rightIter < rightEnd) {
        for (;;) {
            auto left = *leftIter;
            auto right = *rightIter;
            if (left < right) {
                *dstIter++ = left;
                leftIter++;
                if (leftIter >= leftEnd)
                    break;
            } else if (left == right) {
                *dstIter++ = left;
                leftIter++;
                rightIter++;
                if (leftIter >= leftEnd || rightIter >= rightEnd)
                    break;
            } else {
                *dstIter++ = right;
                rightIter++;
                if (rightIter >= rightEnd)
                    break;
            }
        }
    }
    
    while (leftIter < leftEnd)
        *dstIter++ = *leftIter++;
    while (rightIter < rightEnd)
        *dstIter++ = *rightIter++;
    
    return dstIter;
}

} // namespace WTF

// This version of placement new omits a 0 check.
enum NotNullTag { NotNull };
inline void* operator new(size_t, NotNullTag, void* location)
{
    ASSERT(location);
    return location;
}

namespace std {

template<WTF::CheckMoveParameterTag, typename T>
ALWAYS_INLINE constexpr typename remove_reference<T>::type&& move(T&& value)
{
    static_assert(is_lvalue_reference<T>::value, "T is not an lvalue reference; move() is unnecessary.");

    using NonRefQualifiedType = typename remove_reference<T>::type;
    static_assert(!is_const<NonRefQualifiedType>::value, "T is const qualified.");

    return move(forward<T>(value));
}

} // namespace std

namespace WTF {

template<class T, class... Args>
ALWAYS_INLINE decltype(auto) makeUnique(Args&&... args)
{
    static_assert(std::is_same<typename T::WTFIsFastMallocAllocated, int>::value, "T should use FastMalloc (WTF_DEPRECATED_MAKE_FAST_ALLOCATED)");
    static_assert(!HasRefPtrMemberFunctions<T>::value, "T should not be RefCounted");
    return std::make_unique<T>(std::forward<Args>(args)...);
}

// This function is useful when constructing an object that is forwarding its ref-counting to its
// owner. The function returns a `const std::unique_ptr<>` so that it cannot be reassigned. In
// case of reassignment, ref-counting forwarding wouldn't be safe. This function is commonly used
// with `lazyInitialize()` to initialize a const data member.
template<class T, class U = T, class... Args>
ALWAYS_INLINE const std::unique_ptr<U> makeUniqueWithoutRefCountedCheck(Args&&... args)
{
    static_assert(std::is_same<typename T::WTFIsFastMallocAllocated, int>::value, "T should use FastMalloc (WTF_DEPRECATED_MAKE_FAST_ALLOCATED)");
    return std::unique_ptr<U>(std::make_unique<T>(std::forward<Args>(args)...));
}

template<class T, class... Args>
ALWAYS_INLINE decltype(auto) makeUniqueWithoutFastMallocCheck(Args&&... args)
{
    static_assert(!HasRefPtrMemberFunctions<T>::value, "T should not be RefCounted");
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename ResultType, size_t... Is, typename ...Args>
constexpr auto constructFixedSizeArrayWithArgumentsImpl(std::index_sequence<Is...>, Args&&... args) -> std::array<ResultType, sizeof...(Is)>
{
    return { ((void)Is, ResultType { std::forward<Args>(args)... })... };
}

// Construct an std::array with N elements of ResultType, passing Args to each of the N constructors.
template<typename ResultType, size_t N, typename ...Args>
constexpr auto constructFixedSizeArrayWithArguments(Args&&... args) -> decltype(auto)
{
    auto tuple = std::make_index_sequence<N>();
    return constructFixedSizeArrayWithArgumentsImpl<ResultType>(tuple, std::forward<Args>(args)...);
}

template<typename OptionalType> typename OptionalType::value_type valueOrCompute(OptionalType optional, NOESCAPE const std::invocable<> auto& callback)
{
    return optional ? *optional : callback();
}

template<typename OptionalType> auto valueOrDefault(OptionalType&& optionalValue)
{
    return optionalValue ? *std::forward<OptionalType>(optionalValue) : std::remove_reference_t<decltype(*optionalValue)> { };
}

// Less preferred helper function for converting an imported API into a span.
// Use this when we can't edit the imported API and it doesn't offer
// begin() / end() or a span accessor.
template<typename T, std::size_t Extent = std::dynamic_extent>
inline constexpr auto unsafeMakeSpan(T* ptr, size_t size)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    return std::span<T, Extent> { ptr, size };
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
template<typename T, std::size_t Extent, typename U>
constexpr std::span<T, Extent == std::dynamic_extent ? std::dynamic_extent : (sizeof(U) * Extent) / sizeof(T)> spanReinterpretCast(std::span<U, Extent> span)
{
    static_assert(std::is_const_v<T> || (!std::is_const_v<T> && !std::is_const_v<U>), "spanReinterpretCast will not remove constness from source");

    if constexpr (Extent == std::dynamic_extent) {
        if constexpr (sizeof(U) < sizeof(T) || sizeof(U) % sizeof(T))
            RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!(span.size_bytes() % sizeof(T))); // Refuse to change size in bytes from source.
    } else
        static_assert(!((sizeof(U) * Extent) % sizeof(T)), "spanReinterpretCast will not change size in bytes from source");

    using ReturnType = std::span<T, Extent == std::dynamic_extent ? std::dynamic_extent : (sizeof(U) * Extent) / sizeof(T)>;
    return ReturnType { reinterpret_cast<T*>(const_cast<std::remove_const_t<U>*>(span.data())), span.size_bytes() / sizeof(T) };
}
#pragma GCC diagnostic pop

template<typename U, typename T, std::size_t Extent>
std::span<U, Extent> spanConstCast(std::span<T, Extent> span)
{
    return std::span<U, Extent> { const_cast<U*>(span.data()), span.size() };
}

template<typename T, std::size_t Extent>
std::span<const uint8_t, Extent == std::dynamic_extent ? std::dynamic_extent: Extent * sizeof(T)> asBytes(std::span<T, Extent> span)
{
    return std::span<const uint8_t, Extent == std::dynamic_extent ? std::dynamic_extent: Extent * sizeof(T)> { reinterpret_cast<const uint8_t*>(span.data()), span.size_bytes() };
}

template<typename T, std::size_t Extent>
std::span<uint8_t, Extent == std::dynamic_extent ? std::dynamic_extent: Extent * sizeof(T)> asWritableBytes(std::span<T, Extent> span)
{
    return std::span<uint8_t, Extent == std::dynamic_extent ? std::dynamic_extent: Extent * sizeof(T)> { reinterpret_cast<uint8_t*>(span.data()), span.size_bytes() };
}

template<typename T>
std::span<T> singleElementSpan(T& object)
{
    return unsafeMakeSpan(std::addressof(object), 1);
}

template<typename T, std::size_t Extent = std::dynamic_extent>
std::span<const uint8_t, Extent> asByteSpan(const T& input)
{
    return unsafeMakeSpan<const uint8_t, Extent>(reinterpret_cast<const uint8_t*>(&input), sizeof(input));
}

template<typename T, std::size_t Extent>
std::span<const uint8_t> asByteSpan(std::span<T, Extent> input)
{
    return unsafeMakeSpan(reinterpret_cast<const uint8_t*>(input.data()), input.size_bytes());
}

template<typename T, std::size_t Extent = std::dynamic_extent>
std::span<uint8_t, Extent> asMutableByteSpan(T& input)
{
    static_assert(!std::is_const_v<T>);
    return unsafeMakeSpan<uint8_t, Extent>(reinterpret_cast<uint8_t*>(std::addressof(input)), sizeof(input));
}

template<typename T, std::size_t Extent>
std::span<uint8_t> asMutableByteSpan(std::span<T, Extent> input)
{
    static_assert(!std::is_const_v<T>);
    return unsafeMakeSpan(reinterpret_cast<uint8_t*>(input.data()), input.size_bytes());
}

template<typename T, typename U, std::size_t Extent>
const T& reinterpretCastSpanStartTo(std::span<const U, Extent> span)
{
    return spanReinterpretCast<const T>(asByteSpan(span).first(sizeof(T)))[0];
}

template<typename T, typename U, std::size_t Extent>
T& reinterpretCastSpanStartTo(std::span<U, Extent> span)
{
    return spanReinterpretCast<T>(asMutableByteSpan(span).first(sizeof(T)))[0];
}

enum class IgnoreTypeChecks : bool { No, Yes };

template<IgnoreTypeChecks ignoreTypeChecks = IgnoreTypeChecks::No, typename T, std::size_t TExtent, typename U, std::size_t UExtent>
bool equalSpans(std::span<T, TExtent> a, std::span<U, UExtent> b)
{
    static_assert(sizeof(T) == sizeof(U));
    static_assert(ignoreTypeChecks == IgnoreTypeChecks::Yes || std::has_unique_object_representations_v<T>);
    static_assert(ignoreTypeChecks == IgnoreTypeChecks::Yes || std::has_unique_object_representations_v<U>);
    if (a.size() != b.size())
        return false;
    return !memcmp(a.data(), b.data(), a.size_bytes()); // NOLINT
}

template<typename T, std::size_t TExtent, typename U, std::size_t UExtent>
bool spanHasPrefix(std::span<T, TExtent> span, std::span<U, UExtent> prefix)
{
    static_assert(sizeof(T) == sizeof(U));
    static_assert(std::has_unique_object_representations_v<T>);
    static_assert(std::has_unique_object_representations_v<U>);
    if (span.size() < prefix.size())
        return false;
    return !memcmp(span.data(), prefix.data(), prefix.size_bytes()); // NOLINT
}

template<typename T, std::size_t TExtent, typename U, std::size_t UExtent>
bool spanHasSuffix(std::span<T, TExtent> span, std::span<U, UExtent> suffix)
{
    static_assert(sizeof(T) == sizeof(U));
    static_assert(std::has_unique_object_representations_v<T>);
    static_assert(std::has_unique_object_representations_v<U>);
    if (span.size() < suffix.size())
        return false;
    return !memcmp(span.last(suffix.size()).data(), suffix.data(), suffix.size_bytes()); // NOLINT
}

template<typename T, std::size_t TExtent, typename U, std::size_t UExtent>
std::strong_ordering compareSpans(std::span<T, TExtent> a, std::span<U, UExtent> b)
{
    static_assert(sizeof(T) == sizeof(U));
    static_assert(std::has_unique_object_representations_v<T>);
    static_assert(std::has_unique_object_representations_v<U>);
    int result = memcmp(a.data(), b.data(), std::min(a.size_bytes(), b.size_bytes())); // NOLINT
    if (result) {
        if (result < 0)
            return std::strong_ordering::less;
        return std::strong_ordering::greater;
    }
    if (a.size() != b.size())
        return a.size() > b.size() ? std::strong_ordering::greater : std::strong_ordering::less;
    return std::strong_ordering::equal;
}

// Returns the index of the first occurrence of |needed| in |haystack| or notFound if not present.
template<typename T, std::size_t TExtent, typename U, std::size_t UExtent>
size_t find(std::span<T, TExtent> haystack, std::span<U, UExtent> needle)
{
    static_assert(sizeof(T) == 1);
    static_assert(sizeof(T) == sizeof(U));
    auto* result = static_cast<T*>(memmem(haystack.data(), haystack.size(), needle.data(), needle.size())); // NOLINT
    if (!result)
        return notFound;
    return result - haystack.data();
}

template<typename T, std::size_t TExtent, typename U, std::size_t UExtent>
size_t contains(std::span<T, TExtent> haystack, std::span<U, UExtent> needle)
{
    static_assert(sizeof(T) == 1);
    static_assert(sizeof(T) == sizeof(U));
    return !!memmem(haystack.data(), haystack.size(), needle.data(), needle.size()); // NOLINT
}

template<typename T, std::size_t TExtent, typename U, std::size_t UExtent>
void memcpySpan(std::span<T, TExtent> destination, std::span<U, UExtent> source)
{
    static_assert(sizeof(T) == sizeof(U));
    static_assert(std::is_trivially_copyable_v<T> || std::is_floating_point_v<T>);
    static_assert(std::is_trivially_copyable_v<U> || std::is_floating_point_v<U>);
    RELEASE_ASSERT(destination.size() >= source.size());
    memcpy(destination.data(), source.data(), source.size_bytes()); // NOLINT
}

template<typename T, std::size_t TExtent, typename U, std::size_t UExtent>
void memmoveSpan(std::span<T, TExtent> destination, std::span<U, UExtent> source)
{
    static_assert(sizeof(T) == sizeof(U));
    static_assert(std::is_trivially_copyable_v<T> || std::is_floating_point_v<T>);
    static_assert(std::is_trivially_copyable_v<U> || std::is_floating_point_v<U>);
    RELEASE_ASSERT(destination.size() >= source.size());
    memmove(destination.data(), source.data(), source.size_bytes()); // NOLINT
}

template<typename T, std::size_t Extent>
void memsetSpan(std::span<T, Extent> destination, uint8_t byte)
{
    static_assert(std::is_trivially_copyable_v<T>);
    memset(destination.data(), byte, destination.size_bytes()); // NOLINT
}

template<typename T, std::size_t Extent>
void zeroSpan(std::span<T, Extent> destination)
{
    static_assert(std::is_trivially_copyable_v<T> || std::is_floating_point_v<T>);
    memset(destination.data(), 0, destination.size_bytes()); // NOLINT
}

template<typename T>
void zeroBytes(T& object)
{
    zeroSpan(asMutableByteSpan(object));
}

template<typename T, std::size_t Extent>
void secureMemsetSpan(std::span<T, Extent> destination, uint8_t byte)
{
    static_assert(std::is_trivially_copyable_v<T>);
#ifdef __STDC_LIB_EXT1__
    memset_s(destination.data(), byte, destination.size_bytes()); // NOLINT
#else
    memset(destination.data(), byte, destination.size_bytes()); // NOLINT
#endif
}

template<typename T> void skip(std::span<T>& data, size_t amountToSkip)
{
    data = data.subspan(amountToSkip);
}

template<typename T> void dropLast(std::span<T>& data, size_t amountToDrop = 1)
{
    data = data.first(data.size() - amountToDrop);
}

template<typename T> T& consumeLast(std::span<T>& data)
{
    auto* last = &data.back();
    data = data.first(data.size() - 1);
    return *last;
}

template<typename T> void clampedMoveCursorWithinSpan(std::span<T>& cursor, std::span<T> container, int delta)
{
    ASSERT(cursor.data() >= container.data());
    ASSERT(std::to_address(cursor.end()) == std::to_address(container.end()));
    auto clampedNewIndex = std::clamp<int>(cursor.data() - container.data() + delta, 0, container.size());
    cursor = container.subspan(clampedNewIndex);
}

template<typename T> std::span<T> consumeSpan(std::span<T>& data, size_t amountToConsume)
{
    auto consumed = data.first(amountToConsume);
    skip(data, amountToConsume);
    return consumed;
}

template<typename T> T& consume(std::span<T>& data)
{
    T& value = data[0];
    skip(data, 1);
    return value;
}

template<typename DestinationType, typename SourceType>
match_constness_t<SourceType, DestinationType>& consumeAndReinterpretCastTo(std::span<SourceType>& data) requires(sizeof(SourceType) == 1)
{
    return spanReinterpretCast<match_constness_t<SourceType, DestinationType>>(consumeSpan(data, sizeof(DestinationType)))[0];
}

template<typename T, std::size_t TExtent, typename U, std::size_t UExtent>
bool spansOverlap(std::span<T, TExtent> a, std::span<U, UExtent> b)
{
    return static_cast<const void*>(a.data()) < static_cast<const void*>(std::to_address(b.end()))
        && static_cast<const void*>(b.data()) < static_cast<const void*>(std::to_address(a.end()));
}

/* WTF_FOR_EACH */

// https://www.scs.stanford.edu/~dm/blog/va-opt.html
#define WTF_PARENS ()
#define WTF_EXPAND(...) WTF_EXPAND4(WTF_EXPAND4(WTF_EXPAND4(WTF_EXPAND4(__VA_ARGS__))))
#define WTF_EXPAND4(...) WTF_EXPAND3(WTF_EXPAND3(WTF_EXPAND3(WTF_EXPAND3(__VA_ARGS__))))
#define WTF_EXPAND3(...) WTF_EXPAND2(WTF_EXPAND2(WTF_EXPAND2(WTF_EXPAND2(__VA_ARGS__))))
#define WTF_EXPAND2(...) WTF_EXPAND1(WTF_EXPAND1(WTF_EXPAND1(WTF_EXPAND1(__VA_ARGS__))))
#define WTF_EXPAND1(...) __VA_ARGS__
#define WTF_FOR_EACH_HELPER(macro, a1, ...) macro(a1) __VA_OPT__(, WTF_FOR_EACH_AGAIN WTF_PARENS (macro, __VA_ARGS__))
#define WTF_FOR_EACH_AGAIN() WTF_FOR_EACH_HELPER
#define WTF_FOR_EACH(macro, ...) __VA_OPT__(WTF_EXPAND(WTF_FOR_EACH_HELPER(macro, __VA_ARGS__)))

/* SAFE_PRINTF */

// https://gist.github.com/sehe/3374327
template <class T> inline typename std::enable_if<std::is_integral<T>::value, T>::type safePrintfType(T arg) { return arg; }
template <class T> inline typename std::enable_if<std::is_floating_point<T>::value, T>::type safePrintfType(T arg) { return arg; }
template <class T> inline typename std::enable_if<std::is_pointer<T>::value, T>::type safePrintfType(T arg) {
    static_assert(!std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>, char>, "char* is not bounds safe; please use a null terminated string type");
    return arg;
}

// These versions of printf reject char* but accept known null terminated
// string types, like ASCIILiteral and CString. A type can specialize
// 'safePrintfType' to advertise conversion to null terminated string.

// We do this as a macro so that we still get compile-time checking that our
// arguments match our format string.

#define SAFE_PRINTF_TYPE(...) WTF_FOR_EACH(WTF::safePrintfType, __VA_ARGS__)

#define SAFE_PRINTF(format, ...) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN \
    printf(format __VA_OPT__(, SAFE_PRINTF_TYPE(__VA_ARGS__))) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#define SAFE_FPRINTF(file, format, ...) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN \
    fprintf(file, format __VA_OPT__(, SAFE_PRINTF_TYPE(__VA_ARGS__))) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#define SAFE_SPRINTF(destinationSpan, format, ...) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN \
    snprintf(destinationSpan.data(), destinationSpan.size_bytes(), format, SAFE_PRINTF_TYPE(__VA_ARGS__)) \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

template<typename T> concept ByteType = sizeof(T) == 1 && ((std::is_integral_v<T> && !std::same_as<T, bool>) || std::same_as<T, std::byte>) && !std::is_const_v<T>;

template<typename> struct ByteCastTraits;

template<ByteType T> struct ByteCastTraits<T> {
    template<ByteType U> static constexpr U cast(T character) { return static_cast<U>(character); }
};

template<ByteType T> struct ByteCastTraits<T*> {
    template<ByteType U> static constexpr auto cast(T* pointer) { return std::bit_cast<U*>(pointer); }
};

template<ByteType T> struct ByteCastTraits<const T*> {
    template<ByteType U> static constexpr auto cast(const T* pointer) { return std::bit_cast<const U*>(pointer); }
};

template<ByteType T, size_t Extent> struct ByteCastTraits<std::span<T, Extent>> {
    template<ByteType U> static constexpr auto cast(std::span<T, Extent> span) { return spanReinterpretCast<U>(span); }
};

template<ByteType T, size_t Extent> struct ByteCastTraits<std::span<const T, Extent>> {
    template<ByteType U> static constexpr auto cast(std::span<const T, Extent> span) { return spanReinterpretCast<const U>(span); }
};

template<ByteType T, typename U> constexpr auto byteCast(const U& value)
{
    return ByteCastTraits<U>::template cast<T>(value);
}

template<typename T> constexpr auto unsignedCast(T value) requires (std::is_integral_v<T> || std::is_enum_v<T>)
{
    return static_cast<std::make_unsigned_t<T>>(value);
}

// This is like std::invocable but it takes the expected signature rather than just the arguments.
template<typename Functor, typename Signature> concept Invocable = requires(std::decay_t<Functor>&& f, std::function<Signature> expected) {
    { expected = std::move(f) };
};

// Concept for constraining to user-defined "Tuple-like" types.
//
// Based on exposition-only text in https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p2165r3.pdf
// and https://stackoverflow.com/questions/68443804/c20-concept-to-check-tuple-like-types.

template<class T, std::size_t N> concept HasTupleElement = requires(T t) {
    typename std::tuple_element_t<N, std::remove_const_t<T>>;
    { get<N>(t) } -> std::convertible_to<std::tuple_element_t<N, T>&>;
};

template<class T> concept TupleLike = !std::is_reference_v<T>
    && requires(T t) {
        typename std::tuple_size<T>::type;
        requires std::derived_from<
          std::tuple_size<T>,
          std::integral_constant<std::size_t, std::tuple_size_v<T>>
        >;
      }
    && []<std::size_t... N>(std::index_sequence<N...>) {
        return (HasTupleElement<T, N> && ...);
    }(std::make_index_sequence<std::tuple_size_v<T>>());

// This is like std::apply, but works with user-defined "Tuple-like" types as well as the
// standard ones. The only real difference between its implementation and the standard one
// is the use of un-prefixed `get`.
//
// This should be something we can remove if P2165 (https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2165r3.pdf)
// is adopted and implemented.
template<class F, class T, size_t ...I>
constexpr decltype(auto) apply_impl(F&& functor, T&& tupleLike, std::index_sequence<I...>)
{
    using std::get;
    return std::invoke(std::forward<F>(functor), get<I>(std::forward<T>(tupleLike))...);
}

template<class F, class T>
constexpr decltype(auto) apply(F&& functor, T&& tupleLike)
{
    return apply_impl(std::forward<F>(functor), std::forward<T>(tupleLike), std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<T>>> { });
}

// Utility for "zippering" tuples and tuple-like objects. Implementation based off
// https://stackoverflow.com/questions/11322095/how-to-make-a-function-that-zips-two-tuples-in-c11-stl
// and extended to support tuple-like.
//
// Example usage:
//
//   std::tuple<int, string, double> foo = { 1,   "hello",   1.5  };
//   std::tuple<double, char, float> bar = { 0.5, 'i',       0.1f };
//   std::tuple<int, string, double> baz = { 2,   "goodbye", 3.0  };
//
//   auto result = WTF::tuple_zip(foo, bar, baz);
//
//   This leaves result transposed and equal to:
//
//      std::tuple {
//          std::tuple<int, double, int>        { 1,       0.5,     2         },
//          std::tuple<string, char, string>    { "hello", 'i',     "goodbye" },
//          std::tuple<double, float, double>   { 1.5,     0.1f,    3.0       },
//      }

namespace detail {

template<std::size_t I, typename... TupleLikes> using zip_tuple_at_index_t = std::tuple<std::tuple_element_t<I, std::decay_t<TupleLikes>>...>;

template<std::size_t I, typename... TupleLikes> auto zip_tuple_at_index(TupleLikes&&... tupleLikes)
{
    return zip_tuple_at_index_t<I, TupleLikes...> { get<I>(std::forward<TupleLikes>(tupleLikes))... };
}

template<typename... TupleLikes, std::size_t... I> auto tuple_zip_impl(TupleLikes&& ... tupleLikes, std::index_sequence<I...>)
{
    return std::tuple<zip_tuple_at_index_t<I, TupleLikes...>...> {
        zip_tuple_at_index<I>(std::forward<TupleLikes>(tupleLikes)...)...
    };
}

} // namespace detail

template<typename Head, typename... Tail> auto tuple_zip(Head&& head, Tail&& ...tail)
{
    constexpr std::size_t size = std::tuple_size_v<std::decay_t<Head>>;

    static_assert(((std::tuple_size_v<std::decay_t<Tail>> == size) && ...), "Tuple size mismatch, can not zip.");

    return detail::tuple_zip_impl<Head, Tail...>(
        std::forward<Head>(head),
        std::forward<Tail>(tail)...,
        std::make_index_sequence<size>()
    );
}

template<typename WordType, std::size_t Extent, typename Func>
ALWAYS_INLINE constexpr void forEachSetBit(std::span<const WordType, Extent> bits, const Func& func)
{
    constexpr size_t wordSize = sizeof(WordType) * CHAR_BIT;
    for (size_t i = 0; i < bits.size(); ++i) {
        WordType word = bits[i];
        if (!word)
            continue;
        size_t base = i * wordSize;

#if CPU(X86_64) || CPU(ARM64)
        // We should only use ctz() when we know that ctz() is implemented using
        // a fast hardware instruction. Otherwise, this will actually result in
        // worse performance.
        while (word) {
            WordType temp = word & -word;
            size_t offset = ctz(word);
            if constexpr (std::is_same_v<IterationStatus, decltype(func(base + offset))>) {
                if (func(base + offset) == IterationStatus::Done)
                    return;
            } else
                func(base + offset);
            word ^= temp;
        }
#else
        for (size_t j = 0; j < wordSize; ++j) {
            if (word & 1) {
                if constexpr (std::is_same_v<IterationStatus, decltype(func(base + j))>) {
                    if (func(base + j) == IterationStatus::Done)
                        return;
                } else
                    func(base + j);
            }
            word >>= 1;
        }
#endif
    }
}

template<typename WordType, std::size_t Extent, typename Func>
ALWAYS_INLINE constexpr void forEachSetBit(std::span<const WordType, Extent> bits, size_t startIndex, const Func& func)
{
    constexpr size_t wordSize = sizeof(WordType) * CHAR_BIT;
    auto iterate = [&](WordType word, size_t i) ALWAYS_INLINE_LAMBDA {
        size_t base = i * wordSize;

#if CPU(X86_64) || CPU(ARM64)
        // We should only use ctz() when we know that ctz() is implementated using
        // a fast hardware instruction. Otherwise, this will actually result in
        // worse performance.
        while (word) {
            WordType temp = word & -word;
            size_t offset = ctz(word);
            if constexpr (std::is_same_v<IterationStatus, decltype(func(base + offset))>) {
                if (func(base + offset) == IterationStatus::Done)
                    return;
            } else
                func(base + offset);
            word ^= temp;
        }
#else
        for (size_t j = 0; j < wordSize; ++j) {
            if (word & 1) {
                if constexpr (std::is_same_v<IterationStatus, decltype(func(base + j))>) {
                    if (func(base + j) == IterationStatus::Done)
                        return;
                } else
                    func(base + j);
            }
            word >>= 1;
        }
#endif
    };

    size_t startWord = startIndex / wordSize;
    if (startWord >= bits.size())
        return;

    WordType word = bits[startWord];
    size_t startIndexInWord = startIndex - startWord * wordSize;
    WordType masked = word & (~((static_cast<WordType>(1) << startIndexInWord) - 1));
    if (masked)
        iterate(masked, startWord);

    for (size_t i = startWord + 1; i < bits.size(); ++i) {
        WordType word = bits[i];
        if (!word)
            continue;
        iterate(word, i);
    }
}

template<typename Object, typename Allocator = FastMalloc, typename... Arguments> std::pair<Object*, void*> createWithTrailingBytes(size_t trailingBytesSize, Arguments... arguments)
{
    Object* object = static_cast<Object*>(Allocator::malloc(sizeof(Object) + trailingBytesSize));
    new (NotNull, object) Object(std::forward<Arguments>(arguments)...);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    return { object, object + 1 };
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

template<typename Object> std::pair<Object*, void*> fromTrailingBytes(void* trailingBytes)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    Object* object = static_cast<Object*>(trailingBytes) - 1;
    return { object, object + 1 };
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

template<typename Object, typename Allocator = FastMalloc> std::pair<Object*, void*> reallocWithTrailingBytes(Object* object, size_t newTrailingBytesSize)
{
    size_t newAllocationSize = sizeof(Object) + newTrailingBytesSize;
    object = static_cast<Object*>(Allocator::realloc(object, newAllocationSize));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    return { object, object + 1 };
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

template<typename Object, typename Allocator = FastMalloc> void destroyWithTrailingBytes(Object* object)
{
    object->~Object();
    Allocator::free(object);
}

template<typename T, typename U>
ALWAYS_INLINE void lazyInitialize(const std::unique_ptr<T>& ptr, const std::unique_ptr<U>&& obj)
{
    RELEASE_ASSERT(!ptr);
    const_cast<std::unique_ptr<T>&>(ptr) = std::move(const_cast<std::unique_ptr<U>&&>(obj));
}

ALWAYS_INLINE std::optional<double> stringToDouble(std::span<const char> buffer, size_t& parsedLength)
{
    RELEASE_ASSERT(buffer.back() == '\0');
    char* end;
    auto result = std::strtod(buffer.data(), &end);
    if (errno == ERANGE) {
        parsedLength = 0;
        return std::nullopt;
    }
    parsedLength = end - buffer.data();
    return result;
}

ALWAYS_INLINE std::weak_ordering weakOrderingCast(std::partial_ordering ordering)
{
    RELEASE_ASSERT(ordering != std::partial_ordering::unordered);
    if (is_eq(ordering))
        return std::weak_ordering::equivalent;
    return is_lt(ordering) ? std::weak_ordering::less : std::weak_ordering::greater;
}

} // namespace WTF

#define WTFMove(value) std::move<WTF::CheckMoveParameter>(value)

namespace WTF {
namespace detail {
template<typename T, typename U> using copy_const = std::conditional_t<std::is_const_v<T>, const U, U>;
template<typename T, typename U> using override_ref = std::conditional_t<std::is_rvalue_reference_v<T>, std::remove_reference_t<U>&&, U&>;
template<typename T, typename U> using forward_like_impl = override_ref<T&&, copy_const<std::remove_reference_t<T>, std::remove_reference_t<U>>>;
} // namespace detail
template<typename T, typename U> constexpr auto forward_like(U&& value) -> detail::forward_like_impl<T, U> { return static_cast<detail::forward_like_impl<T, U>>(value); }
} // namespace WTF

using WTF::GB;
using WTF::KB;
using WTF::MB;
using WTF::approximateBinarySearch;
using WTF::asBytes;
using WTF::asByteSpan;
using WTF::asMutableByteSpan;
using WTF::asWritableBytes;
using WTF::binarySearch;
using WTF::byteCast;
using WTF::callStatelessLambda;
using WTF::checkAndSet;
using WTF::clampedMoveCursorWithinSpan;
using WTF::compareSpans;
using WTF::constructFixedSizeArrayWithArguments;
using WTF::consume;
using WTF::consumeAndReinterpretCastTo;
using WTF::consumeLast;
using WTF::consumeSpan;
using WTF::contains;
using WTF::dropLast;
using WTF::equalSpans;
using WTF::find;
using WTF::findBitInWord;
using WTF::insertIntoBoundedVector;
using WTF::is8ByteAligned;
using WTF::isCompilationThread;
using WTF::isPointerAligned;
using WTF::isStatelessLambda;
using WTF::lazyInitialize;
using WTF::makeUnique;
using WTF::makeUniqueWithoutFastMallocCheck;
using WTF::makeUniqueWithoutRefCountedCheck;
using WTF::memcpySpan;
using WTF::memmoveSpan;
using WTF::memsetSpan;
using WTF::mergeDeduplicatedSorted;
using WTF::reinterpretCastSpanStartTo;
using WTF::secureMemsetSpan;
using WTF::singleElementSpan;
using WTF::skip;
using WTF::spanConstCast;
using WTF::spanHasPrefix;
using WTF::spanHasSuffix;
using WTF::spansOverlap;
using WTF::spanReinterpretCast;
using WTF::stringToDouble;
using WTF::toTwosComplement;
using WTF::tryBinarySearch;
using WTF::unsafeMakeSpan;
using WTF::unsignedCast;
using WTF::valueOrCompute;
using WTF::valueOrDefault;
using WTF::weakOrderingCast;
using WTF::zeroBytes;
using WTF::zeroSpan;
using WTF::Invocable;
using WTF::VariantWrapper;
using WTF::VariantOrSingle;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
