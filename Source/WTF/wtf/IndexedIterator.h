/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef WTF_IndexedIterator_h
#define WTF_IndexedIterator_h

namespace WTF {

#define INDEXED_ITERATOR_OVERFLOW(check) do {\
    if (!(check))\
        OverflowHandler::overflowed();\
} while (0)

template <typename VectorType, typename T, typename OverflowHandler = CrashOnOverflow> struct IndexedIterator {
    typedef struct IndexedIterator<const VectorType, const T, OverflowHandler> const_iterator;

    typedef T ValueType;
    typedef ptrdiff_t difference_type;
    typedef ValueType value_type;
    typedef ValueType* pointer;
    typedef ValueType& reference;
    typedef std::bidirectional_iterator_tag iterator_category;

    IndexedIterator()
        : m_vector(nullptr)
        , m_position(0)
    {
    }

    IndexedIterator(VectorType& vector, ValueType* position)
        : m_vector(&vector)
        , m_position(position - vector.data())
    {
        INDEXED_ITERATOR_OVERFLOW(m_position <= vector.size());
    }

    IndexedIterator(const IndexedIterator& other)
        : m_vector(other.m_vector)
        , m_position(other.m_position)
    {
    }

    ValueType* operator->() const { return &m_vector->at(m_position); }
    ValueType& operator*() const { return m_vector->at(m_position); }
    ValueType* get() const
    {
        INDEXED_ITERATOR_OVERFLOW(m_position <= m_vector->size());
        return m_vector->data() + m_position;
    }

    IndexedIterator& operator++()
    {
        INDEXED_ITERATOR_OVERFLOW(m_position < m_vector->size());
        m_position++;
        return *this;
    }

    IndexedIterator operator++(int)
    {
        INDEXED_ITERATOR_OVERFLOW(m_position < m_vector->size());
        IndexedIterator result(*this);
        m_position++;
        return result;
    }

    IndexedIterator& operator--()
    {
        INDEXED_ITERATOR_OVERFLOW(m_position > 0);
        m_position--;
        return *this;
    }

    IndexedIterator operator--(int)
    {
        INDEXED_ITERATOR_OVERFLOW(m_position > 0);
        IndexedIterator result(*this);
        m_position--;
        return result;
    }

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    typedef size_t (IndexedIterator::*UnspecifiedBoolType);
    operator UnspecifiedBoolType() const { return m_vector ? &IndexedIterator::m_position : nullptr; }


    IndexedIterator& operator+=(long increment)
    {
        if (increment < 0) {
            INDEXED_ITERATOR_OVERFLOW(increment != std::numeric_limits<int32_t>::min());
            return (*this) -= static_cast<size_t>(-increment);
        }
        INDEXED_ITERATOR_OVERFLOW((std::numeric_limits<size_t>::max() - m_position) > static_cast<size_t>(increment));
        m_position += increment;
        INDEXED_ITERATOR_OVERFLOW(m_position <= m_vector->size());
        return *this;
    }

    IndexedIterator& operator-=(long decrement)
    {
        if (decrement < 0) {
            INDEXED_ITERATOR_OVERFLOW(decrement != std::numeric_limits<int32_t>::min());
            return (*this) += static_cast<size_t>(-decrement);
        }

        INDEXED_ITERATOR_OVERFLOW(static_cast<size_t>(decrement) <= m_position);
        m_position -= decrement;
        return *this;
    }

    IndexedIterator& operator+=(size_t increment)
    {
        INDEXED_ITERATOR_OVERFLOW((std::numeric_limits<size_t>::max() - m_position) > increment);
        m_position += increment;
        INDEXED_ITERATOR_OVERFLOW(m_position <= m_vector->size());
        return *this;
    }

    IndexedIterator& operator-=(size_t decrement)
    {
        INDEXED_ITERATOR_OVERFLOW(decrement <= m_position);
        m_position -= decrement;
        return *this;
    }

    IndexedIterator& operator+=(int32_t increment)
    {
        if (increment < 0) {
            INDEXED_ITERATOR_OVERFLOW(increment != std::numeric_limits<int32_t>::min());
            return (*this) -= static_cast<size_t>(-increment);
        }
        INDEXED_ITERATOR_OVERFLOW((std::numeric_limits<size_t>::max() - m_position) > static_cast<size_t>(increment));
        m_position += increment;
        INDEXED_ITERATOR_OVERFLOW(m_position <= m_vector->size());
        return *this;
    }

    IndexedIterator& operator-=(int32_t decrement)
    {
        if (decrement < 0) {
            INDEXED_ITERATOR_OVERFLOW(decrement != std::numeric_limits<int32_t>::min());
            return (*this) += static_cast<size_t>(-decrement);
        }

        INDEXED_ITERATOR_OVERFLOW(static_cast<size_t>(decrement) <= m_position);
        m_position -= decrement;
        return *this;
    }

    IndexedIterator operator+(size_t increment) const
    {
        IndexedIterator result(*this);
        return result += increment;
    }

    IndexedIterator operator-(size_t decrement) const
    {
        IndexedIterator result(*this);
        return result -= decrement;
    }

    IndexedIterator operator+(int32_t increment) const
    {
        IndexedIterator result(*this);
        return result += increment;
    }

    IndexedIterator operator-(int32_t decrement) const
    {
        IndexedIterator result(*this);
        return result -= decrement;
    }

    IndexedIterator operator+(unsigned long long increment) const
    {
        IndexedIterator result(*this);
        INDEXED_ITERATOR_OVERFLOW(increment <= std::numeric_limits<size_t>::max());
        return result += static_cast<size_t>(increment);
    }

    IndexedIterator operator-(unsigned long long decrement) const
    {
        IndexedIterator result(*this);
        INDEXED_ITERATOR_OVERFLOW(decrement <= std::numeric_limits<size_t>::max());
        return result -= static_cast<size_t>(decrement);
    }
    IndexedIterator operator+(long long increment) const
    {
        IndexedIterator result(*this);
        if (increment < 0) {
            INDEXED_ITERATOR_OVERFLOW(increment != std::numeric_limits<long long>::min());
            return result -= static_cast<unsigned long long>(-increment);
        }
        return result += static_cast<unsigned long long>(increment);
    }

    IndexedIterator operator-(long long decrement) const
    {
        IndexedIterator result(*this);
        if (decrement < 0) {
            INDEXED_ITERATOR_OVERFLOW(decrement != std::numeric_limits<long long>::min());
            return result += static_cast<unsigned long long>(-decrement);
        }
        return result -= static_cast<unsigned long long>(decrement);
    }

#if __SIZEOF_SIZE_T__ != __SIZEOF_INT__ || PLATFORM(MAC) || PLATFORM(IOS)
    IndexedIterator operator+(unsigned increment) const
    {
        IndexedIterator result(*this);
        return result += static_cast<size_t>(increment);
    }

    IndexedIterator operator-(unsigned decrement) const
    {
        IndexedIterator result(*this);
        return result -= static_cast<size_t>(decrement);
    }
#endif

    IndexedIterator operator+(long increment) const
    {
        IndexedIterator result(*this);
        if (increment < 0) {
            INDEXED_ITERATOR_OVERFLOW(increment != std::numeric_limits<long>::min());
            return result -= static_cast<size_t>(-increment);
        }
        return result += static_cast<size_t>(increment);
    }

    IndexedIterator operator-(long decrement) const
    {
        IndexedIterator result(*this);
        if (decrement < 0) {
            INDEXED_ITERATOR_OVERFLOW(decrement != std::numeric_limits<long>::min());
            return result += static_cast<size_t>(-decrement);
        }
        return result -= static_cast<size_t>(decrement);
    }

    ptrdiff_t operator-(const const_iterator& right) const
    {
        INDEXED_ITERATOR_OVERFLOW(m_vector == right.m_vector);
        return m_position - right.m_position;
    }

    ptrdiff_t operator-(const ValueType* right) const
    {
        INDEXED_ITERATOR_OVERFLOW(right >= m_vector->data());
        INDEXED_ITERATOR_OVERFLOW(m_position >= static_cast<size_t>(right - m_vector->data()));
        return get() - right;
    }

    void operator=(const IndexedIterator& other)
    {
        m_vector = other.m_vector;
        m_position = other.m_position;
    }

    bool operator==(const const_iterator& other) const
    {
        INDEXED_ITERATOR_OVERFLOW(isSafeToCompare(other));
        return unsafeGet() == other.unsafeGet();
    }

    bool operator!=(const const_iterator& other) const
    {
        INDEXED_ITERATOR_OVERFLOW(isSafeToCompare(other));
        return unsafeGet() != other.unsafeGet();
    }

    bool operator==(const ValueType* other) const
    {
        INDEXED_ITERATOR_OVERFLOW(isSafeToCompare(other));
        return unsafeGet() == other;
    }

    bool operator!=(const ValueType* other) const
    {
        INDEXED_ITERATOR_OVERFLOW(isSafeToCompare(other));
        return unsafeGet() != other;
    }

    bool operator<(const ValueType* other) const
    {
        INDEXED_ITERATOR_OVERFLOW(isSafeToCompare(other));
        return unsafeGet() < other;
    }

    bool operator<=(const ValueType* other) const
    {
        INDEXED_ITERATOR_OVERFLOW(isSafeToCompare(other));
        return unsafeGet() <= other;
    }

    bool operator<(const const_iterator& other) const
    {
        INDEXED_ITERATOR_OVERFLOW(isSafeToCompare(other));
        return unsafeGet() < other.unsafeGet();
    }

    bool operator<=(const const_iterator& other) const
    {
        INDEXED_ITERATOR_OVERFLOW(safeToCompare(other));
        return unsafeGet() <= other.unsafeGet();
    }

    bool operator>(const IndexedIterator& other) const
    {
        INDEXED_ITERATOR_OVERFLOW(isSafeToCompare(other));
        return unsafeGet() > other.unsafeGet();
    }

    bool operator>=(const const_iterator& other) const
    {
        INDEXED_ITERATOR_OVERFLOW(isSafeToCompare(other));
        return unsafeGet() >= other.unsafeGet();
    }

    bool operator>(const ValueType* other) const
    {
        INDEXED_ITERATOR_OVERFLOW(isSafeToCompare(other));
        return unsafeGet() > other;
    }

    bool operator>=(const ValueType* other) const
    {
        INDEXED_ITERATOR_OVERFLOW(isSafeToCompare(other));
        return unsafeGet() >= other;
    }

    operator const_iterator() const
    {
        const_iterator result;
        result.m_vector = m_vector;
        result.m_position = m_position;
        return result;
    }

    VectorType* m_vector;
    size_t m_position;

    bool isSafeToCompare(const const_iterator& other) const
    {
        if (m_vector == other.m_vector)
            return true;
        return !m_vector || !other.m_vector;
    }

    bool isSafeToCompare(const ValueType* value) const
    {
        if (!value)
            return true;
        if (!m_vector)
            return true;
        size_t delta = static_cast<size_t>(value - m_vector->data());
        return delta <= m_vector->size();
    }

    ValueType* unsafeGet() const
    {
        if (!m_vector)
            return nullptr;
        return get();
    }
};

