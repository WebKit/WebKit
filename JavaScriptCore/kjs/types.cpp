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

// ------------------------------ ListIterator ---------------------------------

//d  dont add   ListIterator();

ListIterator::ListIterator(ListNode *n) : node(n)
{
}

ListIterator::ListIterator(const List &l)
  : node(static_cast<ListImp*>(l.imp())->hook->next)
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

List::List()
  : Value(new ListImp())
{
  //fprintf(stderr,"List::List() this=%p imp=%p refcount=%d\n",this,rep,rep->refcount);
}

void List::append(const Value& val)
{
  static_cast<ListImp*>(rep)->append(val);
}

void List::prepend(const Value& val)
{
  static_cast<ListImp*>(rep)->prepend(val);
}

void List::appendList(const List& lst)
{
  static_cast<ListImp*>(rep)->appendList(lst);
}

void List::prependList(const List& lst)
{
  static_cast<ListImp*>(rep)->prependList(lst);
}

void List::removeFirst()
{
  static_cast<ListImp*>(rep)->removeFirst();
}

void List::removeLast()
{
  static_cast<ListImp*>(rep)->removeLast();
}

void List::remove(const Value &val)
{
  static_cast<ListImp*>(rep)->remove(val);
}

void List::clear()
{
  static_cast<ListImp*>(rep)->clear();
}

List List::copy() const
{
  return static_cast<ListImp*>(rep)->copy();
}

ListIterator List::begin() const
{
  return static_cast<ListImp*>(rep)->begin();
}

ListIterator List::end() const
{
  return static_cast<ListImp*>(rep)->end();
}

bool List::isEmpty() const
{
  return static_cast<ListImp*>(rep)->isEmpty();
}

int List::size() const
{
  return static_cast<ListImp*>(rep)->size();
}

Value List::at(int i) const
{
  return static_cast<ListImp*>(rep)->at(i);
}

Value List::operator[](int i) const
{
  return static_cast<ListImp*>(rep)->at(i);
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

