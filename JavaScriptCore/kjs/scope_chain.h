// -*- c-basic-offset: 2 -*-
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

#ifndef KJS_SCOPE_CHAIN_H
#define KJS_SCOPE_CHAIN_H

#include "value.h"

// internal data types

namespace KJS {

  class ScopeChain;
  class ScopeChainIterator;
  class ScopeChainNode;
  class ScopeChainHookNode;

  /**
   * @short Iterator for @ref KJS::ScopeChain objects.
   */
  class ScopeChainIterator {
    friend class ScopeChain;
    ScopeChainIterator() : node(0) { }
    ScopeChainIterator(ScopeChainNode *n) : node(n) { }
  public:
    /**
     * Construct an iterator that points to the first element of the list.
     * @param l The list the iterator will operate on.
     */
    ScopeChainIterator(const ScopeChain &l);
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
     * Compare the iterator with another one.
     * @return True if the two iterators operate on the same list element.
     * False otherwise.
     */
    bool operator==(const ScopeChainIterator &it) const { return node == it.node; }
    /**
     * Check for inequality with another iterator.
     * @return True if the two iterators operate on different list elements.
     */
    bool operator!=(const ScopeChainIterator &it) const { return node != it.node; }
  private:
    ScopeChainNode *node;
  };

  /**
   * @short Native list type.
   *
   * ScopeChain is a native ECMAScript type. ScopeChain values are only used for
   * intermediate results of expression evaluation and cannot be stored
   * as properties of objects.
   *
   * The list is explicitly shared. Note that while copy() returns a
   * copy of the list the referenced objects are still shared.
   */
  class ScopeChain {
    friend class ScopeChainIterator;
  public:
    ScopeChain(bool needsMarking = false);
    ScopeChain(const ScopeChain& l);
    ScopeChain &operator=(const ScopeChain& l);

    ~ScopeChain();

    /**
     * Insert an object at the beginning of the list.
     *
     * @param val Pointer to object.
     */
    void prepend(const Value& val);
    /**
     * Remove the element at the beginning of the list.
     */
    void removeFirst();
    /**
     * Returns a shallow copy of the list. Ownership is passed to the user
     * who is responsible for deleting the list then.
     */
    ScopeChain copy() const;
    /**
     * @return A @ref KJS::ScopeChainIterator pointing to the first element.
     */
    ScopeChainIterator begin() const;
    /**
     * @return A @ref KJS::ScopeChainIterator pointing to the last element.
     */
    ScopeChainIterator end() const;

    bool isEmpty() const;
    
    void mark() const;

    // temporary
    void prependList(const ScopeChain &);
    void append(const Value &);

  private:

    void prepend(ValueImp *val);
    void erase(ScopeChainNode *n);
    void clearInternal();
    void refAll() const;
    void derefAll() const;
    void swap(ScopeChain &other);
    
    ScopeChainHookNode *hook;
    bool m_needsMarking;
  };
  
}; // namespace

#endif // KJS_SCOPE_CHAIN_H
