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

#ifndef _KJS_FUNCTION_H_
#define _KJS_FUNCTION_H_

#include <assert.h>

#include "object.h"
#include "types.h"

namespace KJS {

  enum CodeType { GlobalCode,
		  EvalCode,
		  FunctionCode,
		  AnonymousCode,
		  HostCode };

  enum FunctionAttribute { ImplicitNone, ImplicitThis, ImplicitParents };

  class Function;
  class Parameter;

  /**
   * @short Implementation class for Functions.
   */
  class FunctionImp : public ObjectImp {
    friend class Function;
  public:
    FunctionImp();
    FunctionImp(const UString &n);
    virtual ~FunctionImp();
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
    virtual Completion execute(const List &) = 0;
    bool hasAttribute(FunctionAttribute a) const { return (attr & a) != 0; }
    virtual CodeType codeType() const = 0;
    KJSO thisValue() const;
    void addParameter(const UString &n);
    void setLength(int l);
    KJSO executeCall(Imp *thisV, const List *args);
    KJSO executeCall(Imp *thisV, const List *args, const List *extraScope);
    UString name() const;
  protected:
    UString ident;
    FunctionAttribute attr;
    Parameter *param;
  private:
    void processParameters(const List *);
  };

  /**
   * @short Abstract base class for internal functions.
   */
  class InternalFunctionImp : public FunctionImp {
  public:
    InternalFunctionImp();
    InternalFunctionImp(int l);
    InternalFunctionImp(const UString &n);
    virtual ~InternalFunctionImp() { }
    virtual String toString() const;
    virtual KJSO toPrimitive(Type) const { return toString(); }
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
    virtual Completion execute(const List &);
    virtual CodeType codeType() const { return HostCode; }
  };

  /**
   * @short Base class for Function objects.
   */
  class Function : public KJSO {
  public:
    Function(Imp *);
    virtual ~Function() { }
    Completion execute(const List &);
    bool hasAttribute(FunctionAttribute a) const;
    CodeType codeType() const { return HostCode; }
    KJSO thisValue() const;
  };
  
  /**
   * @short Implementation class for Constructors.
   */
  class ConstructorImp : public InternalFunctionImp {
  public:
    ConstructorImp();
    ConstructorImp(const UString &n);	/* TODO: add length */
    ConstructorImp(const KJSO &, int);
    ConstructorImp(const UString &n, const KJSO &p, int len);
    virtual ~ConstructorImp();
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
    virtual Completion execute(const List &);
    virtual Object construct(const List &) = 0;
  };

  /**
    * @short Constructor object for use with the 'new' operator
    */
  class Constructor : public Function {
  public:
    Constructor(Imp *);
    virtual ~Constructor();
    //    Constructor(const Object& proto, int len);
    /**
     * @return @ref ConstructorType
     */
    Completion execute(const List &);
    Object construct(const List &args);
    static Constructor dynamicCast(const KJSO &obj);
  };

}; // namespace

#endif
