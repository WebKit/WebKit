/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <type_traits>
#include <wtf/Assertions.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

namespace WTF {

struct CrossThreadCopierBaseHelper {
    template<typename T> struct RemovePointer {
        typedef T Type;
    };
    template<typename T> struct RemovePointer<T*> {
        typedef T Type;
    };

    template<typename T> struct RemovePointer<RefPtr<T>> {
        typedef T Type;
    };

    template<typename T> struct IsEnumOrConvertibleToInteger {
        static constexpr bool value = std::is_integral<T>::value || std::is_enum<T>::value || std::is_convertible<T, long double>::value;
    };

    template<typename T> struct IsThreadSafeRefCountedPointer {
        static constexpr bool value = std::is_convertible<typename RemovePointer<T>::Type*, ThreadSafeRefCounted<typename RemovePointer<T>::Type>*>::value;
    };
};

template<typename T> struct CrossThreadCopierPassThrough {
    typedef T Type;
    static Type copy(const T& parameter)
    {
        return parameter;
    }
};

template<bool isEnumOrConvertibleToInteger, bool isThreadSafeRefCounted, typename T> struct CrossThreadCopierBase;

// Integers get passed through without any changes.
template<typename T> struct CrossThreadCopierBase<true, false, T> : public CrossThreadCopierPassThrough<T> {
};

// Classes that have an isolatedCopy() method get a default specialization.
template<class T> struct CrossThreadCopierBase<false, false, T> {
    template<typename U> static auto copy(U&& value)
    {
        return std::forward<U>(value).isolatedCopy();
    }
};

// Custom copy methods.
template<typename T> struct CrossThreadCopierBase<false, true, T> {
    typedef typename CrossThreadCopierBaseHelper::RemovePointer<T>::Type RefCountedType;
    static_assert(std::is_convertible<RefCountedType*, ThreadSafeRefCounted<RefCountedType>*>::value, "T is not convertible to ThreadSafeRefCounted!");

    typedef RefPtr<RefCountedType> Type;
    static Type copy(const T& refPtr)
    {
        return refPtr;
    }
};

template<> struct CrossThreadCopierBase<false, false, WTF::ASCIILiteral> {
    typedef WTF::ASCIILiteral Type;
    static Type copy(const Type& source)
    {
        return source;
    }
};

template<typename T>
struct CrossThreadCopier : public CrossThreadCopierBase<CrossThreadCopierBaseHelper::IsEnumOrConvertibleToInteger<T>::value, CrossThreadCopierBaseHelper::IsThreadSafeRefCountedPointer<T>::value, T> {
};

// Default specialization for Vectors of CrossThreadCopyable classes.
template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct CrossThreadCopierBase<false, false, Vector<T, inlineCapacity, OverflowHandler, minCapacity>> {
    using Type = Vector<T, inlineCapacity, OverflowHandler, minCapacity>;
    static Type copy(const Type& source)
    {
        Type destination;
        destination.reserveInitialCapacity(source.size());
        for (auto& object : source)
            destination.uncheckedAppend(CrossThreadCopier<T>::copy(object));
        return destination;
    }
};
    
// Default specialization for HashSets of CrossThreadCopyable classes
template<typename T> struct CrossThreadCopierBase<false, false, HashSet<T> > {
    typedef HashSet<T> Type;
    static Type copy(const Type& source)
    {
        Type destination;
        for (auto& object : source)
            destination.add(CrossThreadCopier<T>::copy(object));
        return destination;
    }
};

// Default specialization for HashMaps of CrossThreadCopyable classes
template<typename K, typename V> struct CrossThreadCopierBase<false, false, HashMap<K, V> > {
    typedef HashMap<K, V> Type;
    static Type copy(const Type& source)
    {
        Type destination;
        for (auto& keyValue : source)
            destination.add(CrossThreadCopier<K>::copy(keyValue.key), CrossThreadCopier<V>::copy(keyValue.value));
        return destination;
    }
};

// Default specialization for pairs of CrossThreadCopyable classes
template<typename F, typename S> struct CrossThreadCopierBase<false, false, std::pair<F, S> > {
    typedef std::pair<F, S> Type;
    static Type copy(const Type& source)
    {
        return std::make_pair(CrossThreadCopier<F>::copy(source.first), CrossThreadCopier<S>::copy(source.second));
    }
};

// Default specialization for Optional of CrossThreadCopyable class.
template<typename T> struct CrossThreadCopierBase<false, false, Optional<T>> {
    template<typename U> static Optional<T> copy(U&& source)
    {
        if (!source)
            return WTF::nullopt;
        return CrossThreadCopier<T>::copy(std::forward<U>(source).value());
    }
};

template<typename T> auto crossThreadCopy(T&& source)
{
    return CrossThreadCopier<std::remove_cv_t<std::remove_reference_t<T>>>::copy(std::forward<T>(source));
}
    
} // namespace WTF

using WTF::CrossThreadCopierBaseHelper;
using WTF::CrossThreadCopierBase;
using WTF::CrossThreadCopier;
using WTF::crossThreadCopy;
