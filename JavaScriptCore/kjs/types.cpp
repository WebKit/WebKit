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

#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "internal.h"
#include "collector.h"
#include "operations.h"
#include "error_object.h"
#include "nodes.h"

using namespace KJS;

namespace KJS {
  // ---------------------------------------------------------------------------
  //                            Internal type impls
  // ---------------------------------------------------------------------------

  /**
   * @internal
   */
  class ListNode {
    friend class List;
    friend class ListImp;
    friend class ListIterator;
    ListNode(Value val, ListNode *p, ListNode *n)
      : member(val.imp()), prev(p), next(n) {};
    ValueImp *member;
    ListNode *prev, *next;
  };

  class ListImp : public ValueImp {
    friend class ListIterator;
    friend class List;
    friend class InterpreterImp;
    friend class ObjectImp;
  private:
    ListImp();
    ~ListImp();

    Type type() const { return ListType; }

    virtual void mark();

    Value toPrimitive(ExecState *exec, Type preferred = UnspecifiedType) const;
    bool toBoolean(ExecState *exec) const;
    double toNumber(ExecState *exec) const;
    UString toString(ExecState *exec) const;
    Object toObject(ExecState *exec) const;

    void append(const Value& val);
    void prepend(const Value& val);
    void appendList(const List& lst);
    void prependList(const List& lst);
    void removeFirst();
    void removeLast();
    void remove(const Value &val);
    void clear();
    ListImp *copy() const;
    ListIterator begin() const { return ListIterator(hook->next); }
    ListIterator end() const { return ListIterator(hook); }
    //    bool isEmpty() const { return (hook->prev == hook); }
    bool isEmpty() const;
    int size() const;
    Value at(int i) const;
    Value operator[](int i) const { return at(i); }
    static ListImp* empty();

#ifdef KJS_DEBUG_MEM
    static int count;
#endif
  private:
    void erase(ListNode *n);
    ListNode *hook;
    static ListImp *emptyList;
  };
  
}


// ------------------------------ ListIterator ---------------------------------

//d  dont add   ListIterator();

ListIterator::ListIterator(ListNode *n) : node(n)
{
}

ListIterator::ListIterator(const List &l)
  : node(l.imp->hook->next)
{
}

ValueImp* ListIterator::operator->() const
{
  return node->member;
}

//    operator Value* () const { return node->member; }
Value ListIterator::operator*() const
{
  return Value(node->member);
}

//    operator Value*() const { return node->member; }
Value ListIterator::operator++()
{
  node = node->next;
  return Value(node->member);
}

Value ListIterator::operator++(int)
{
  const ListNode *n = node;
  ++*this;
  return Value(n->member);
}

Value ListIterator::operator--()
{
  node = node->prev;
  return Value(node->member);
}

Value ListIterator::operator--(int)
{
  const ListNode *n = node;
  --*this;
  return Value(n->member);
}

bool ListIterator::operator==(const ListIterator &it) const
{
  return (node==it.node);
}

bool ListIterator::operator!=(const ListIterator &it) const
{
  return (node!=it.node);
}

// ------------------------------ List -----------------------------------------

List::List(bool needsMarking)
  : m_needsMarking(needsMarking)
{
  imp = m_needsMarking ? ListImp::empty() : new ListImp();
    
  if (!m_needsMarking) {
    imp->ref();
  }
}


List::List(const List& l)
  : m_needsMarking(false)
{  
  imp = l.imp;

  if (!m_needsMarking) {
    imp->ref();
  }
}

List::List(ListImp *p_imp) 
  : m_needsMarking(false)
{
  imp = p_imp;

  if (!m_needsMarking) {
    imp->ref();
  }
}


List& List::operator=(const List& l)
{
  if (!m_needsMarking) {
    l.imp->ref();
    imp->deref();
  }

  imp = l.imp;

  return *this;
}
      
List::~List()
{
  if (!m_needsMarking) {
    imp->deref();
  }
}

void List::mark()
{
  if (!imp->marked()) {
    imp->mark();
  }
}

void List::append(const Value& val)
{
  imp->append(val);
}

void List::prepend(const Value& val)
{
  imp->prepend(val);
}

void List::appendList(const List& lst)
{
  imp->appendList(lst);
}

void List::prependList(const List& lst)
{
  imp->prependList(lst);
}

void List::removeFirst()
{
  imp->removeFirst();
}

void List::removeLast()
{
  imp->removeLast();
}

void List::remove(const Value &val)
{
  imp->remove(val);
}

void List::clear()
{
  imp->clear();
}

List List::copy() const
{
  return imp->copy();
}

ListIterator List::begin() const
{
  return imp->begin();
}

