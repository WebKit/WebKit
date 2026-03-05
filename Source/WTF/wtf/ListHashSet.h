/*
 * Copyright (C) 2005-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2011, Benjamin Poulain <ikipou@gmail.com>
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

#pragma once

#include <wtf/Forward.h>
#include <wtf/GetPtr.h>
#include <wtf/HashSet.h>

#if CHECK_HASHTABLE_ITERATORS
#include <wtf/WeakPtr.h>
#endif

namespace WTF {
template<typename Value, typename HashFunctions> class ListHashSet;
struct ListHashSetLink;
template<typename ValueArg, typename HashArg> class ListHashSetConstIterator;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<typename Value, typename HashFunctions> struct IsDeprecatedWeakRefSmartPointerException<WTF::ListHashSet<Value, HashFunctions>> : std::true_type { };
template<> struct IsDeprecatedWeakRefSmartPointerException<WTF::ListHashSetLink> : std::true_type { };
template<typename ValueArg, typename HashArg> struct IsDeprecatedWeakRefSmartPointerException<WTF::ListHashSetConstIterator<ValueArg, HashArg>> : std::true_type { };
}

namespace WTF {

struct ListHashSetLink
#if CHECK_HASHTABLE_ITERATORS
    : CanMakeWeakPtr<ListHashSetLink, WeakPtrFactoryInitialization::Eager>
#endif
{
    ~ListHashSetLink();
    void unlink();
    void insertAfter(ListHashSetLink*);
    void insertBefore(ListHashSetLink*);

    ListHashSetLink* m_prev { this };
    ListHashSetLink* m_next { this };
};

// ListHashSet: Just like HashSet, this class provides a Set
// interface - a collection of unique objects with O(1) insertion,
// removal and test for containership. However, it also has an
// order - iterating it will always give back values in the order
// in which they are added.

// Unlike iteration of most WTF Hash data structures, iteration is
// guaranteed safe against mutation of the ListHashSet, except for
// removal of the item currently pointed to by a given iterator.

template<typename Value, typename HashFunctions> class ListHashSet;

template<typename ValueArg, typename HashArg> class ListHashSetIterator;
template<typename ValueArg, typename HashArg> class ListHashSetConstIterator;

template<typename ValueArg> struct ListHashSetNode;

template<typename HashArg> struct ListHashSetNodeHashFunctions;
template<typename HashArg> struct ListHashSetTranslator;

template<typename ValueArg, typename HashArg> class ListHashSet final
#if CHECK_HASHTABLE_ITERATORS
    : public CanMakeWeakPtr<ListHashSet<ValueArg, HashArg>, WeakPtrFactoryInitialization::Eager>
#endif
{
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ListHashSet);
private:
    using ValueTraits = HashTraits<ValueArg>;

    typedef ListHashSetLink Link;
    typedef ListHashSetNode<ValueArg> Node;

    struct NodeTraits : public HashTraits<std::unique_ptr<Node>> {
        static constexpr bool hasIsWeakNullValueFunction = ValueTraits::hasIsWeakNullValueFunction;
        static bool isWeakNullValue(const std::unique_ptr<Node>& node) { return isHashTraitsWeakNullValue<ValueTraits>(node->m_value); }
    };

    typedef ListHashSetNodeHashFunctions<HashArg> NodeHash;
    typedef ListHashSetTranslator<HashArg> BaseTranslator;

    typedef HashArg HashFunctions;

public:
    typedef ValueArg ValueType;

    typedef ListHashSetIterator<ValueType, HashArg> iterator;
    typedef ListHashSetConstIterator<ValueType, HashArg> const_iterator;
    friend class ListHashSetConstIterator<ValueType, HashArg>;

    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    typedef HashTableAddResult<iterator> AddResult;

    ListHashSet() = default;
    ListHashSet(std::initializer_list<ValueType>);
    ListHashSet(const ListHashSet&);
    ListHashSet(ListHashSet&&);
    ListHashSet& operator=(const ListHashSet&);
    ListHashSet& operator=(ListHashSet&&);
    ~ListHashSet() = default;

    void swap(ListHashSet&);

    unsigned size() const;
    unsigned capacity() const;
    bool isEmpty() const;

    iterator begin() LIFETIME_BOUND { return makeIterator(head()); }
    iterator end() LIFETIME_BOUND { return makeIterator(&m_sentinel); }
    const_iterator begin() const LIFETIME_BOUND { return makeConstIterator(head()); }
    const_iterator end() const LIFETIME_BOUND { return makeConstIterator(&m_sentinel); }

    iterator random() LIFETIME_BOUND { return makeIterator(m_impl.random()); }
    const_iterator random() const LIFETIME_BOUND { return makeIterator(m_impl.random()); }

    reverse_iterator rbegin() LIFETIME_BOUND { return reverse_iterator(end()); }
    reverse_iterator rend() LIFETIME_BOUND { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const LIFETIME_BOUND { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const LIFETIME_BOUND { return const_reverse_iterator(begin()); }

    ValueType& first() LIFETIME_BOUND;
    const ValueType& first() const LIFETIME_BOUND;
    void removeFirst();
    ValueType takeFirst();

    ValueType& last() LIFETIME_BOUND;
    const ValueType& last() const LIFETIME_BOUND;
    void removeLast();
    ValueType takeLast();

    iterator find(const ValueType&) LIFETIME_BOUND;
    const_iterator find(const ValueType&) const LIFETIME_BOUND;
    bool contains(const ValueType&) const;

    // An alternate version of find() that finds the object by hashing and comparing
    // with some other type, to avoid the cost of type conversion.
    // The HashTranslator interface is defined in HashSet.
    template<typename HashTranslator, typename T> iterator find(const T&) LIFETIME_BOUND;
    template<typename HashTranslator, typename T> const_iterator find(const T&) const LIFETIME_BOUND;
    template<typename HashTranslator, typename T> bool contains(const T&) const;

    // The return value of add is a pair of an iterator to the new value's location, 
    // and a bool that is true if an new entry was added.
    AddResult add(const ValueType&) LIFETIME_BOUND;
    AddResult add(ValueType&&) LIFETIME_BOUND;

    // Add the value to the end of the collection. If the value was already in
    // the list, it is moved to the end.
    AddResult appendOrMoveToLast(const ValueType&) LIFETIME_BOUND;
    AddResult appendOrMoveToLast(ValueType&&) LIFETIME_BOUND;
    bool moveToLastIfPresent(const ValueType&);

    // Add the value to the beginning of the collection. If the value was already in
    // the list, it is moved to the beginning.
    AddResult prependOrMoveToFirst(const ValueType&) LIFETIME_BOUND;
    AddResult prependOrMoveToFirst(ValueType&&) LIFETIME_BOUND;

    AddResult insertBefore(const ValueType& beforeValue, const ValueType& newValue) LIFETIME_BOUND;
    AddResult insertBefore(const ValueType& beforeValue, ValueType&& newValue) LIFETIME_BOUND;
    AddResult insertBefore(iterator, const ValueType&) LIFETIME_BOUND;
    AddResult insertBefore(iterator, ValueType&&) LIFETIME_BOUND;

    bool remove(const ValueType&);
    bool remove(iterator);
    bool removeIf(NOESCAPE const Invocable<bool(ValueType&)> auto&);
    void clear();

    // Useful when the key type is WeakPtr
    size_t computeSize() const requires (ValueTraits::hasIsWeakNullValueFunction);
    bool isEmptyIgnoringNullReferences() const requires (ValueTraits::hasIsWeakNullValueFunction);
    void removeWeakNullEntries() requires (ValueTraits::hasIsWeakNullValueFunction);

    // Overloads for smart pointer values that take the raw pointer type as the parameter.
    template<SmartPtr V = ValueType> iterator find(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>*) LIFETIME_BOUND;
    template<SmartPtr V = ValueType> const_iterator find(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>*) const LIFETIME_BOUND;
    template<SmartPtr V = ValueType> bool contains(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>*) const;
    template<SmartPtr V = ValueType> AddResult insertBefore(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>*, const ValueType&) LIFETIME_BOUND;
    template<SmartPtr V = ValueType> AddResult insertBefore(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>*, ValueType&&) LIFETIME_BOUND;
    template<SmartPtr V = ValueType> bool remove(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>*);

    // Overloads for smart pointer values that take the raw reference type as the parameter.
    template<SmartPtr V = ValueType> iterator find(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>& ref) LIFETIME_BOUND { return find(&ref); }
    template<SmartPtr V = ValueType> iterator find(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>& ref) const LIFETIME_BOUND { return find(&ref); }
    template<SmartPtr V = ValueType> bool contains(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>& ref) const { return contains(&ref); }
    template<SmartPtr V = ValueType> AddResult insertBefore(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>& ref, const ValueType& v) LIFETIME_BOUND { return insertBefore(&ref, v); }
    template<SmartPtr V = ValueType> AddResult insertBefore(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>& ref, ValueType&& v) LIFETIME_BOUND { return insertBefore(&ref, v); }
    template<SmartPtr V = ValueType> bool remove(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>& ref) { return remove(&ref); }

private:
    Link* head() { return m_sentinel.m_next; }
    Link* tail() { return m_sentinel.m_prev; }

    const Link* head() const { return m_sentinel.m_next; }
    const Link* tail() const { return m_sentinel.m_prev; }

    void unlink(Node*);
    void appendNode(Node*);
    void prependNode(Node*);
    void insertNodeBefore(Link* beforeNode, Node* newNode);
    
    iterator makeIterator(Link*);
    const_iterator makeConstIterator(const Link*) const;

    HashTable<std::unique_ptr<Node>, std::unique_ptr<Node>, IdentityExtractor, NodeHash, NodeTraits, NodeTraits> m_impl;

    // This sentinel holds the list and acts as its end() iterator. The list is circular.
    // Empty: [ Sentinel ] => [ Sentinel ]. One item: [ Sentinel ] => [ Item ] => [ Sentinel ].
    Link m_sentinel;
};

template<typename ValueArg> struct ListHashSetNode : public ListHashSetLink {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(ListHashSetNode);

    template<typename T> ListHashSetNode(T&& value)
        : m_value(std::forward<T>(value))
    {
    }

    ValueArg m_value;
};

template<typename HashArg> struct ListHashSetNodeHashFunctions {
    template<typename T> static unsigned hash(const T& key) { return HashArg::hash(key->m_value); }
    template<typename T> static bool equal(const T& a, const T& b) { return HashArg::equal(a->m_value, b->m_value); }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

inline ListHashSetLink::~ListHashSetLink()
{
    unlink();
}

inline void ListHashSetLink::unlink()
{
    m_prev->m_next = m_next;
    m_next->m_prev = m_prev;

    m_prev = this;
    m_next = this;
}

inline void ListHashSetLink::insertAfter(ListHashSetLink* prev)
{
    ASSERT(m_prev = this);
    ASSERT(m_next = this);

    m_prev = prev;
    m_next = prev->m_next;

    m_prev->m_next = this;
    m_next->m_prev = this;
}

inline void ListHashSetLink::insertBefore(ListHashSetLink* next)
{
    ASSERT(m_prev = this);
    ASSERT(m_next = this);

    m_prev = next->m_prev;
    m_next = next;

    m_prev->m_next = this;
    m_next->m_prev = this;
}

template<typename ValueArg, typename HashArg> class ListHashSetIterator {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ListHashSetIterator);
private:
    typedef ListHashSet<ValueArg, HashArg> ListHashSetType;
    typedef ListHashSetIterator<ValueArg, HashArg> iterator;
    typedef ListHashSetConstIterator<ValueArg, HashArg> const_iterator;
    typedef ListHashSetLink Link;
    typedef ListHashSetNode<ValueArg> Node;
    typedef ValueArg ValueType;

    friend class ListHashSet<ValueArg, HashArg>;

    ListHashSetIterator(const ListHashSetType* set, Link* position, Link* sentinel)
        : m_iterator(set, position, sentinel)
    {
    }

public:
    typedef ptrdiff_t difference_type;
    typedef ValueType value_type;
    typedef ValueType* pointer;
    typedef ValueType& reference;
    typedef std::bidirectional_iterator_tag iterator_category;

    ListHashSetIterator() = default;

    // default copy, assignment and destructor are OK

    ValueType* get() const { return const_cast<ValueType*>(m_iterator.get()); }
    ValueType& operator*() const { return *get(); }
    ValueType* operator->() const { return get(); }

    iterator& operator++() { ++m_iterator; return *this; }
    iterator operator++(int)
    {
        iterator result = *this;
        ++(*this);
        return result;
    }

    iterator& operator--() { --m_iterator; return *this; }
    iterator operator--(int)
    {
        iterator result = *this;
        --(*this);
        return result;
    }

    // Comparison.
    friend bool operator==(const iterator&, const iterator&) = default;

    operator const_iterator() const { return m_iterator; }

private:
    Node* node() { return const_cast<Node*>(m_iterator.node()); }
    Link* link() { return const_cast<Link*>(m_iterator.link()); }

    const_iterator m_iterator;
};

template<typename ValueArg, typename HashArg> class ListHashSetConstIterator {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ListHashSetConstIterator);
private:
    typedef ListHashSet<ValueArg, HashArg> ListHashSetType;
    typedef ListHashSetIterator<ValueArg, HashArg> iterator;
    typedef ListHashSetConstIterator<ValueArg, HashArg> const_iterator;
    typedef ListHashSetLink Link;
    typedef ListHashSetNode<ValueArg> Node;
    typedef ValueArg ValueType;
    using ValueTraits = HashTraits<ValueType>;

    friend class ListHashSet<ValueArg, HashArg>;
    friend class ListHashSetIterator<ValueArg, HashArg>;

    ListHashSetConstIterator(const ListHashSetType* set, const Link* position, const Link* sentinel)
        : m_position(position)
        , m_sentinel(sentinel)
#if CHECK_HASHTABLE_ITERATORS
        , m_weakSet(set, EnableWeakPtrThreadingAssertions::No)
        , m_weakPosition(position, EnableWeakPtrThreadingAssertions::No)
#endif
    {
        UNUSED_PARAM(set);
        skipEmptyBuckets();
    }

public:
    typedef ptrdiff_t difference_type;
    typedef const ValueType value_type;
    typedef const ValueType* pointer;
    typedef const ValueType& reference;
    typedef std::bidirectional_iterator_tag iterator_category;

    ListHashSetConstIterator()
    {
    }

    const ValueType* get() const
    {
#if CHECK_HASHTABLE_ITERATORS
        ASSERT(m_weakPosition);
#endif
        return std::addressof(node()->m_value);
    }

    const ValueType& operator*() const { return *get(); }
    const ValueType* operator->() const { return get(); }

    const_iterator& operator++()
    {
#if CHECK_HASHTABLE_ITERATORS
        ASSERT(m_weakSet);
        ASSERT(m_weakPosition);
#endif
        m_position = m_position->m_next;
#if CHECK_HASHTABLE_ITERATORS
        m_weakPosition = m_position;
#endif
        skipEmptyBuckets();
        return *this;
    }

    const_iterator operator++(int)
    {
        const_iterator result = *this;
        ++(*this);
        return result;
    }

    const_iterator& operator--()
    {
#if CHECK_HASHTABLE_ITERATORS
        ASSERT(m_weakSet);
        ASSERT(m_weakPosition);
#endif
        m_position = m_position->m_prev;
#if CHECK_HASHTABLE_ITERATORS
        m_weakPosition = m_position;
#endif
        skipEmptyBucketsBackwards();
        return *this;
    }

    const_iterator operator--(int)
    {
        const_iterator result = *this;
        --(*this);
        return result;
    }

    // Comparison.
    bool operator==(const const_iterator& other) const
    {
        return m_position == other.m_position;
    }

private:
    void skipEmptyBuckets()
    {
        while (m_position != m_sentinel && isHashTraitsWeakNullValue<ValueTraits>(node()->m_value)) {
            m_position = m_position->m_next;
#if CHECK_HASHTABLE_ITERATORS
            m_weakPosition = m_position;
#endif
        }
    }

    void skipEmptyBucketsBackwards()
    {
        while (m_position != m_sentinel && isHashTraitsWeakNullValue<ValueTraits>(node()->m_value)) {
            m_position = m_position->m_prev;
#if CHECK_HASHTABLE_ITERATORS
            m_weakPosition = m_position;
#endif
        }
    }

    const Node* node() const
    {
#if CHECK_HASHTABLE_ITERATORS
        ASSERT(m_weakSet);
        ASSERT(m_weakPosition);
        ASSERT(m_position != m_sentinel);
#endif
        return static_cast<const Node*>(m_position);
    }

    const Link* link() const
    {
#if CHECK_HASHTABLE_ITERATORS
        ASSERT(m_weakSet);
        ASSERT(m_weakPosition);
#endif
        return m_position;
    }

    const Link* m_position { nullptr };
    const Link* m_sentinel { nullptr };
#if CHECK_HASHTABLE_ITERATORS
    WeakPtr<const ListHashSetType> m_weakSet;
    WeakPtr<Link> m_weakPosition;
#endif
};

template<typename HashFunctions>
struct ListHashSetTranslator {
    template<typename T> static unsigned hash(const T& key) { return HashFunctions::hash(key); }
    template<typename T, typename U> static bool equal(const T& a, const U& b) { return HashFunctions::equal(a->m_value, b); }
    template<typename T, typename U, typename V> static void translate(std::unique_ptr<T>& location, U&& key, V&&)
    {
        location = makeUnique<T>(std::forward<U>(key));
    }
};

template<typename T, typename U>
inline ListHashSet<T, U>::ListHashSet(std::initializer_list<T> initializerList)
{
    for (const auto& value : initializerList)
        add(value);
}

template<typename T, typename U>
inline ListHashSet<T, U>::ListHashSet(const ListHashSet& other)
    : ListHashSet<T, U>()
{
    for (auto& item : other)
        add(item);
}

template<typename T, typename U>
inline ListHashSet<T, U>& ListHashSet<T, U>::operator=(const ListHashSet& other)
{
    if (&other == this)
        return *this;

    ListHashSet tmp(other);
    swap(tmp);
    return *this;
}

template<typename T, typename U>
inline ListHashSet<T, U>::ListHashSet(ListHashSet&& other)
    : m_impl(WTF::move(other.m_impl))
{
    Link* otherHead = other.head();
    other.m_sentinel.unlink();

    if (otherHead != &other.m_sentinel)
        m_sentinel.insertBefore(otherHead);
}

template<typename T, typename U>
inline ListHashSet<T, U>& ListHashSet<T, U>::operator=(ListHashSet&& other)
{
    ListHashSet movedSet(WTF::move(other));
    swap(movedSet);
    return *this;
}

template<typename T, typename U>
inline void ListHashSet<T, U>::swap(ListHashSet& other)
{
    m_impl.swap(other.m_impl);

    Link* head = this->head();
    m_sentinel.unlink();

    Link* otherHead = other.head();
    other.m_sentinel.unlink();

    if (head != &m_sentinel)
        other.m_sentinel.insertBefore(head);

    if (otherHead != &other.m_sentinel)
        m_sentinel.insertBefore(otherHead);
}

template<typename T, typename U>
inline unsigned ListHashSet<T, U>::size() const
{
    return m_impl.size(); 
}

template<typename T, typename U>
inline unsigned ListHashSet<T, U>::capacity() const
{
    return m_impl.capacity(); 
}

template<typename T, typename U>
inline bool ListHashSet<T, U>::isEmpty() const
{
    return m_impl.isEmpty(); 
}

template<typename T, typename U>
inline T& ListHashSet<T, U>::first() LIFETIME_BOUND
{
    return *begin();
}

template<typename T, typename U>
inline const T& ListHashSet<T, U>::first() const LIFETIME_BOUND
{
    return *begin();
}

template<typename T, typename U>
inline void ListHashSet<T, U>::removeFirst()
{
    takeFirst();
}

template<typename T, typename U>
inline T ListHashSet<T, U>::takeFirst()
{
    auto it = m_impl.template find<BaseTranslator, ShouldValidateKey::Yes>(first());

    T result = WTF::move((*it)->m_value);
    m_impl.remove(it);

    return result;
}

template<typename T, typename U>
inline T& ListHashSet<T, U>::last() LIFETIME_BOUND
{
    auto last = --end();
    return *last;
}

template<typename T, typename U>
inline const T& ListHashSet<T, U>::last() const LIFETIME_BOUND
{
    auto last = --end();
    return *last;
}

template<typename T, typename U>
inline void ListHashSet<T, U>::removeLast()
{
    takeLast();
}

template<typename T, typename U>
inline T ListHashSet<T, U>::takeLast()
{
    ASSERT(!isEmpty());
    auto it = m_impl.template find<BaseTranslator, ShouldValidateKey::Yes>(last());

    T result = WTF::move((*it)->m_value);
    m_impl.remove(it);

    return result;
}

template<typename T, typename U>
inline auto ListHashSet<T, U>::find(const ValueType& value) LIFETIME_BOUND -> iterator
{
    auto it = m_impl.template find<BaseTranslator, ShouldValidateKey::Yes>(value);
    if (it == m_impl.end())
        return end();
    return makeIterator(it->get());
}

template<typename T, typename U>
inline auto ListHashSet<T, U>::find(const ValueType& value) const LIFETIME_BOUND -> const_iterator
{
    auto it = m_impl.template find<BaseTranslator, ShouldValidateKey::Yes>(value);
    if (it == m_impl.end())
        return end();
    return makeConstIterator(it->get());
}

template<typename Translator>
struct ListHashSetTranslatorAdapter {
    template<typename T> static unsigned hash(const T& key) { return Translator::hash(key); }
    template<typename T, typename U> static bool equal(const T& a, const U& b) { return Translator::equal(a->m_value, b); }
};

template<typename ValueType, typename U>
template<typename HashTranslator, typename T>
inline auto ListHashSet<ValueType, U>::find(const T& value) LIFETIME_BOUND -> iterator
{
    auto it = m_impl.template find<ListHashSetTranslatorAdapter<HashTranslator>, ShouldValidateKey::Yes>(value);
    if (it == m_impl.end())
        return end();
    return makeIterator(it->get());
}

template<typename ValueType, typename U>
template<typename HashTranslator, typename T>
inline auto ListHashSet<ValueType, U>::find(const T& value) const LIFETIME_BOUND -> const_iterator
{
    auto it = m_impl.template find<ListHashSetTranslatorAdapter<HashTranslator>, ShouldValidateKey::Yes>(value);
    if (it == m_impl.end())
        return end();
    return makeConstIterator(it->get());
}

template<typename ValueType, typename U>
template<typename HashTranslator, typename T>
inline bool ListHashSet<ValueType, U>::contains(const T& value) const
{
    return m_impl.template contains<ListHashSetTranslatorAdapter<HashTranslator>, ShouldValidateKey::Yes>(value);
}

template<typename T, typename U>
inline bool ListHashSet<T, U>::contains(const ValueType& value) const
{
    return m_impl.template contains<BaseTranslator, ShouldValidateKey::Yes>(value);
}

template<typename T, typename U>
auto ListHashSet<T, U>::add(const ValueType& value) LIFETIME_BOUND -> AddResult
{
    auto result = m_impl.template add<BaseTranslator, ShouldValidateKey::Yes>(value, [] { return nullptr; });
    if (result.isNewEntry)
        appendNode(result.iterator->get());
    return AddResult(makeIterator(result.iterator->get()), result.isNewEntry);
}

template<typename T, typename U>
auto ListHashSet<T, U>::add(ValueType&& value) LIFETIME_BOUND -> AddResult
{
    auto result = m_impl.template add<BaseTranslator, ShouldValidateKey::Yes>(WTF::move(value), [] { return nullptr; });
    if (result.isNewEntry)
        appendNode(result.iterator->get());
    return AddResult(makeIterator(result.iterator->get()), result.isNewEntry);
}

template<typename T, typename U>
auto ListHashSet<T, U>::appendOrMoveToLast(const ValueType& value) LIFETIME_BOUND -> AddResult
{
    auto result = m_impl.template add<BaseTranslator, ShouldValidateKey::Yes>(value, [] { return nullptr; });
    Node* node = result.iterator->get();
    if (!result.isNewEntry)
        unlink(node);
    appendNode(node);

    return AddResult(makeIterator(result.iterator->get()), result.isNewEntry);
}

template<typename T, typename U>
auto ListHashSet<T, U>::appendOrMoveToLast(ValueType&& value) LIFETIME_BOUND -> AddResult
{
    auto result = m_impl.template add<BaseTranslator, ShouldValidateKey::Yes>(WTF::move(value), [] { return nullptr; });
    Node* node = result.iterator->get();
    if (!result.isNewEntry)
        unlink(node);
    appendNode(node);

    return AddResult(makeIterator(result.iterator->get()), result.isNewEntry);
}

template<typename T, typename U>
bool ListHashSet<T, U>::moveToLastIfPresent(const ValueType& value)
{
    auto iterator = m_impl.template find<BaseTranslator, ShouldValidateKey::Yes>(value);
    if (iterator == m_impl.end())
        return false;
    Node* node = iterator->get();
    unlink(node);
    appendNode(node);
    return true;
}

template<typename T, typename U>
auto ListHashSet<T, U>::prependOrMoveToFirst(const ValueType& value) LIFETIME_BOUND -> AddResult
{
    auto result = m_impl.template add<BaseTranslator, ShouldValidateKey::Yes>(value, [] { return nullptr; });
    Node* node = result.iterator->get();
    if (!result.isNewEntry)
        unlink(node);
    prependNode(node);

    return AddResult(makeIterator(result.iterator->get()), result.isNewEntry);
}

template<typename T, typename U>
auto ListHashSet<T, U>::prependOrMoveToFirst(ValueType&& value) LIFETIME_BOUND -> AddResult
{
    auto result = m_impl.template add<BaseTranslator, ShouldValidateKey::Yes>(WTF::move(value), [] { return nullptr; });
    Node* node = result.iterator->get();
    if (!result.isNewEntry)
        unlink(node);
    prependNode(node);

    return AddResult(makeIterator(result.iterator->get()), result.isNewEntry);
}

template<typename T, typename U>
auto ListHashSet<T, U>::insertBefore(const ValueType& beforeValue, const ValueType& newValue) LIFETIME_BOUND -> AddResult
{
    return insertBefore(find(beforeValue), newValue);
}

template<typename T, typename U>
auto ListHashSet<T, U>::insertBefore(const ValueType& beforeValue, ValueType&& newValue) LIFETIME_BOUND -> AddResult
{
    return insertBefore(find(beforeValue), WTF::move(newValue));
}

template<typename T, typename U>
auto ListHashSet<T, U>::insertBefore(iterator it, const ValueType& newValue) LIFETIME_BOUND -> AddResult
{
    auto result = m_impl.template add<BaseTranslator, ShouldValidateKey::Yes>(newValue, [] { return nullptr; });
    if (result.isNewEntry)
        insertNodeBefore(it.link(), result.iterator->get());
    return AddResult(makeIterator(result.iterator->get()), result.isNewEntry);
}

template<typename T, typename U>
auto ListHashSet<T, U>::insertBefore(iterator it, ValueType&& newValue) LIFETIME_BOUND -> AddResult
{
    auto result = m_impl.template add<BaseTranslator, ShouldValidateKey::Yes>(WTF::move(newValue), [] { return nullptr; });
    if (result.isNewEntry)
        insertNodeBefore(it.link(), result.iterator->get());
    return AddResult(makeIterator(result.iterator->get()), result.isNewEntry);
}

template<typename T, typename U>
inline bool ListHashSet<T, U>::remove(iterator it)
{
    if (it == end())
        return false;
    remove(it->get());
    return true;
}

template<typename T, typename U>
inline bool ListHashSet<T, U>::removeIf(NOESCAPE const Invocable<bool(ValueType&)> auto& functor)
{
    return m_impl.removeIf([&](std::unique_ptr<Node>& node) {
        return functor(node->m_value);
    });
}

template<typename T, typename U>
inline bool ListHashSet<T, U>::remove(const ValueType& value)
{
    auto it = m_impl.template find<BaseTranslator, ShouldValidateKey::Yes>(value);
    if (it == m_impl.end())
        return false;
    m_impl.remove(it);
    return true;
}

template<typename T, typename U>
inline size_t ListHashSet<T, U>::computeSize() const requires (ValueTraits::hasIsWeakNullValueFunction)
{
    return m_impl.computeSize();
}

template<typename T, typename U>
inline bool ListHashSet<T, U>::isEmptyIgnoringNullReferences() const requires (ValueTraits::hasIsWeakNullValueFunction)
{
    return m_impl.isEmptyIgnoringNullReferences();
}

template<typename T, typename U>
inline void ListHashSet<T, U>::removeWeakNullEntries() requires (ValueTraits::hasIsWeakNullValueFunction)
{
    m_impl.removeWeakNullEntries();
}

template<typename T, typename U>
inline void ListHashSet<T, U>::clear()
{
    m_impl.clear(); 
}

template<typename T, typename U>
template<SmartPtr V>
inline auto ListHashSet<T, U>::find(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>* value) LIFETIME_BOUND -> iterator
{
    auto it = m_impl.template find<BaseTranslator, ShouldValidateKey::Yes>(value);
    if (it == m_impl.end())
        return end();
    return makeIterator(it->get());
}

template<typename T, typename U>
template<SmartPtr V>
inline auto ListHashSet<T, U>::find(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>* value) const LIFETIME_BOUND -> const_iterator
{
    auto it = m_impl.template find<BaseTranslator, ShouldValidateKey::Yes>(value);
    if (it == m_impl.end())
        return end();
    return makeConstIterator(it->get());
}

template<typename T, typename U>
template<SmartPtr V>
inline auto ListHashSet<T, U>::contains(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>* value) const -> bool
{
    return m_impl.template contains<BaseTranslator, ShouldValidateKey::Yes>(value);
}

template<typename T, typename U>
template<SmartPtr V>
inline auto ListHashSet<T, U>::insertBefore(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>* beforeValue, const ValueType& newValue) LIFETIME_BOUND -> AddResult
{
    return insertBefore(find(beforeValue), newValue);
}

template<typename T, typename U>
template<SmartPtr V>
inline auto ListHashSet<T, U>::insertBefore(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>* beforeValue, ValueType&& newValue) LIFETIME_BOUND -> AddResult
{
    return insertBefore(find(beforeValue), WTF::move(newValue));
}

template<typename T, typename U>
template<SmartPtr V>
inline auto ListHashSet<T, U>::remove(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>* value) -> bool
{
    auto it = m_impl.template find<BaseTranslator, ShouldValidateKey::Yes>(value);
    if (it == m_impl.end())
        return false;
    m_impl.remove(it);
    return true;
}

template<typename T, typename U>
void ListHashSet<T, U>::unlink(Node* node)
{
    ASSERT(node != &m_sentinel);
    node->unlink();
}

template<typename T, typename U>
void ListHashSet<T, U>::appendNode(Node* node)
{
    ASSERT(node != &m_sentinel);
    node->insertBefore(&m_sentinel);
}

template<typename T, typename U>
void ListHashSet<T, U>::prependNode(Node* node)
{
    ASSERT(node != &m_sentinel);
    node->insertAfter(&m_sentinel);
}

template<typename T, typename U>
void ListHashSet<T, U>::insertNodeBefore(Link* beforeNode, Node* newNode)
{
    ASSERT(newNode != &m_sentinel);
    newNode->insertBefore(beforeNode);
}

template<typename T, typename U>
inline auto ListHashSet<T, U>::makeIterator(Link* position) -> iterator
{
    return iterator(this, position, &m_sentinel);
}

template<typename T, typename U>
inline auto ListHashSet<T, U>::makeConstIterator(const Link* position) const -> const_iterator
{ 
    return const_iterator(this, position, &m_sentinel);
}

} // namespace WTF

using WTF::ListHashSet;
