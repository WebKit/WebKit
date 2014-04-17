/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef WTF_StdLibExtras_h
#define WTF_StdLibExtras_h

#include <chrono>
#include <memory>
#include <wtf/Assertions.h>
#include <wtf/CheckedArithmetic.h>

// This was used to declare and define a static local variable (static T;) so that
//  it was leaked so that its destructors were not called at exit. Using this
//  macro also allowed to workaround a compiler bug present in Apple's version of GCC 4.0.1.
//
// Newly written code should use static NeverDestroyed<T> instead.
#ifndef DEPRECATED_DEFINE_STATIC_LOCAL
#if COMPILER(GCC) && defined(__APPLE_CC__) && __GNUC__ == 4 && __GNUC_MINOR__ == 0 && __GNUC_PATCHLEVEL__ == 1
#define DEPRECATED_DEFINE_STATIC_LOCAL(type, name, arguments) \
    static type* name##Ptr = new type arguments; \
    type& name = *name##Ptr
#else
#define DEPRECATED_DEFINE_STATIC_LOCAL(type, name, arguments) \
    static type& name = *new type arguments
#endif
#endif

// Use this macro to declare and define a debug-only global variable that may have a
// non-trivial constructor and destructor. When building with clang, this will suppress
// warnings about global constructors and exit-time destructors.
#ifndef NDEBUG
#if COMPILER(CLANG)
#define DEFINE_DEBUG_ONLY_GLOBAL(type, name, arguments) \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wglobal-constructors\"") \
    _Pragma("clang diagnostic ignored \"-Wexit-time-destructors\"") \
    static type name arguments; \
    _Pragma("clang diagnostic pop")
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

// STRINGIZE: Can convert any value to quoted string, even expandable macros
#define STRINGIZE(exp) #exp
#define STRINGIZE_VALUE_OF(exp) STRINGIZE(exp)

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
#if (CPU(ARM) || CPU(MIPS)) && COMPILER(GCC)
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

static const size_t KB = 1024;
static const size_t MB = 1024 * 1024;

inline bool isPointerAligned(void* p)
{
    return !((intptr_t)(p) & (sizeof(char*) - 1));
}

inline bool is8ByteAligned(void* p)
{
    return !((uintptr_t)(p) & (sizeof(double) - 1));
}

/*
 * C++'s idea of a reinterpret_cast lacks sufficient cojones.
 */
template<typename ToType, typename FromType>
inline ToType bitwise_cast(FromType from)
{
    static_assert(sizeof(FromType) == sizeof(ToType), "bitwise_cast size of FromType and ToType must be equal!");
    union {
        FromType from;
        ToType to;
    } u;
    u.from = from;
    return u.to;
}