ListIterator List::end() const
{
  return imp->end();
}

bool List::isEmpty() const
{
  return imp->isEmpty();
}

int List::size() const
{
  return imp->size();
}

Value List::at(int i) const
{
  return imp->at(i);
}

Value List::operator[](int i) const
{
  return imp->at(i);
}

const List List::empty()
{
  return ListImp::empty();
}

#ifdef KJS_DEBUG_MEM
void List::globalClear()
{
  delete ListImp::emptyList;
  ListImp::emptyList = 0L;
}
#endif

void List::markEmptyList()
{
  if (ListImp::emptyList && !ListImp::emptyList->marked())
    ListImp::emptyList->mark();
}


// ------------------------------ ListImp --------------------------------------

#ifdef KJS_DEBUG_MEM
int ListImp::count = 0;
#endif

Value ListImp::toPrimitive(ExecState */*exec*/, Type /*preferredType*/) const
{
  // invalid for List
  assert(false);
  return Value();
}

bool ListImp::toBoolean(ExecState */*exec*/) const
{
  // invalid for List
  assert(false);
  return false;
}

double ListImp::toNumber(ExecState */*exec*/) const
{
  // invalid for List
  assert(false);
  return 0;
}

UString ListImp::toString(ExecState */*exec*/) const
{
  // invalid for List
  assert(false);
  return UString::null;
}

Object ListImp::toObject(ExecState */*exec*/) const
{
  // invalid for List
  assert(false);
  return Object();
}

ListImp::ListImp()
{
#ifdef KJS_DEBUG_MEM
  count++;
#endif

  hook = new ListNode(Null(), 0L, 0L);
  hook->next = hook;
  hook->prev = hook;
  //fprintf(stderr,"ListImp::ListImp %p hook=%p\n",this,hook);
}

ListImp::~ListImp()
{
  //fprintf(stderr,"ListImp::~ListImp %p\n",this);
#ifdef KJS_DEBUG_MEM
  count--;
#endif

  clear();
  delete hook;

  if ( emptyList == this )
    emptyList = 0L;
}

void ListImp::mark()
{
  ListNode *n = hook->next;
  while (n != hook) {
    if (!n->member->marked())
      n->member->mark();
    n = n->next;
  }
  ValueImp::mark();
}

void ListImp::append(const Value& obj)
{
  ListNode *n = new ListNode(obj, hook->prev, hook);
  hook->prev->next = n;
  hook->prev = n;
}

void ListImp::prepend(const Value& obj)
{
  ListNode *n = new ListNode(obj, hook, hook->next);
  hook->next->prev = n;
  hook->next = n;
}

void ListImp::appendList(const List& lst)
{
  ListIterator it = lst.begin();
  ListIterator e = lst.end();
  while(it != e) {
    append(*it);
    ++it;
  }
}

void ListImp::prependList(const List& lst)
{
  ListIterator it = lst.end();
  ListIterator e = lst.begin();
  while(it != e) {
    --it;
    prepend(*it);
  }
}

void ListImp::removeFirst()
{
  erase(hook->next);
}

void ListImp::removeLast()
{
  erase(hook->prev);
}

void ListImp::remove(const Value &obj)
{
  if (obj.isNull())
    return;
  ListNode *n = hook->next;
  while (n != hook) {
    if (n->member == obj.imp()) {
      erase(n);
      return;
    }
    n = n->next;
  }
}

void ListImp::clear()
{
  ListNode *n = hook->next;
  while (n != hook) {
    n = n->next;
    delete n->prev;
  }

  hook->next = hook;
  hook->prev = hook;
}

ListImp *ListImp::copy() const
{
  ListImp* newList = new ListImp;

  ListIterator e = end();
  ListIterator it = begin();

  while(it != e) {
    newList->append(*it);
    ++it;
  }

  //fprintf( stderr, "ListImp::copy returning newList=%p\n", newList );
  return newList;
}

void ListImp::erase(ListNode *n)
{
  if (n != hook) {
    n->next->prev = n->prev;
    n->prev->next = n->next;
    delete n;
  }
}

bool ListImp::isEmpty() const
{
  return (hook->prev == hook);
}

int ListImp::size() const
{
  int s = 0;
  ListNode *node = hook;
  while ((node = node->next) != hook)
    s++;

  return s;
}

Value ListImp::at(int i) const
{
  if (i < 0 || i >= size())
    return Undefined();

  ListIterator it = begin();
  int j = 0;
  while ((j++ < i))
    it++;

  return *it;
}

ListImp *ListImp::emptyList = 0L;

ListImp *ListImp::empty()
{
  if (!emptyList)
    emptyList = new ListImp();
  return emptyList;
}