template <typename T, typename ValueType, typename OverflowHandler>
inline typename IndexedIterator<T, ValueType, OverflowHandler>::ValueType* getPtr(IndexedIterator<T, ValueType, OverflowHandler> p)
{
    return p.get();
}

template <typename T, typename ValueType, typename OverflowHandler>
inline ptrdiff_t operator-(const ValueType* left, const IndexedIterator<T, ValueType, OverflowHandler>& right)
{
    INDEXED_ITERATOR_OVERFLOW(left >= right.m_vector->data());
    INDEXED_ITERATOR_OVERFLOW(left <= right.m_vector->end().get());
    return left - right.get();
}

template <typename T, typename ValueType, typename OverflowHandler>
inline ptrdiff_t operator-(ValueType* left, const IndexedIterator<T, ValueType, OverflowHandler>& right)
{
    INDEXED_ITERATOR_OVERFLOW(left >= right.m_vector->data());
    INDEXED_ITERATOR_OVERFLOW(left <= right.m_vector->end().get());
    return left - right.get();
}

template <typename T, typename ValueType, typename OverflowHandler>
inline ptrdiff_t operator-(const ValueType* left, const IndexedIterator<const T, const ValueType, OverflowHandler>& right)
{
    INDEXED_ITERATOR_OVERFLOW(left >= right.m_vector->data());
    INDEXED_ITERATOR_OVERFLOW(left <= right.m_vector->end().get());
    return left - right.get();
}

