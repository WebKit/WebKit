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

  class ListNode {
    friend class List;
    friend class ListIterator;
  protected:
    ListNode(const Value &val, ListNode *p, ListNode *n)
      : member(val.imp()), prev(p), next(n) {};
    ListNode(ValueImp *val, ListNode *p, ListNode *n)
      : member(val), prev(p), next(n) {};
    ValueImp *member;
    ListNode *prev, *next;
  };

  class ListHookNode : public ListNode {
    friend class List;
    
    ListHookNode() : ListNode(Value(), NULL, NULL), refcount(1) { prev = this; next = this; }
    int refcount;
  };
  }


// ------------------------------ ListIterator ---------------------------------

ValueImp* ListIterator::operator->() const
{
  return node->member;
}

Value ListIterator::operator*() const
{
  return Value(node->member);
}

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

// ------------------------------ List -----------------------------------------

List::List(bool needsMarking)
  : hook(new ListHookNode()),
    m_needsMarking(needsMarking)
{
  if (!m_needsMarking) {
    refAll();
  }
}


List::List(const List& l)
  : hook(l.hook),
    m_needsMarking(false)
{  
  hook->refcount++;
  if (!m_needsMarking) {
    refAll();
  }
}

List& List::operator=(const List& l)
{
  List tmp(l);

  tmp.swap(*this);

  return *this;
}
      
List::~List()
{
  if (!m_needsMarking) {
    derefAll();
  }
  
  hook->refcount--;
  if (hook->refcount == 0) {
    clearInternal();
    delete hook;
  }
}

void List::mark()
{
  ListNode *n = hook->next;
  while (n != hook) {
    if (!n->member->marked())
      n->member->mark();
    n = n->next;
  }
}

void List::append(const Value& val)
{
  ListNode *n = new ListNode(val, hook->prev, hook);
  if (!m_needsMarking) {
    n->member->ref();
  }
  hook->prev->next = n;
  hook->prev = n;
}

void List::append(ValueImp *val)
{
  ListNode *n = new ListNode(val, hook->prev, hook);
  if (!m_needsMarking) {
    val->ref();
  }
  hook->prev->next = n;
  hook->prev = n;
}

void List::prepend(const Value& val)
{
  ListNode *n = new ListNode(val, hook, hook->next);
  if (!m_needsMarking) {
    n->member->ref();
  }
  hook->next->prev = n;
  hook->next = n;
}

void List::prepend(ValueImp *val)
{
  ListNode *n = new ListNode(val, hook->prev, hook);
  if (!m_needsMarking) {
    val->ref();
  }
  hook->next->prev = n;
  hook->next = n;
}

void List::appendList(const List& lst)
{
  ListNode *otherHook = lst.hook;
  ListNode *o = otherHook->next;
  while (o != otherHook) {
    append(o->member);
    o = o->next;
  }
}

void List::prependList(const List& lst)
{
  ListNode *otherHook = lst.hook;
  ListNode *o = otherHook->prev;
  while (o != otherHook) {
    prepend(o->member);
    o = o->prev;
  }
}

void List::removeFirst()
{
  erase(hook->next);
}

void List::removeLast()
{
  erase(hook->prev);
}

void List::remove(const Value &val)
{
  if (val.isNull())
    return;
  ListNode *n = hook->next;
  while (n != hook) {
    if (n->member == val.imp()) {
      erase(n);
      return;
    }
    n = n->next;
  }
}


void List::clear()
{
  if (!m_needsMarking) {
    derefAll();
  }
  clearInternal();
}

void List::clearInternal()
{
  ListNode *n = hook->next;
  while (n != hook) {
    n = n->next;
    delete n->prev;
  }

  hook->next = hook;
  hook->prev = hook;
}

List List::copy() const
{
  List newList;
  newList.appendList(*this);
  return newList;
}

ListIterator List::begin() const
{
  return ListIterator(hook->next);
}

ListIterator List::end() const
{
  return ListIterator(hook);
}

bool List::isEmpty() const
{
  return (hook->prev == hook);
}

int List::size() const
{
  int s = 0;
  ListNode *node = hook;
  while ((node = node->next) != hook)
    s++;

  return s;
}

Value List::at(int i) const
{
  if (i < 0 || i >= size())
    return Undefined();

  ListIterator it = begin();
  int j = 0;
  while ((j++ < i))
    it++;

  return *it;
}

Value List::operator[](int i) const
{
  return at(i);
}

const List &List::empty()
{
  static List l;
  return l;
}


void List::erase(ListNode *n)
{
  if (n != hook) {
    if (!m_needsMarking) {
      n->member->deref();
    }
    n->next->prev = n->prev;
    n->prev->next = n->next;
    delete n;
  }
}

void List::refAll()
{
  ListNode *n = hook->next;

  while (n != hook) {
    n->member->ref();
    n = n->next;
  }
}

void List::derefAll()
{
  ListNode *n = hook->next;

  while (n != hook) {
    n->member->deref();
    n = n->next;
  }
}

void List::swap(List &other)
{
  if (m_needsMarking && !other.m_needsMarking) {
    refAll();
    other.derefAll();
  } else if (!m_needsMarking && other.m_needsMarking) {
    other.refAll();
    derefAll();
  }

  ListHookNode *tmp = hook;
  hook = other.hook;
  other.hook = tmp;
}

#ifdef KJS_DEBUG_MEM
void List::globalClear()
{
}
#endif

