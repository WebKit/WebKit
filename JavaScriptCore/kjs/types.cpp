/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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
 */

#include <stdio.h>

#include "kjs.h"
#include "object.h"
#include "types.h"
#include "internal.h"
#include "operations.h"
#include "nodes.h"

using namespace KJS;

Undefined::Undefined() : KJSO(UndefinedImp::staticUndefined) { }

Undefined::~Undefined() { }

Null::Null() : KJSO(NullImp::staticNull) { }

Null::~Null() { }

Boolean::Boolean(bool b)
  : KJSO(b ? BooleanImp::staticTrue : BooleanImp::staticFalse)
{
}

Boolean::Boolean(BooleanImp *i) : KJSO(i) { }

Boolean::~Boolean() { }

bool Boolean::value() const
{
  assert(rep);
  return ((BooleanImp*)rep)->value();
}

Number::Number(int i)
  : KJSO(new NumberImp(static_cast<double>(i))) { }

Number::Number(unsigned int u)
  : KJSO(new NumberImp(static_cast<double>(u))) { }

Number::Number(double d)
  : KJSO(new NumberImp(d)) { }

Number::Number(long int l)
  : KJSO(new NumberImp(static_cast<double>(l))) { }

Number::Number(long unsigned int l)
  : KJSO(new NumberImp(static_cast<double>(l))) { }

Number::Number(NumberImp *i)
  : KJSO(i) { }

Number::~Number() { }

double Number::value() const
{
  assert(rep);
  return ((NumberImp*)rep)->value();
}

int Number::intValue() const
{
  assert(rep);
  return (int)((NumberImp*)rep)->value();
}

bool Number::isNaN() const
{
  return KJS::isNaN(((NumberImp*)rep)->value());
}

bool Number::isInf() const
{
  return KJS::isInf(((NumberImp*)rep)->value());
}

String::String(const UString &s) : KJSO(new StringImp(UString(s))) { }

String::String(StringImp *i) : KJSO(i) { }

String::~String() { }

UString String::value() const
{
  assert(rep);
  return ((StringImp*)rep)->value();
}

Reference::Reference(const KJSO& b, const UString &p)
  : KJSO(new ReferenceImp(b, p))
{
}

Reference::~Reference()
{
}

Completion::Completion(Imp *d) : KJSO(d) { }

Completion::Completion(Compl c)
  : KJSO(new CompletionImp(c, KJSO(), UString::null))
{
  if (c == Throw)
    KJScriptImp::setException(new UndefinedImp());
}

Completion::Completion(Compl c, const KJSO& v, const UString &t)
  : KJSO(new CompletionImp(c, v, t))
{
  if (c == Throw)
    KJScriptImp::setException(v.imp());
}

Completion::~Completion() { }

Compl Completion::complType() const
{
  assert(rep);
  return ((CompletionImp*)rep)->completion();
}

bool Completion::isValueCompletion() const
{
  assert(rep);
  return !((CompletionImp*)rep)->value().isNull();
}

KJSO Completion::value() const
{
  assert(isA(CompletionType));
  return ((CompletionImp*)rep)->value();
}

UString Completion::target() const
{
  assert(rep);
  return ((CompletionImp*)rep)->target();
}

ListIterator::ListIterator(const List &l)
  : node(l.hook->next)
{
}

List::List()
{
#ifdef KJS_DEBUG_MEM
  count++;
#endif

  static KJSO *null = 0;
  if (!null)
     null = new KJSO();

  hook = new ListNode(*null, 0L, 0L);
  hook->next = hook;
  hook->prev = hook;
}

List::~List()
{
#ifdef KJS_DEBUG_MEM
  count--;
#endif

  clear();
  delete hook;
}

void List::append(const KJSO& obj)
{
  ListNode *n = new ListNode(obj, hook->prev, hook);
  hook->prev->next = n;
  hook->prev = n;
}

void List::prepend(const KJSO& obj)
{
  ListNode *n = new ListNode(obj, hook, hook->next);
  hook->next->prev = n;
  hook->next = n;
}

void List::removeFirst()
{
  erase(hook->next);
}

void List::removeLast()
{
  erase(hook->prev);
}

void List::remove(const KJSO &obj)
{
  if (obj.isNull())
    return;
  ListNode *n = hook->next;
  while (n != hook) {
    if (n->member.imp() == obj.imp()) {
      erase(n);
      return;
    }
    n = n->next;
  }
}

void List::clear()
{
  ListNode *n = hook->next;
  while (n != hook) {
    n = n->next;
    delete n->prev;
  }

  hook->next = hook;
  hook->prev = hook;
}

List *List::copy() const
{
  List *newList = new List();
  ListIterator e = end();
  ListIterator it = begin();

  while(it != e) {
    newList->append(*it);
    ++it;
  }

  return newList;
}

void List::erase(ListNode *n)
{
  if (n != hook) {
    n->next->prev = n->prev;
    n->prev->next = n->next;
    delete n;
  }
}

int List::size() const
{
  int s = 0;
  ListNode *node = hook;
  while ((node = node->next) != hook)
    s++;

  return s;
}

KJSO List::at(int i) const
{
  if (i < 0 || i >= size())
    return Undefined();

  ListIterator it = begin();
  int j = 0;
  while ((j++ < i))
    it++;

  return *it;
}

List *List::emptyList = 0L;

const List *List::empty()
{
  if (!emptyList)
    emptyList = new List;
#ifdef KJS_DEBUG_MEM
  else
    count++;
#endif


  return emptyList;
}
