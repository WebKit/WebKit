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

// ------------------------------ Reference ------------------------------------

Reference::Reference(const Object& b, const UString& p)
  : Value(new ReferenceImp(b,p))
{
}

Reference::Reference(const Null& b, const UString& p)
  : Value(new ReferenceImp(b,p))
{
}

Reference::Reference(ReferenceImp *v) : Value(v)
{
}

Reference::Reference(const Reference &v) : Value(v)
{
}

Reference::~Reference()
{
}

Reference& Reference::operator=(const Reference &v)
{
  Value::operator=(v);
  return *this;
}

Reference Reference::dynamicCast(const Value &v)
{
  if (v.isNull() || v.type() != ReferenceType)
    return 0;

  return static_cast<ReferenceImp*>(v.imp());
}

// ------------------------------ ListIterator ---------------------------------

//d  dont add   ListIterator();

ListIterator::ListIterator(ListNode *n) : node(n)
{
}

ListIterator::ListIterator(const List &l)
  : node(static_cast<ListImp*>(l.imp())->hook->next)
{
}

ListIterator& ListIterator::operator=(const ListIterator &iterator)
{
  node=iterator.node;
  return *this;
}

ListIterator::ListIterator(const ListIterator &i) : node(i.node)
{
}

ListIterator::~ListIterator()
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

List::List(ListImp *v) : Value(v)
{
  //fprintf(stderr,"List::List(imp) this=%p imp=%p refcount=%d\n",this,v,v?v->refcount:0);
}

List::List(const List &v) : Value(v)
{
  //fprintf(stderr,"List::List(List) this=%p imp=%p refcount=%d\n",this,rep,rep?rep->refcount:0);
}

List::~List()
{
  //fprintf(stderr,"List::~List() this=%p imp=%p refcount=%d\n",this,rep,rep->refcount-1);
}

List& List::operator=(const List &v)
{
  //fprintf(stderr,"List::operator=() this=%p\n",this);
  Value::operator=(v);
  return *this;
}

List List::dynamicCast(const Value &v)
{
  if (v.isNull() || v.type() != ListType)
    return 0;

  return static_cast<ListImp*>(v.imp());
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

// ------------------------------ Completion -----------------------------------

Completion::Completion(ComplType c, const Value& v, const UString &t)
  : Value(new CompletionImp(c,v,t))
{
}

Completion::Completion(CompletionImp *v) : Value(v)
{
}

Completion::Completion(const Completion &v) : Value(v)
{
}

Completion::~Completion()
{
}

Completion& Completion::operator=(const Completion &v)
{
  Value::operator=(v);
  return *this;
}

Completion Completion::dynamicCast(const Value &v)
{
  if (v.isNull() || v.type() != CompletionType)
    return 0;

  return static_cast<CompletionImp*>(v.imp());
}

ComplType Completion::complType() const
{
  return static_cast<CompletionImp*>(rep)->complType();
}

Value Completion::value() const
{
  return static_cast<CompletionImp*>(rep)->value();
}

UString Completion::target() const
{
  return static_cast<CompletionImp*>(rep)->target();
}

bool Completion::isValueCompletion() const
{
  return !value().isNull();
}
