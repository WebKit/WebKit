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

namespace KJS {

  struct ListNode {
    ListNode(const Value &val, ListNode *p, ListNode *n)
      : member(val.imp()), prev(p), next(n) { }
    ListNode(ValueImp *val, ListNode *p, ListNode *n)
      : member(val), prev(p), next(n) { }
    ValueImp *member;
    ListNode *prev, *next;
  };

  struct ListHookNode : public ListNode {
    ListHookNode(bool needsMarking) : ListNode(Value(), this, this),
        listRefCount(1), nodesRefCount(needsMarking ? 0 : 1) { }
    int listRefCount;
    int nodesRefCount;
  };

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

List::List(bool needsMarking) : hook(new ListHookNode(needsMarking)), m_needsMarking(needsMarking)
{
}

List::List(const List& l) : hook(l.hook), m_needsMarking(false)
{
  ++hook->listRefCount;
  if (hook->nodesRefCount++ == 0)
    refAll();
}

List& List::operator=(const List& l)
{
  List(l).swap(*this);
  return *this;
}

List::~List()
{
  if (!m_needsMarking)
    if (--hook->nodesRefCount == 0)
      derefAll();
  
  if (--hook->listRefCount == 0) {
    assert(hook->nodesRefCount == 0);
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
  if (hook->nodesRefCount)
    n->member->ref();
  hook->prev->next = n;
  hook->prev = n;
}

void List::append(ValueImp *val)
{
  ListNode *n = new ListNode(val, hook->prev, hook);
  if (hook->nodesRefCount)
    val->ref();
  hook->prev->next = n;
  hook->prev = n;
}

void List::prepend(const Value& val)
{
  ListNode *n = new ListNode(val, hook, hook->next);
  if (hook->nodesRefCount)
    n->member->ref();
  hook->next->prev = n;
  hook->next = n;
}

void List::prepend(ValueImp *val)
{
  ListNode *n = new ListNode(val, hook, hook->next);
  if (hook->nodesRefCount)
    val->ref();
  hook->next->prev = n;
  hook->next = n;
}

void List::prependList(const List& lst)
{
  ListNode *otherHook = lst.hook;
  ListNode *n = otherHook->prev;
  while (n != otherHook) {
    prepend(n->member);
    n = n->prev;
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
  for (ListNode *n = hook->next; n != hook; n = n->next)
    if (n->member == val.imp()) {
      erase(n);
      return;
    }
}

void List::clear()
{
  if (hook->nodesRefCount)
    derefAll();
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
  newList.prependList(*this);
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
  return hook->prev == hook;
}

int List::size() const
{
  int s = 0;
  for (ListNode *n = hook->next; n != hook; n = n->next)
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
    if (hook->nodesRefCount)
      n->member->deref();
    n->next->prev = n->prev;
    n->prev->next = n->next;
    delete n;
  }
}

void List::refAll()
{
  for (ListNode *n = hook->next; n != hook; n = n->next)
    n->member->ref();
}

void List::derefAll()
{
  for (ListNode *n = hook->next; n != hook; n = n->next)
    n->member->deref();
}

void List::swap(List &other)
{
  if (!m_needsMarking)
    if (other.hook->nodesRefCount++ == 0)
      other.refAll();
  if (!other.m_needsMarking)
    if (hook->nodesRefCount++ == 0)
      refAll();

  if (!m_needsMarking)
    if (--hook->nodesRefCount == 0)
      derefAll();
  if (!other.m_needsMarking)
    if (--other.hook->nodesRefCount == 0)
      other.derefAll();

  ListHookNode *tmp = hook;
  hook = other.hook;
  other.hook = tmp;
}

void List::replaceFirst(ValueImp *v)
{
  ListNode *n = hook->next;
  if (hook->nodesRefCount) {
    v->ref();
    n->member->deref();
  }
  n->member = v;
}

void List::replaceLast(ValueImp *v)
{
  ListNode *n = hook->prev;
  if (hook->nodesRefCount) {
    v->ref();
    n->member->deref();
  }
  n->member = v;
}

} // namespace KJS
