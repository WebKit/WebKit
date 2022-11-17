/*
 * Copyright (C) 2008-2022 Apple Inc. All Rights Reserved.
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

#include <cstring>
#include <memory>
#include <type_traits>
#include <variant>
#include <wtf/Assertions.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Compiler.h>
#include <wtf/GetPtr.h>
#include <wtf/TypeCasts.h>

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

// OBJECT_OFFSETOF: Like the C++ offsetof macro, but you can use it with classes.
// The magic number 0x4000 is insignificant. We use it to avoid using NULL, since
// NULL can cause compiler problems, especially in cases of multiple inheritance.
#define OBJECT_OFFSETOF(class, field) (reinterpret_cast<ptrdiff_t>(&(reinterpret_cast<class*>(0x4000)->field)) - 0x4000)

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
#if (CPU(ARM) || CPU(MIPS) || CPU(RISCV64)) && COMPILER(GCC_COMPATIBLE)
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
inline ToType bitwise_cast(FromType from)
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

// Macro that returns a compile time constant with the length of an array, but gives an error if passed a non-array.
template<typename T, size_t Size> char (&ArrayLengthHelperFunction(T (&)[Size]))[Size];
// GCC needs some help to deduce a 0 length array.
#if COMPILER(GCC_COMPATIBLE)
template<typename T> char (&ArrayLengthHelperFunction(T (&)[0]))[0];
#endif
#define WTF_ARRAY_LENGTH(array) sizeof(::WTF::ArrayLengthHelperFunction(array))

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
bool isStatelessLambda()
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

    constexpr size_t bitsInWord = sizeof(word) * 8;
    ASSERT_UNUSED(bitsInWord, startOrResultIndex <= bitsInWord && endIndex <= bitsInWord);

    size_t index = startOrResultIndex;
    word >>= index;

#if COMPILER(GCC_COMPATIBLE) && (CPU(X86_64) || CPU(ARM64))
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

// Visitor adapted from http://stackoverflow.com/questions/25338795/is-there-a-name-for-this-tuple-creation-idiom

template <class A, class... B>
struct Visitor : Visitor<A>, Visitor<B...> {
    Visitor(A a, B... b)
        : Visitor<A>(a)
        , Visitor<B...>(b...)
    {
    }

    using Visitor<A>::operator ();
    using Visitor<B...>::operator ();
};
  
template <class A>
struct Visitor<A> : A {
    Visitor(A a)
        : A(a)
    {
    }

    using A::operator();
};
 
template <class... F>
Visitor<F...> makeVisitor(F... f)
{
    return Visitor<F...>(f...);
}

template<class V, class... F>
auto switchOn(V&& v, F&&... f) -> decltype(std::visit(makeVisitor(std::forward<F>(f)...), std::forward<V>(v)))
{
    return std::visit(makeVisitor(std::forward<F>(f)...), std::forward<V>(v));
}

namespace Detail {

template<std::size_t, class, class> struct AlternativeIndexHelper;

template<std::size_t index, class T, class U>
struct AlternativeIndexHelper<index, T, std::variant<U>> {
    static constexpr std::size_t count = std::is_same_v<T, U>;
    static constexpr std::size_t value = index;
};

template<std::size_t index, class T, class U, class... Types> struct AlternativeIndexHelper<index, T, std::variant<U, Types...>> {
    static constexpr std::size_t count = std::is_same_v<T, U> + AlternativeIndexHelper<index + 1, T, std::variant<Types...>>::count;
    static constexpr std::size_t value = std::is_same_v<T, U> ? index : AlternativeIndexHelper<index + 1, T, std::variant<Types...>>::value;
};

} // namespace Detail

template<class T, class U> struct alternativeIndex {
    static_assert(Detail::AlternativeIndexHelper<0, T, U>::count == 1, "There needs to be exactly one of the given type in the variant");
    static constexpr std::size_t value = Detail::AlternativeIndexHelper<0, T, U>::value;
};

template <class T, class U> inline constexpr std::size_t alternativeIndexV = alternativeIndex<T, U>::value;

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

template<class T, class... Args>
ALWAYS_INLINE decltype(auto) makeUnique(Args&&... args)
{
    static_assert(std::is_same<typename T::webkitFastMalloced, int>::value, "T is FastMalloced");
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<class T, class... Args>
ALWAYS_INLINE decltype(auto) makeUniqueWithoutFastMallocCheck(Args&&... args)
{
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

// FIXME: Use std::is_sorted instead of this and remove it, once we require C++20.
template<typename Iterator, typename Predicate> constexpr bool isSortedConstExpr(Iterator first, Iterator last, Predicate predicate)
{
    if (first == last)
        return true;
    auto current = first;
    auto previous = current;
    while (++current != last) {
        if (!predicate(*previous, *current))
            return false;
        previous = current;
    }
    return true;
}

// FIXME: Use std::is_sorted instead of this and remove it, once we require C++20.
template<typename Iterator> constexpr bool isSortedConstExpr(Iterator first, Iterator last)
{
    return isSortedConstExpr(first, last, [] (auto& a, auto& b) { return a < b; });
}

// FIXME: Use std::all_of instead of this and remove it, once we require C++20.
template<typename Iterator, typename Predicate> constexpr bool allOfConstExpr(Iterator first, Iterator last, Predicate predicate)
{
    for (; first != last; ++first) {
        if (!predicate(*first))
            return false;
    }
    return true;
}

template<typename OptionalType, class Callback> typename OptionalType::value_type valueOrCompute(OptionalType optional, Callback callback) 
{
    return optional ? *optional : callback();
}

template<typename OptionalType> auto valueOrDefault(OptionalType&& optionalValue)
{
    return optionalValue ? *std::forward<OptionalType>(optionalValue) : std::remove_reference_t<decltype(*optionalValue)> { };
}

} // namespace WTF

#define WTFMove(value) std::move<WTF::CheckMoveParameter>(value)

// FIXME: Needed for GCC<=9.3. Remove it after Ubuntu 20.04 end of support (May 2023).
#if defined(__GLIBCXX__) && !defined(HAVE_STD_REMOVE_CVREF) && !COMPILER(CLANG)
namespace std {
template <typename T>
struct remove_cvref {
    using type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
};

template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;
}
#endif

using WTF::GB;
using WTF::KB;
using WTF::MB;
using WTF::approximateBinarySearch;
using WTF::binarySearch;
using WTF::bitwise_cast;
using WTF::callStatelessLambda;
using WTF::checkAndSet;
using WTF::constructFixedSizeArrayWithArguments;
using WTF::findBitInWord;
using WTF::insertIntoBoundedVector;
using WTF::is8ByteAligned;
using WTF::isCompilationThread;
using WTF::isPointerAligned;
using WTF::isStatelessLambda;
using WTF::makeUnique;
using WTF::makeUniqueWithoutFastMallocCheck;
using WTF::mergeDeduplicatedSorted;
using WTF::roundUpToMultipleOf;
using WTF::roundDownToMultipleOf;
using WTF::safeCast;
using WTF::tryBinarySearch;
using WTF::valueOrCompute;
using WTF::valueOrDefault;
