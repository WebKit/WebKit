/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _OBJECT_OBJECT_H_
#define _OBJECT_OBJECT_H_

#include "object.h"
#include "function.h"

namespace KJS {

  class ObjectObject : public ConstructorImp {
  public:
    ObjectObject(const Object &funcProto, const Object &objProto);
    Completion execute(const List &);
    Object construct(const List &);
  };

  class ObjectPrototype : public ObjectImp {
  public:
    ObjectPrototype();
    bool hasProperty(const UString &p, bool recursive) const;
    KJSO get(const UString &p) const;
    enum { ToString, ValueOf };
  };

  class ObjectProtoFunc : public InternalFunctionImp {
  public:
    ObjectProtoFunc(int i);
    Completion execute(const List &);
  private:
    int id;
  };

}; // namespace

#endif
