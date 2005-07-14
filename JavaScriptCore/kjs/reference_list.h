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

#ifndef _KJS_REFERENCE_LIST_H_
#define _KJS_REFERENCE_LIST_H_

#include "types.h"
#include "reference.h"

namespace KJS {

  class ReferenceListNode;
  class ReferenceListHeadNode;

  class ReferenceListIterator {
    friend class ReferenceList;
  
  public:
    bool operator!=(const ReferenceListIterator &it) const;
    const Reference *operator->() const;
    const Reference &operator++(int i);
    
  private:
    ReferenceListIterator(ReferenceListNode *n);
    ReferenceListIterator();
    ReferenceListNode *node;
  };
  
  class ReferenceList {
  public:
    ReferenceList();
    ReferenceList(const ReferenceList &list);
    ReferenceList &operator=(const ReferenceList &list);
    ~ReferenceList();

    void append(const Reference& val);
    int length();

    ReferenceListIterator begin() const;
    ReferenceListIterator end() const;
    
  private:
    void swap(ReferenceList &list);
    ReferenceListHeadNode *head;
    ReferenceListNode *tail;
  }; 
  
}

#endif
