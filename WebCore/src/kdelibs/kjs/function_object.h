// -*- c-basic-offset: 2 -*-
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
 *
 *  $Id$
 */

#ifndef _FUNCTION_OBJECT_H_
#define _FUNCTION_OBJECT_H_

#include "internal.h"
#include "object_object.h"
#include "function.h"

namespace KJS {

  /**
   * @internal
   *
   * The initial value of Function.prototype (and thus all objects created
   * with the Function constructor
   */
  class FunctionPrototypeImp : public InternalFunctionImp {
  public:
    FunctionPrototypeImp(ExecState *exec);
    virtual ~FunctionPrototypeImp();

    virtual bool implementsCall() const;
    virtual Value call(ExecState *exec, Object &thisObj, const List &args);
  };

  /**
   * @internal
   *
   * Class to implement all methods that are properties of the
   * Function.prototype object
   */
  class FunctionProtoFuncImp : public InternalFunctionImp {
  public:
    FunctionProtoFuncImp(ExecState *exec,
                        FunctionPrototypeImp *funcProto, int i, int len);

    virtual bool implementsCall() const;
    virtual Value call(ExecState *exec, Object &thisObj, const List &args);

    enum { ToString, Apply, Call };
  private:
    int id;
  };

  /**
   * @internal
   *
   * The initial value of the the global variable's "Function" property
   */
  class FunctionObjectImp : public InternalFunctionImp {
  public:
    FunctionObjectImp(ExecState *exec, FunctionPrototypeImp *funcProto);
    virtual ~FunctionObjectImp();

    virtual bool implementsConstruct() const;
    virtual Object construct(ExecState *exec, const List &args);
    virtual bool implementsCall() const;
    virtual Value call(ExecState *exec, Object &thisObj, const List &args);
  };

}; // namespace

#endif // _FUNCTION_OBJECT_H_
