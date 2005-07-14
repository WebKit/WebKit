// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003 Apple Computer, Inc
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

#include "reference_list.h"

#include "protect.h"

namespace KJS {
  class ReferenceListNode {
    friend class ReferenceList;
    friend class ReferenceListIterator;

  protected:
    ReferenceListNode(const Reference &ref) : reference(ref), next(NULL) {}

  private:
    ProtectedReference reference;
    ReferenceListNode *next;
  };

  class ReferenceListHeadNode : ReferenceListNode {
    friend class ReferenceList;
    friend class ReferenceListIterator;
    
    ReferenceListHeadNode(const Reference &ref) : ReferenceListNode(ref), refcount(1) {}
    int refcount;
    int length;
  };

}

using namespace KJS;

// ReferenceList

ReferenceList::ReferenceList() : 
  head(NULL),
  tail(NULL)
{
}

ReferenceList::ReferenceList(const ReferenceList &list)
{
  head = list.head;
  tail = list.tail;
  if (head != NULL) {
    head->refcount++;
  }
}

ReferenceList &ReferenceList::operator=(const ReferenceList &list)
{
  ReferenceList tmp(list);
  tmp.swap(*this);

  return *this;
}

void ReferenceList::swap(ReferenceList &list)
{
  ReferenceListHeadNode *tmpHead = list.head;
  list.head = head;
  head = tmpHead;

  ReferenceListNode *tmpTail = list.tail;
  list.tail = tail;
  tail = tmpTail;
}


void ReferenceList::append(const Reference& ref)
{
  if (tail == NULL) {
    tail = head = new ReferenceListHeadNode(ref);
  } else {
    tail->next = new ReferenceListNode(ref);
    tail = tail->next;
  }
  head->length++;
}

int ReferenceList::length()
{
  return head ? head->length : 0;
}

ReferenceList::~ReferenceList()
{
  if (head != NULL && --(head->refcount) == 0) {
    ReferenceListNode *next;
    
    for (ReferenceListNode *p = head; p != NULL; p = next) {
      next = p->next;
      if (p == head) {
	delete (ReferenceListHeadNode *)p;
      } else {
	delete p;
      }
    }
  }
}
    
ReferenceListIterator ReferenceList::begin() const
{
  return ReferenceListIterator(head);
}

ReferenceListIterator ReferenceList::end() const
{
  return ReferenceListIterator(NULL);
}


// ReferenceListIterator


ReferenceListIterator::ReferenceListIterator(ReferenceListNode *n) :
  node(n)
{
}

bool ReferenceListIterator::operator!=(const ReferenceListIterator &it) const 
{ 
  return node != it.node;
}

const Reference *ReferenceListIterator::operator->() const 
{ 
  return &node->reference;
}

const Reference &ReferenceListIterator::operator++(int i) 
{
  const Reference &ref = node->reference;
  node = node->next;
  return ref;
}
