/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#ifndef KJS_LIST_H
#define KJS_LIST_H

#include "value.h"

namespace KJS {

  class List;
  class ListIterator;
  class ListNode;
  class ListHookNode;

  /**
   * @short Iterator for @ref KJS::List objects.
   */
  class ListIterator {
    friend class List;
    ListIterator() : node(0) { }
    ListIterator(ListNode *n) : node(n) { }
  public:
    /**
     * Construct an iterator that points to the first element of the list.
     * @param l The list the iterator will operate on.
     */
    ListIterator(const List &l);
    /**
     * Dereference the iterator.
     * @return A pointer to the element the iterator operates on.
     */
    ValueImp* operator->() const;
    Value operator*() const;
    /**
     * Postfix increment operator.
     * @return The element after the increment.
     */
    Value operator++();
    /**
     * Prefix increment operator.
     */
    Value operator++(int);
    /**
     * Postfix decrement operator.
     */
    Value operator--();
    /**
     * Prefix decrement operator.
     */
    Value operator--(int);
    /**
     * Compare the iterator with another one.
     * @return True if the two iterators operate on the same list element.
     * False otherwise.
     */
    bool operator==(const ListIterator &it) const { return node == it.node; }
    /**
     * Check for inequality with another iterator.
     * @return True if the two iterators operate on different list elements.
     */
    bool operator!=(const ListIterator &it) const { return node != it.node; }
  private:
    ListNode *node;
  };

  /**
   * @short Native list type.
   *
   * List is a native ECMAScript type. List values are only used for
   * intermediate results of expression evaluation and cannot be stored
   * as properties of objects.
   *
   * The list is explicitly shared. Note that while copy() returns a
   * copy of the list the referenced objects are still shared.
   */
  class List {
    friend class ListIterator;
  public:
    List();
    List(const List& l);
    List &operator=(const List& l);
      
    ~List();

    List copyTail() const { List result = copy(); result.removeFirst(); return result; }

    /**
     * Append an object to the end of the list.
     *
     * @param val Pointer to object.
     */
    void append(const Value& val);
    void append(ValueImp *val);
    /**
     * Remove all elements from the list.
     */
    void clear();

    /**
     * @return A @ref KJS::ListIterator pointing to the first element.
     */
    ListIterator begin() const;
    /**
     * @return A @ref KJS::ListIterator pointing to the last element.
     */
    ListIterator end() const;
    /**
     * @return true if the list is empty. false otherwise.
     */
    bool isEmpty() const;
    /**
     * @return the current size of the list.
     */
    int size() const;
    /**
     * Retrieve an element at an indexed position. If you want to iterate
     * trough the whole list using @ref KJS::ListIterator will be faster.
     *
     * @param i List index.
     * @return Return the element at position i. @ref KJS::Undefined if the
     * index is out of range.
     */
    Value at(int i) const;
    /**
     * Equivalent to @ref at.
     */
    Value operator[](int i) const;

    /**
     * Returns a pointer to a static instance of an empty list. Useful if a
     * function has a @ref KJS::List parameter.
     */
    static const List &empty();
    
    void replaceFirst(ValueImp *);
    void replaceLast(ValueImp *);
    
  private:

    void removeFirst();
    List copy() const;
    void prepend(ValueImp *val);
    void prependList(const List& lst);
    void erase(ListNode *n);
    void clearInternal();
    void refAll();
    void derefAll();
    void swap(List &other);
    
    ListHookNode *hook;
  };
  
}; // namespace KJS

#endif // KJS_LIST_H