template <typename T, typename ValueType, typename OverflowHandler>
inline bool operator==(ValueType* left, const IndexedIterator<T, ValueType, OverflowHandler>& right)
{
    return right == left;
}

template <typename T, typename ValueType, typename OverflowHandler>
inline bool operator==(const ValueType* left, const IndexedIterator<T, ValueType, OverflowHandler>& right)
{
    return right == left;
}

template <typename T, typename ValueType, typename OverflowHandler>
inline bool operator!=(ValueType* left, const IndexedIterator<T, ValueType, OverflowHandler>& right)
{
    return right != left;
}

template <typename T, typename ValueType, typename OverflowHandler>
inline bool operator!=(const ValueType* left, const IndexedIterator<T, ValueType, OverflowHandler>& right)
{
    return right != left;
}

template <typename T, typename ValueType, typename OverflowHandler>
inline bool operator<=(ValueType* left, const IndexedIterator<T, ValueType, OverflowHandler>& right)
{
    return right >= left;
}

template <typename T, typename ValueType, typename OverflowHandler>
inline bool operator<=(const ValueType* left, const IndexedIterator<T, ValueType, OverflowHandler>& right)
{
    return right >= left;
}

template <typename T, typename ValueType, typename OverflowHandler>
inline bool operator>=(ValueType* left, const IndexedIterator<T, ValueType, OverflowHandler>& right)
{
    return right <= left;
}