template<typename ToType, typename FromType>
inline ToType safeCast(FromType value)
{
    ASSERT(isInBounds<ToType>(value));
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
#if COMPILER(GCC)
template<typename T> char (&ArrayLengthHelperFunction(T (&)[0]))[0];
#endif
#define WTF_ARRAY_LENGTH(array) sizeof(::WTF::ArrayLengthHelperFunction(array))

// Efficient implementation that takes advantage of powers of two.
inline size_t roundUpToMultipleOf(size_t divisor, size_t x)
{
    ASSERT(divisor && !(divisor & (divisor - 1)));
    size_t remainderMask = divisor - 1;
    return (x + remainderMask) & ~remainderMask;
}

template<size_t divisor> inline size_t roundUpToMultipleOf(size_t x)
{
    static_assert(divisor && !(divisor & (divisor - 1)), "divisor must be a power of two!");
    return roundUpToMultipleOf(divisor, x);
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
        KeyType val = extractKey(&array[offset + pos]);
        
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

} // namespace WTF

#if OS(WINCE)
// Windows CE CRT has does not implement bsearch().
inline void* wtf_bsearch(const void* key, const void* base, size_t count, size_t size, int (*compare)(const void *, const void *))
{
    const char* first = static_cast<const char*>(base);

    while (count) {
        size_t pos = (count - 1) >> 1;
        const char* item = first + pos * size;
        int compareResult = compare(item, key);
        if (!compareResult)
            return const_cast<char*>(item);
        if (compareResult < 0) {
            count -= (pos + 1);
            first += (pos + 1) * size;
        } else
            count = pos;
    }

    return 0;
}

#define bsearch(key, base, count, size, compare) wtf_bsearch(key, base, count, size, compare)
#endif

// This version of placement new omits a 0 check.
enum NotNullTag { NotNull };
inline void* operator new(size_t, NotNullTag, void* location)
{
    ASSERT(location);
    return location;
}

#if (COMPILER(GCC) && !COMPILER(CLANG) && !GCC_VERSION_AT_LEAST(4, 8, 1))

// Work-around for Pre-C++11 syntax in MSVC 2010, and prior as well as GCC < 4.8.1.
namespace std {
    template<class T> struct is_trivially_destructible {
        static const bool value = std::has_trivial_destructor<T>::value;
    };
}
#endif

// This adds various C++14 features for versions of the STL that may not yet have them.
namespace std {
// MSVC 2013 supports std::make_unique already.
#if !defined(_MSC_VER) || _MSC_VER < 1800
template<class T> struct _Unique_if {
    typedef unique_ptr<T> _Single_object;
};

template<class T> struct _Unique_if<T[]> {
    typedef unique_ptr<T[]> _Unknown_bound;
};

template<class T, size_t N> struct _Unique_if<T[N]> {
    typedef void _Known_bound;
};

template<class T, class... Args> inline typename _Unique_if<T>::_Single_object
make_unique(Args&&... args)
{
    return unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<class T> inline typename _Unique_if<T>::_Unknown_bound
make_unique(size_t n)
{
    typedef typename remove_extent<T>::type U;
    return unique_ptr<T>(new U[n]());
}

template<class T, class... Args> typename _Unique_if<T>::_Known_bound
make_unique(Args&&...) = delete;
#endif

// Compile-time integer sequences
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3658.html
// (Note that we only implement index_sequence, and not the more generic integer_sequence).
template<size_t... indexes> struct index_sequence {
    static size_t size() { return sizeof...(indexes); }
};

template<size_t currentIndex, size_t...indexes> struct make_index_sequence_helper;

template<size_t...indexes> struct make_index_sequence_helper<0, indexes...> {
    typedef std::index_sequence<indexes...> type;
};

template<size_t currentIndex, size_t...indexes> struct make_index_sequence_helper {
    typedef typename make_index_sequence_helper<currentIndex - 1, currentIndex - 1, indexes...>::type type;
};

template<size_t length> struct make_index_sequence : public make_index_sequence_helper<length>::type { };

#if COMPILER_SUPPORTS(CXX_USER_LITERALS)
// These literals are available in C++14, so once we require C++14 compilers we can get rid of them here.
// (User-literals need to have a leading underscore so we add it here - the "real" literals don't have underscores).
namespace literals {
namespace chrono_literals {
    CONSTEXPR inline chrono::seconds operator"" _s(unsigned long long s)
    {
        return chrono::seconds(static_cast<chrono::seconds::rep>(s));
    }

    CONSTEXPR chrono::milliseconds operator"" _ms(unsigned long long ms)
    {
        return chrono::milliseconds(static_cast<chrono::milliseconds::rep>(ms));
    }
}
}
#endif
}

using WTF::KB;
using WTF::MB;
using WTF::isCompilationThread;
using WTF::insertIntoBoundedVector;
using WTF::isPointerAligned;
using WTF::is8ByteAligned;
using WTF::binarySearch;
using WTF::tryBinarySearch;
using WTF::approximateBinarySearch;
using WTF::bitwise_cast;
using WTF::safeCast;

#if COMPILER_SUPPORTS(CXX_USER_LITERALS)
// We normally don't want to bring in entire std namespaces, but literals are an exception.
using namespace std::literals::chrono_literals;
#endif

#endif // WTF_StdLibExtras_h
