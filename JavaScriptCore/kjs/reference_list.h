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

#ifndef _KJS_REFERENCE_LIST_H_
#define _KJS_REFERENCE_LIST_H_

#include <types.h>

namespace KJS {


class ReferenceListIterator : private ListIterator {
  friend class ReferenceList;
 public:

  bool operator!=(const ReferenceListIterator &it) const { return this->ListIterator::operator!=(it); }
  ReferenceImp* operator->() const { return (ReferenceImp *)ListIterator::operator->(); }
  Reference operator++(int i) { return Reference((ReferenceImp *)ListIterator::operator++(i).imp()); }

 private:
  ReferenceListIterator(const ListIterator& it) : ListIterator(it) { }
};

class ReferenceList : private List {
 public:
  void ReferenceList::append(const Reference& val) { List::append(val); }

  ReferenceListIterator begin() const { return List::begin(); }
  ReferenceListIterator end() const { return List::end(); }

}; 

}

#endif