template <typename T, typename ValueType, typename OverflowHandler>
inline bool operator>=(const ValueType* left, const IndexedIterator<T, ValueType, OverflowHandler>& right)
{
    return right <= left;
}

template <typename T, typename ValueType, typename OverflowHandler>
inline bool operator<(ValueType* left, const IndexedIterator<T, ValueType, OverflowHandler>& right)
{
    return right > left;
}

template <typename T, typename ValueType, typename OverflowHandler>
inline bool operator<(const ValueType* left, const IndexedIterator<T, ValueType, OverflowHandler>& right)
{
    return right > left;
}

template <typename T, typename ValueType, typename OverflowHandler>
inline bool operator>(ValueType* left, const IndexedIterator<T, ValueType, OverflowHandler>& right)
{
    return right < left;
}

template <typename T, typename ValueType, typename OverflowHandler>
inline bool operator>(const ValueType* left, const IndexedIterator<T, ValueType, OverflowHandler>& right)
{
    return right < left;
}

template <typename VectorType, typename OverflowHandler> struct IndexedIteratorSelector {
    typedef typename VectorType::ValueType ValueType;
    typedef IndexedIterator<VectorType, ValueType, OverflowHandler> iterator;
    typedef typename iterator::const_iterator const_iterator;
    static iterator makeIterator(VectorType& vector, ValueType* p) { return iterator(vector, p); }
    static const_iterator makeConstIterator(const VectorType& vector, const ValueType* p) { return const_iterator(vector, p); }
};

#undef INDEXED_ITERATOR_OVERFLOW

}

#endif
