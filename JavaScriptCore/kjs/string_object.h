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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _STRING_OBJECT_H_
#define _STRING_OBJECT_H_

#include "internal.h"
#include "function_object.h"

namespace KJS {

  class StringInstance : public JSObject {
  public:
    StringInstance(JSObject *proto);
    StringInstance(JSObject *proto, const UString &string);

    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    virtual bool deleteProperty(ExecState *exec, const Identifier &propertyName);

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    static JSValue *lengthGetter(ExecState *exec, const Identifier&, const PropertySlot &slot);
    static JSValue *indexGetter(ExecState *exec, const Identifier&, const PropertySlot &slot);
  };

  /**
   * @internal
   *
   * The initial value of String.prototype (and thus all objects created
   * with the String constructor
   */
  class StringPrototype : public StringInstance {
  public:
    StringPrototype(ExecState *exec,
                       ObjectPrototype *objProto);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };

  /**
   * @internal
   *
   * Class to implement all methods that are properties of the
   * String.prototype object
   */
  class StringProtoFunc : public InternalFunctionImp {
  public:
    StringProtoFunc(ExecState *exec, int i, int len);

    virtual bool implementsCall() const;
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

    enum { ToString, ValueOf, CharAt, CharCodeAt, Concat, IndexOf, LastIndexOf,
	   Match, Replace, Search, Slice, Split,
	   Substr, Substring, FromCharCode, ToLowerCase, ToUpperCase,
           ToLocaleLowerCase, ToLocaleUpperCase
#ifndef KJS_PURE_ECMA
	   , Big, Small, Blink, Bold, Fixed, Italics, Strike, Sub, Sup,
	   Fontcolor, Fontsize, Anchor, Link
#endif
    };
  private:
    int id;
  };

  /**
   * @internal
   *
   * The initial value of the the global variable's "String" property
   */
  class StringObjectImp : public InternalFunctionImp {
  public:
    StringObjectImp(ExecState *exec,
                    FunctionPrototype *funcProto,
                    StringPrototype *stringProto);

    virtual bool implementsConstruct() const;
    virtual JSObject *construct(ExecState *exec, const List &args);
    virtual bool implementsCall() const;
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);
  };

  /**
   * @internal
   *
   * Class to implement all methods that are properties of the
   * String object
   */
  class StringObjectFuncImp : public InternalFunctionImp {
  public:
    StringObjectFuncImp(ExecState *exec, FunctionPrototype *funcProto);
    virtual bool implementsCall() const;
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);
  };

} // namespace

#endif

