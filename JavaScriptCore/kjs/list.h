/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003 Apple Computer, Inc.
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef KJS_LIST_H
#define KJS_LIST_H

#include "value.h"

namespace KJS {

    struct ListImpBase {
        int size;
        int refCount;
	int valueRefCount; // FIXME: Get rid of this.
    };
    
    class ListIterator;

    /**
     * @short Native list type.
     *
     * List is a native ECMAScript type. List values are only used for
     * intermediate results of expression evaluation and cannot be stored
     * as properties of objects.
     *
     * The list is explicitly shared. Note that while copyTail() returns a
     * copy of the list the referenced objects are still shared.
     */
    class List {
    public:
        List();
	List(bool needsMarking);
        ~List() { deref(); }

        List(const List &b) : _impBase(b._impBase), _needsMarking(false) {
	    ++_impBase->refCount; 
	    ++_impBase->valueRefCount; 
	}
        List &operator=(const List &);

        /**
         * Append an object to the end of the list.
         *
         * @param val Pointer to object.
         */
        void append(JSValue *val);
        /**
         * Remove all elements from the list.
         */
        void clear();

        /**
         * Make a copy of the list
         */
        List copy() const;

        /**
         * Copy all elements from the second list here
         */
        void copyFrom(const List& other);

        /**
         * Make a copy of the list, omitting the first element.
         */
        List copyTail() const;
    
        /**
         * @return true if the list is empty. false otherwise.
         */
        bool isEmpty() const { return _impBase->size == 0; }
        /**
         * @return the current size of the list.
         */
        int size() const { return _impBase->size; }
        /**
         * @return A KJS::ListIterator pointing to the first element.
         */
        ListIterator begin() const;
        /**
         * @return A KJS::ListIterator pointing to the last element.
         */
        ListIterator end() const;
        
        /**
         * Retrieve an element at an indexed position. If you want to iterate
         * trough the whole list using KJS::ListIterator will be faster.
         *
         * @param i List index.
         * @return Return the element at position i. KJS::Undefined if the
         * index is out of range.
         */
        JSValue *at(int i) const;
        /**
         * Equivalent to at.
         */
        JSValue *operator[](int i) const { return at(i); }
    
        /**
         * Returns a pointer to a static instance of an empty list. Useful if a
         * function has a KJS::List parameter.
         */
        static const List &empty();
        
	void mark() { if (_impBase->valueRefCount == 0) markValues(); }

        static void markProtectedLists();
    private:
        ListImpBase *_impBase;
	bool _needsMarking;
        
        void deref() { if (!_needsMarking) --_impBase->valueRefCount; if (--_impBase->refCount == 0) release(); }

        void release();
        void markValues();
    };
  
    /**
     * @short Iterator for KJS::List objects.
     */
    class ListIterator {
    public:
        /**
         * Construct an iterator that points to the first element of the list.
         * @param l The list the iterator will operate on.
         */
        ListIterator(const List &l) : _list(&l), _i(0) { }
        ListIterator(const List &l, int index) : _list(&l), _i(index) { }
        /**
         * Dereference the iterator.
         * @return A pointer to the element the iterator operates on.
         */
        JSValue *operator->() const { return _list->at(_i); }
        JSValue *operator*() const { return _list->at(_i); }
        /**
         * Prefix increment operator.
         * @return The element after the increment.
         */
        JSValue *operator++() { return _list->at(++_i); }
        /**
         * Postfix increment operator.
         */
        JSValue *operator++(int) { return _list->at(_i++); }
        /**
         * Prefix decrement operator.
         */
        JSValue *operator--() { return _list->at(--_i); }
        /**
         * Postfix decrement operator.
         */
        JSValue *operator--(int) { return _list->at(_i--); }
        /**
         * Compare the iterator with another one.
         * @return True if the two iterators operate on the same list element.
         * False otherwise.
         */
        bool operator==(const ListIterator &it) const { return _i == it._i; }
        /**
         * Check for inequality with another iterator.
         * @return True if the two iterators operate on different list elements.
         */
        bool operator!=(const ListIterator &it) const { return _i != it._i; }

    private:
        const List *_list;
        int _i;
    };

    inline ListIterator List::begin() const { return ListIterator(*this); }
    inline ListIterator List::end() const { return ListIterator(*this, size()); }
 
} // namespace KJS

#endif // KJS_LIST_H
