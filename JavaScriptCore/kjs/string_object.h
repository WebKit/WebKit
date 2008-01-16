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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef STRING_OBJECT_H_
#define STRING_OBJECT_H_

#include "function_object.h"
#include "JSWrapperObject.h"
#include "internal.h"
#include "lookup.h"

namespace KJS {

  class StringInstance : public JSWrapperObject {
  public:
    StringInstance(JSObject *proto);
    StringInstance(JSObject *proto, StringImp*);
    StringInstance(JSObject *proto, const UString&);

    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

    virtual void put(ExecState* exec, const Identifier& propertyName, JSValue*, int attr = None);
    virtual bool deleteProperty(ExecState* exec, const Identifier& propertyName);
    virtual void getPropertyNames(ExecState*, PropertyNameArray&);

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;

    StringImp* internalValue() const { return static_cast<StringImp*>(JSWrapperObject::internalValue());}

  private:
    bool inlineGetOwnPropertySlot(ExecState*, unsigned, PropertySlot&);

    static JSValue* lengthGetter(ExecState*, JSObject *, const Identifier&, const PropertySlot&);
    static JSValue* indexGetter(ExecState*, JSObject *, const Identifier&, const PropertySlot&);
  };

  // WebCore uses this to make style.filter undetectable
  class StringInstanceThatMasqueradesAsUndefined : public StringInstance {
  public:
      StringInstanceThatMasqueradesAsUndefined(JSObject* proto, const UString& string)
          : StringInstance(proto, string) { }
      virtual bool masqueradeAsUndefined() const { return true; }
      virtual bool toBoolean(ExecState*) const { return false; }
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
   * Functions to implement all methods that are properties of the
   * String.prototype object
   */

  JSValue* stringProtoFuncToString(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncValueOf(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncCharAt(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncCharCodeAt(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncConcat(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncIndexOf(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncLastIndexOf(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncMatch(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncReplace(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncSearch(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncSlice(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncSplit(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncSubstr(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncSubstring(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncToLowerCase(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncToUpperCase(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncToLocaleLowerCase(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncToLocaleUpperCase(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncLocaleCompare(ExecState*, JSObject*, const List&);

  JSValue* stringProtoFuncBig(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncSmall(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncBlink(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncBold(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncFixed(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncItalics(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncStrike(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncSub(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncSup(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncFontcolor(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncFontsize(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncAnchor(ExecState*, JSObject*, const List&);
  JSValue* stringProtoFuncLink(ExecState*, JSObject*, const List&);

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
    StringObjectFuncImp(ExecState*, FunctionPrototype*, const Identifier&);
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);
  };

} // namespace

#endif

