/*
 * Copyright (C) 2008-2024 Apple Inc. All Rights Reserved.
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
#include <climits>
#include <concepts>
#include <cstring>
#include <functional>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>
#include <wtf/Assertions.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Compiler.h>
#include <wtf/GetPtr.h>
#include <wtf/IterationStatus.h>
#include <wtf/TypeCasts.h>

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

template<typename ToType, typename FromType>
constexpr inline ToType bitwise_cast(FromType from)
{
    static_assert(sizeof(FromType) == sizeof(ToType), "bitwise_cast size of FromType and ToType must be equal!");
#if COMPILER_SUPPORTS(BUILTIN_IS_TRIVIALLY_COPYABLE)
    // Not all recent STL implementations support the std::is_trivially_copyable type trait. Work around this by only checking on toolchains which have the equivalent compiler intrinsic.
    static_assert(__is_trivially_copyable(ToType), "bitwise_cast of non-trivially-copyable type!");
    static_assert(__is_trivially_copyable(FromType), "bitwise_cast of non-trivially-copyable type!");
#endif
    typename std::remove_const<ToType>::type to { };
    std::memcpy(static_cast<void*>(&to), static_cast<void*>(&from), sizeof(to));
    return to;
}

template<typename ToType, typename FromType>
inline ToType safeCast(FromType value)
{
    RELEASE_ASSERT(isInBounds<ToType>(value));
    return static_cast<ToType>(value);
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

inline constexpr bool isPowerOfTwo(size_t size) { return !(size & (size - 1)); }

template<typename T> constexpr T mask(T value, uintptr_t mask)
{
    static_assert(sizeof(T) == sizeof(uintptr_t), "sizeof(T) must be equal to sizeof(uintptr_t).");
    return static_cast<T>(static_cast<uintptr_t>(value) & mask);
}

template<typename T> inline T* mask(T* value, uintptr_t mask)
{
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(value) & mask);
}

template<typename T, typename U>
ALWAYS_INLINE constexpr T roundUpToMultipleOfImpl(U divisor, T x)
{
    T remainderMask = static_cast<T>(divisor) - 1;
    return (x + remainderMask) & ~remainderMask;
}

// Efficient implementation that takes advantage of powers of two.
template<typename T, typename U>
inline constexpr T roundUpToMultipleOf(U divisor, T x)
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(divisor && isPowerOfTwo(divisor));
    return roundUpToMultipleOfImpl<T, U>(divisor, x);
}

template<size_t divisor> constexpr size_t roundUpToMultipleOf(size_t x)
{
    static_assert(divisor && isPowerOfTwo(divisor));
    return roundUpToMultipleOfImpl(divisor, x);
}

template<size_t divisor, typename T> inline constexpr T* roundUpToMultipleOf(T* x)
{
    static_assert(sizeof(T*) == sizeof(size_t));
    return reinterpret_cast<T*>(roundUpToMultipleOf<divisor>(reinterpret_cast<size_t>(x)));
}

template<typename T, typename U>
inline constexpr T roundUpToMultipleOfNonPowerOfTwo(U divisor, T x)
{
    T remainder = x % divisor;
    if (!remainder)
        return x;
    return x + static_cast<T>(divisor - remainder);
}

template<typename T, typename C>
inline constexpr Checked<T, C> roundUpToMultipleOfNonPowerOfTwo(Checked<T, C> divisor, Checked<T, C> x)
{
    if (x.hasOverflowed() || divisor.hasOverflowed())
        return ResultOverflowed;
    T remainder = x % divisor;
    if (!remainder)
        return x;
    return x + static_cast<T>(divisor.value() - remainder);
}

template<typename T, typename U>
inline constexpr T roundDownToMultipleOf(U divisor, T x)
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(divisor && isPowerOfTwo(divisor));
    static_assert(sizeof(T) == sizeof(uintptr_t), "sizeof(T) must be equal to sizeof(uintptr_t).");
    return static_cast<T>(mask(static_cast<uintptr_t>(x), ~(divisor - 1ul)));
}

template<typename T> inline constexpr T* roundDownToMultipleOf(size_t divisor, T* x)
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(isPowerOfTwo(divisor));
    return reinterpret_cast<T*>(mask(reinterpret_cast<uintptr_t>(x), ~(divisor - 1ul)));
}

template<size_t divisor, typename T> constexpr T roundDownToMultipleOf(T x)
{
    static_assert(isPowerOfTwo(divisor), "'divisor' must be a power of two.");
    return roundDownToMultipleOf(divisor, x);
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
    return (*bitwise_cast<Func*>(data))(std::forward<ArgumentTypes>(arguments)...);
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

// `asVariant` is used to allow subclasses of std::variant to work with `switchOn`.

template<class... Ts> ALWAYS_INLINE constexpr std::variant<Ts...>& asVariant(std::variant<Ts...>& v)
{
    return v;
}

template<class... Ts> ALWAYS_INLINE constexpr const std::variant<Ts...>& asVariant(const std::variant<Ts...>& v)
{
    return v;
}

template<class... Ts> ALWAYS_INLINE constexpr std::variant<Ts...>&& asVariant(std::variant<Ts...>&& v)
{
    return std::move(v);
}

template<class... Ts> ALWAYS_INLINE constexpr const std::variant<Ts...>&& asVariant(const std::variant<Ts...>&& v)
{
    return std::move(v);
}

#ifdef _LIBCPP_VERSION

// Single-variant switch-based visit function adapted from https://www.reddit.com/r/cpp/comments/kst2pu/comment/giilcxv/.
// Works around bad code generation for std::visit with one std::variant by some standard library / compilers that
// lead to excessive binary size growth. Currently only needed by libc++. See https://webkit.org/b/279498.

template<size_t I = 0, class F, class V> ALWAYS_INLINE decltype(auto) visitOneVariant(F&& f, V&& v)
{
    constexpr auto size = std::variant_size_v<std::remove_cvref_t<V>>;

#define WTF_VISIT_CASE_COUNT 32
#define WTF_VISIT_CASE(N, D) \
        case I + N:                                                                                 \
        {                                                                                           \
            if constexpr (I + N < size) {                                                           \
                return std::invoke(std::forward<F>(f), std::get<I + N>(std::forward<V>(v)));        \
            } else {                                                                                \
                WTF_UNREACHABLE()                                                                   \
            }                                                                                       \
        }                                                                                           \

    switch (v.index()) {
        WTF_VISIT_CASE(0, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(1, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(2, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(3, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(4, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(5, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(6, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(7, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(8, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(9, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(10, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(11, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(12, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(13, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(14, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(15, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(16, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(17, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(18, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(19, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(20, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(21, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(22, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(23, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(24, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(25, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(26, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(27, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(28, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(29, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(30, WTF_VISIT_CASE_COUNT)
        WTF_VISIT_CASE(31, WTF_VISIT_CASE_COUNT)
    }

    constexpr auto nextI = std::min(I + WTF_VISIT_CASE_COUNT, size);

    if constexpr (nextI < size)
        return visitOneVariant<nextI>(std::forward<F>(f), std::forward<V>(v));

    WTF_UNREACHABLE();

#undef WTF_VISIT_CASE_COUNT
#undef WTF_VISIT_CASE
}

template<class V, class... F> ALWAYS_INLINE auto switchOn(V&& v, F&&... f) -> decltype(visitOneVariant(makeVisitor(std::forward<F>(f)...), asVariant(std::forward<V>(v))))
{
    return visitOneVariant(makeVisitor(std::forward<F>(f)...), asVariant(std::forward<V>(v)));
}

#else

template<class V, class... F> ALWAYS_INLINE auto switchOn(V&& v, F&&... f) -> decltype(std::visit(makeVisitor(std::forward<F>(f)...), asVariant(std::forward<V>(v))))
{
    return std::visit(makeVisitor(std::forward<F>(f)...), asVariant(std::forward<V>(v)));
}

#endif

namespace detail {

template<size_t, class, class> struct alternative_index_helper;

template<size_t index, class Type, class T>
struct alternative_index_helper<index, Type, std::variant<T>> {
    static constexpr size_t count = std::is_same_v<Type, T>;
    static constexpr size_t value = index;
};

template<size_t index, class Type, class T, class... Types>
struct alternative_index_helper<index, Type, std::variant<T, Types...>> {
    static constexpr size_t count = std::is_same_v<Type, T> + alternative_index_helper<index + 1, Type, std::variant<Types...>>::count;
    static constexpr size_t value = std::is_same_v<Type, T> ? index : alternative_index_helper<index + 1, Type, std::variant<Types...>>::value;
};

} // namespace detail

template<class T, class Variant> struct variant_alternative_index;

template<class T, class... Types> struct variant_alternative_index<T, std::variant<Types...>>
    : std::integral_constant<size_t, detail::alternative_index_helper<0, T, std::variant<Types...>>::value> {
    static_assert(detail::alternative_index_helper<0, T, std::remove_cv_t<std::variant<Types...>>>::count == 1);
};

template<class T, class Variant> constexpr std::size_t alternativeIndexV = variant_alternative_index<T, Variant>::value;

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

// libstdc++5 does not have constexpr std::tie. Since we cannot redefine std::tie with constexpr, we define WTF::tie instead.
// This workaround can be removed after 2019-04 and all users of WTF::tie can be converted to std::tie
// For more info see: https://bugs.webkit.org/show_bug.cgi?id=180692 and https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65978
template <class ...Args>
constexpr std::tuple<Args&...> tie(Args&... values)
{
    return std::tuple<Args&...>(values...);
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

template<typename T> class TypeHasRefMemberFunction {
    template<typename> static std::false_type test(...);
    template<typename U> static auto test(int) -> decltype(std::declval<U>().ref(), std::true_type());
public:
    static constexpr bool value = std::is_same<decltype(test<T>(0)), std::true_type>::value;
};

template<class T, class... Args>
ALWAYS_INLINE decltype(auto) makeUnique(Args&&... args)
{
    static_assert(std::is_same<typename T::WTFIsFastAllocated, int>::value, "T sould use FastMalloc (WTF_MAKE_FAST_ALLOCATED)");
    static_assert(!TypeHasRefMemberFunction<T>::value, "T should not be RefCounted");
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<class T, class... Args>
ALWAYS_INLINE decltype(auto) makeUniqueWithoutRefCountedCheck(Args&&... args)
{
    static_assert(std::is_same<typename T::WTFIsFastAllocated, int>::value, "T sould use FastMalloc (WTF_MAKE_FAST_ALLOCATED)");
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<class T, class... Args>
ALWAYS_INLINE decltype(auto) makeUniqueWithoutFastMallocCheck(Args&&... args)
{
    static_assert(!TypeHasRefMemberFunction<T>::value, "T should not be RefCounted");
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

template<typename OptionalType, class Callback> typename OptionalType::value_type valueOrCompute(OptionalType optional, Callback callback) 
{
    return optional ? *optional : callback();
}

template<typename OptionalType> auto valueOrDefault(OptionalType&& optionalValue)
{
    return optionalValue ? *std::forward<OptionalType>(optionalValue) : std::remove_reference_t<decltype(*optionalValue)> { };
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
template<typename T, typename U, std::size_t Extent>
std::span<T, Extent == std::dynamic_extent ? std::dynamic_extent : (sizeof(U) * Extent) / sizeof(T)> spanReinterpretCast(std::span<U, Extent> span)
{
    if constexpr (Extent == std::dynamic_extent) {
        if constexpr (sizeof(U) < sizeof(T) || sizeof(U) % sizeof(T))
            RELEASE_ASSERT_WITH_MESSAGE(!(span.size_bytes() % sizeof(T)), "spanReinterpretCast will not change size in bytes from source");
    } else
        static_assert(!((sizeof(U) * Extent) % sizeof(T)), "spanReinterpretCast will not change size in bytes from source");

    static_assert(std::is_const_v<T> || (!std::is_const_v<T> && !std::is_const_v<U>), "spanReinterpretCast will not remove constness from source");

    using ReturnType = std::span<T, Extent == std::dynamic_extent ? std::dynamic_extent : (sizeof(U) * Extent) / sizeof(T)>;
    return ReturnType { reinterpret_cast<T*>(const_cast<std::remove_const_t<U>*>(span.data())), span.size_bytes() / sizeof(T) };
}
#pragma GCC diagnostic pop

template<typename T, std::size_t Extent>
std::span<T, Extent> spanConstCast(std::span<const T, Extent> span)
{
    return std::span<T, Extent> { const_cast<T*>(span.data()), span.size() };
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

template<typename T, std::size_t TExtent, typename U, std::size_t UExtent>
bool equalSpans(std::span<T, TExtent> a, std::span<U, UExtent> b)
{
    static_assert(sizeof(T) == sizeof(U));
    static_assert(std::has_unique_object_representations_v<T>);
    static_assert(std::has_unique_object_representations_v<U>);
    if (a.size() != b.size())
        return false;
    return !memcmp(a.data(), b.data(), a.size_bytes());
}

template<typename T, std::size_t TExtent, typename U, std::size_t UExtent>
void memcpySpan(std::span<T, TExtent> destination, std::span<U, UExtent> source)
{
    static_assert(sizeof(T) == sizeof(U));
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_trivially_copyable_v<U>);
    RELEASE_ASSERT(destination.size() >= source.size());
    memcpy(destination.data(), source.data(), source.size_bytes());
}

template<typename T, std::size_t Extent>
void memsetSpan(std::span<T, Extent> destination, uint8_t byte)
{
    static_assert(std::is_trivially_copyable_v<T>);
    memset(destination.data(), byte, destination.size_bytes());
}

// Preferred helper function for converting an imported C++ API into a span.
template<typename T>
inline auto makeSpan(const T& source)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    return std::span { source.begin(), source.end() };
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

// Less preferred helper function for converting an imported API into a span.
// Use this when we can't edit the imported API and it doesn't offer
// begin() / end() or a span accessor.
template<typename T, std::size_t Extent = std::dynamic_extent>
inline auto unsafeForgeSpan(T* ptr, size_t size)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    return std::span<T, Extent> { ptr, size };
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

template<typename T> concept ByteType = sizeof(T) == 1 && ((std::is_integral_v<T> && !std::same_as<T, bool>) || std::same_as<T, std::byte>) && !std::is_const_v<T>;

template<typename> struct ByteCastTraits;

template<ByteType T> struct ByteCastTraits<T> {
    template<ByteType U> static constexpr U cast(T character) { return static_cast<U>(character); }
};

template<ByteType T> struct ByteCastTraits<T*> {
    template<ByteType U> static constexpr auto cast(T* pointer) { return bitwise_cast<U*>(pointer); }
};

template<ByteType T> struct ByteCastTraits<const T*> {
    template<ByteType U> static constexpr auto cast(const T* pointer) { return bitwise_cast<const U*>(pointer); }
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

template<typename WordType, typename Func>
ALWAYS_INLINE constexpr void forEachSetBit(std::span<const WordType> bits, const Func& func)
{
    constexpr size_t wordSize = sizeof(WordType) * CHAR_BIT;
    for (size_t i = 0; i < bits.size(); ++i) {
        WordType word = bits[i];
        if (!word)
            continue;
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
    }
}

template<typename WordType, typename Func>
ALWAYS_INLINE constexpr void forEachSetBit(std::span<const WordType> bits, size_t startIndex, const Func& func)
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
using WTF::asWritableBytes;
using WTF::binarySearch;
using WTF::bitwise_cast;
using WTF::byteCast;
using WTF::callStatelessLambda;
using WTF::checkAndSet;
using WTF::constructFixedSizeArrayWithArguments;
using WTF::equalSpans;
using WTF::findBitInWord;
using WTF::insertIntoBoundedVector;
using WTF::is8ByteAligned;
using WTF::isCompilationThread;
using WTF::isPointerAligned;
using WTF::isStatelessLambda;
using WTF::makeSpan;
using WTF::makeUnique;
using WTF::makeUniqueWithoutFastMallocCheck;
using WTF::makeUniqueWithoutRefCountedCheck;
using WTF::memcpySpan;
using WTF::memsetSpan;
using WTF::mergeDeduplicatedSorted;
using WTF::roundUpToMultipleOf;
using WTF::roundUpToMultipleOfNonPowerOfTwo;
using WTF::roundDownToMultipleOf;
using WTF::safeCast;
using WTF::spanConstCast;
using WTF::spanReinterpretCast;
using WTF::tryBinarySearch;
using WTF::unsafeForgeSpan;
using WTF::valueOrCompute;
using WTF::valueOrDefault;
using WTF::toTwosComplement;
using WTF::Invocable;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
