/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
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

#ifndef _KJS_OBJECT_H_
#define _KJS_OBJECT_H_

#include <stdlib.h>

#include "ustring.h"

/**
 * @short Main namespace
 */
namespace KJS {

  /**
   * Types of classes derived from KJSO
   */
  enum Type { // main types
              AbstractType = 1,
              UndefinedType,
	      NullType,
	      BooleanType,
	      NumberType,
	      StringType,
	      ObjectType,
	      HostType,
	      ReferenceType,
              CompletionType,
	      // extended types
	      FunctionType,
	      InternalFunctionType,
	      DeclaredFunctionType,
	      AnonymousFunctionType,
	      ConstructorType,
	      ActivationType
  };

  /**
   * Property attributes.
   */
  enum Attribute { None       = 0,
		   ReadOnly   = 1 << 1,
		   DontEnum   = 1 << 2,
		   DontDelete = 1 << 3,
		   Internal   = 1 << 4 };

  /**
   * Types of classes derived from @ref Object.
   */
  enum Class { UndefClass,
	       ArrayClass,
	       StringClass,
	       BooleanClass,
	       NumberClass,
	       ObjectClass,
	       DateClass,
	       RegExpClass,
	       ErrorClass,
	       FunctionClass };

  /**
   * Completion types.
   */
  enum Compl { Normal, Break, Continue, ReturnValue, Throw };

  /**
   * Error codes.
   */
  enum ErrorType { NoError = 0,
		   GeneralError,
		   EvalError,
		   RangeError,
		   ReferenceError,
		   SyntaxError,
		   TypeError,
		   URIError };

  extern const double NaN;
  extern const double Inf;

  // forward declarations
  class Imp;
  class Boolean;
  class Number;
  class String;
  class Object;
  struct Property;
  class PropList;
  class List;

  /**
   * @short Type information.
   */
  struct TypeInfo {
    /**
     * A string denoting the type name. Example: "Number".
     */
    const char *name;
    /**
     * One of the @ref KJS::Type enums.
     */
    Type type;
    /**
     * Pointer to the type information of the base class.
     * NULL if there is none.
     */
    const TypeInfo *base;
    /**
     * Additional specifier for your own use.
     */
    int extra;
    /**
     * Reserved for future extensions (internal).
     */
    void *dummy;
  };

  /**
   * @short Main base class for every KJS object.
   */
  class KJSO {
    friend class ElementNode;
  public:
    /**
     * Constructor.
     */
    KJSO();
    /**
     * @internal
     */
    KJSO(Imp *d);
    /**
     * Copy constructor.
     */
    KJSO(const KJSO &);
    /*
     * Assignment operator
     */
    KJSO& operator=(const KJSO &);
    /**
     * Destructor.
     */
    virtual ~KJSO();
    /**
     * @return True if this object is null, i.e. if there is no data attached
     * to this object. Don't confuse this with the Null object.
     */
    bool isNull() const;
    /**
     * @return True if this objects is of any other value than Undefined.
     */
    bool isDefined() const;
    /**
     * @return the type of the object. One of the @ref KJS::Type enums.
     */
    Type type() const;
    /**
     * Check whether object is of a certain type
     * @param t type to check for
     */
    bool isA(Type t) const { return (type() == t); }
    /**
     * Check whether object is of a certain type. Allows checking of
     * host objects, too.
     * @param type name (Number, Boolean etc.)
     */
    bool isA(const char *s) const;
    /**
     * Use this method when checking for objects. It's safer than checking
     * for a single object type with @ref isA().
     */
    bool isObject() const;
    /**
     * Examine the inheritance structure of this object.
     * @param t Name of the base class.
     * @return True if object is of type t or a derived from such a type.
     */
    bool derivedFrom(const char *s) const;

    /**
     * @return Conversion to primitive type (Undefined, Boolean, Number
     * or String)
     * @param preferred Optional hint. Either StringType or NumberType.
     */
    KJSO toPrimitive(Type preferred = UndefinedType) const; // ECMA 9.1
    /**
     * @return Conversion to Boolean type.
     */
    Boolean toBoolean() const; // ECMA 9.2
    /**
     * @return Conversion to Number type.
     */
    Number toNumber() const; // ECMA 9.3
    /**
     * @return Conversion to double. 0.0 if conversion failed.
     */
    double round() const;
    /**
     * @return Conversion to Number type containing an integer value.
     */
    Number toInteger() const; // ECMA 9.4
    /**
     * @return Conversion to signed integer value.
     */
    int toInt32() const; // ECMA 9.5
    /**
     * @return Conversion to unsigned integer value.
     */
    unsigned int toUInt32() const; // ECMA 9.6
    /**
     * @return Conversion to unsigned short value.
     */
    unsigned short toUInt16() const; // ECMA 9.7
    /**
     * @return Conversion to String type.
     */
    String toString() const; // ECMA 9.8
    /**
     * @return Conversion to Object type.
     */
    Object toObject() const; // ECMA 9.9

    // Properties
    /**
     * Set the internal [[Prototype]] property of this object.
     * @param p A prototype object.
     */
    void setPrototype(const KJSO &p);
    /**
     * Set the "prototype" property of this object. Different from
     * the internal [[Prototype]] property.
     * @param p A prototype object.
     */
    void setPrototypeProperty(const KJSO &p);
    /**
     * @return The internal [[prototype]] property.
     */
    KJSO prototype() const;
    /**
     * The internal [[Get]] method.
     * @return The value of property p.
     */
    KJSO get(const UString &p) const;
    /**
     * The internal [[HasProperty]] method.
     * @param p Property name.
     * @param recursive Indicates whether prototypes are searched as well.
     * @return Boolean value indicating whether the object already has a
     * member with the given name p.
     */
    bool hasProperty(const UString &p, bool recursive = true) const;
    /**
     * The internal [[Put]] method. Sets the specified property to the value v.
     * @param p Property name.
     * @param v Value.
     */
    void put(const UString &p, const KJSO& v);
    /**
     * The internal [[CanPut]] method.
     * @param p Property name.
     * @return A boolean value indicating whether a [[Put]] operation with
     * p succeed.
     */
    bool canPut(const UString &p) const;
    /**
     * The internal [[Delete]] method. Removes the specified property from
     * the object.
     * @param p Property name.
     * @return True if the property was deleted successfully or didn't exist
     * in the first place. False if the DontDelete attribute was set.
     */
    bool deleteProperty(const UString &p);
    /**
     * Same as above put() method except the additional attribute. Right now,
     * this only works with native types as Host Objects don't reimplement
     * this method.
     * @param attr One of @ref KJS::Attribute.
     */
    void put(const UString &p, const KJSO& v, int attr);
    /**
     * Convenience function for adding a Number property constructed from
     * a double value.
     */
    void put(const UString &p, double d, int attr = None);
    /**
     * Convenience function for adding a Number property constructed from
     * an integer value.
     */
    void put(const UString &p, int i, int attr = None);
    /**
     * Convenience function for adding a Number property constructed from
     * an unsigned integer value.
     */
    void put(const UString &p, unsigned int u, int attr = None);

    /**
     * Reference method.
     * @return Reference base if object is a reference. Throws
     * a ReferenceError otherwise.
     */
    KJSO getBase() const;
    /**
     * Reference method.
     * @return Property name of a reference. Null string if object is not
     * a reference.
     */
    UString getPropertyName() const;
    /**
     * Reference method.
     * @return Referenced value. This object if no reference.
     */
    KJSO getValue() const;
    KJSO getValue();  	/* TODO: remove in next version */
    /**
     * Reference method. Set referenced value to v.
     */
    ErrorType putValue(const KJSO& v);

    /**
     * @return True if object supports @ref executeCall() method. That's the
     * case for all objects derived from FunctionType.
     */
    bool implementsCall() const;
    /**
     * Execute function implemented via the @ref Function::execute() method.
     *
     * Note: check availability via @ref implementsCall() beforehand.
     * @param thisV Object serving as the 'this' value.
     * @param args Pointer to the list of arguments or null.
     * @return Result of the function call.
     */
    KJSO executeCall(const KJSO &thisV, const List *args);
    KJSO executeCall(const KJSO &thisV, const List *args, const List *extraScope) const;

    /**
     * Set this object's constructor.
     */
    void setConstructor(KJSO c);

    /**
     * @return A Pointer to the internal implementation.
     */
    Imp *imp() const { return rep; }

#ifdef KJS_DEBUG_MEM
    /**
     * @internal
     */
    static int count;
#endif
  protected:
    /**
     * Pointer to the internal implementation.
     */
    Imp *rep;

  private:
    void putArrayElement(const UString &p, const KJSO &v);
  }; // end of KJSO

  /**
   * @short Base for all implementation classes.
   */
  class Imp {
    friend class KJSO;
    friend class Collector;
    friend class ForInNode;
    friend class Debugger;
  public:
    Imp();
  public:
    virtual KJSO toPrimitive(Type preferred = UndefinedType) const; // ECMA 9.1
    virtual Boolean toBoolean() const; // ECMA 9.2
    virtual Number toNumber() const; // ECMA 9.3
    virtual String toString() const; // ECMA 9.8
    virtual Object toObject() const; // ECMA 9.9

    // properties
    virtual KJSO get(const UString &p) const;
    virtual bool hasProperty(const UString &p, bool recursive = true) const;
    virtual void put(const UString &p, const KJSO& v);
    void put(const UString &p, const KJSO& v, int attr);
    virtual bool canPut(const UString &p) const;
    virtual bool deleteProperty(const UString &p);
    virtual KJSO defaultValue(Type hint) const;

    bool implementsCall() const;

    /**
     * @internal Reserved for mark & sweep garbage collection
     */
    virtual void mark(Imp *imp = 0L);
    bool marked() const;

    Type type() const { return typeInfo()->type; }
    /**
     * @return The TypeInfo struct describing this object.
     */
    virtual const TypeInfo* typeInfo() const { return &info; }

    void setPrototype(const KJSO& p);
    Imp* prototype() const { return proto; }
    void setPrototypeProperty(const KJSO &p);
    void setConstructor(const KJSO& c);

    void* operator new(size_t);
    void operator delete(void*);
    /**
     * @deprecated
     */
    void operator delete(void*, size_t);

#ifdef KJS_DEBUG_MEM
    /**
     * @internal
     */
    static int count;
#endif
  protected:
    virtual ~Imp();
  private:
    Imp(const Imp&);
    Imp& operator=(const Imp&);
    void putArrayElement(const UString &p, const KJSO& v);

    /**
     * Get the property names for this object. To be used by for .. in loops
     * @return The (pointer to the) first element of a PropList, to be deleted
     * by the caller, or 0 if there are no enumerable properties
     */
    PropList *propList(PropList *first = 0L, PropList *last = 0L,
		       bool recursive = true) const;

  public:
    // reference counting mechanism
    inline Imp* ref() { refcount++; return this; }
    inline bool deref() { return (!--refcount); }
    unsigned int refcount;

  private:
    Property *prop;
    Imp *proto;
    static const TypeInfo info;

    // reserved for memory managment - currently used as flags for garbage collection
    // (prev != 0) = marked, (next != 0) = created, (next != this) = created and gc allowed
    Imp *prev, *next;
    // for future extensions
    class ImpInternal;
    ImpInternal *internal;

    void setMarked(bool m);
    void setGcAllowed(bool a);
    bool gcAllowed() const;
    void setCreated(bool c);
    bool created() const;
  };

  /**
   * @short General implementation class for Objects
   */
  class ObjectImp : public Imp {
    friend class Object;
  public:
    ObjectImp(Class c);
    ObjectImp(Class c, const KJSO &v);
    ObjectImp(Class c, const KJSO &v, const KJSO &p);
    virtual ~ObjectImp();
    virtual KJSO toPrimitive(Type preferred = UndefinedType) const;
    virtual Boolean toBoolean() const;
    virtual Number toNumber() const;
    virtual String toString() const;
    virtual Object toObject() const;

    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
    /**
     * @internal Reimplemenation of @ref Imp::mark().
     */
    virtual void mark(Imp *imp = 0L);
  private:
    Class cl;
    Imp *val;
  };

  /**
   * @short Object class encapsulating an internal value.
   */
  class Object : public KJSO {
  public:
    Object(Imp *d);
    Object(Class c = UndefClass);
    Object(Class c, const KJSO& v);
    Object(Class c, const KJSO& v, const Object& p);
    virtual ~Object();
    void setClass(Class c);
    Class getClass() const;
    void setInternalValue(const KJSO& v);
    KJSO internalValue();
    static Object create(Class c);
    static Object create(Class c, const KJSO& val);
    static Object create(Class c, const KJSO& val, const Object &p);
    static Object dynamicCast(const KJSO &obj);
  };

  /**
   * @short Implementation base class for Host Objects.
   */
  class HostImp : public Imp {
  public:
    HostImp();
    virtual ~HostImp();
    virtual const TypeInfo* typeInfo() const { return &info; }

    virtual Boolean toBoolean() const;
    virtual String toString() const;

    static const TypeInfo info;
  };

  class KJScriptImp;
  /**
   * The Global object represents the global namespace. It holds the native
   * objects like String and functions like eval().
   *
   * It also serves as a container for variables created by the user, i.e.
   * the statement
   * <pre>
   *   var i = 2;
   * </pre>
   * will basically perform a Global::current().put("i", Number(2)); operation.
   *
   * @short Unique global object containing initial native properties.
   */
  class Global : public Object {
    friend class KJScriptImp;
  public:
    /**
     * Constructs a Global object. This is done by the interpreter once and
     * there should be no need to create an instance on your own. Usually,
     * you'll just want to access the current instance.
     * For example like this:
     * <pre>
     * Global global(Global::current());
     * KJSO proto = global.objectPrototype();
     * </pre>
     */
    Global();
    /**
     * Destruct the Global object.
     */
    virtual ~Global();
    /**
     * @return A reference to the Global object belonging to the current
     * interpreter instance.
     */
    static Global current();
    /**
     * @return A handle to Object.prototype.
     */
    KJSO objectPrototype() const;
    /**
     * @return A handle to Function.prototype.
     */
    KJSO functionPrototype() const;
    /**
     * Set a filter object that will intercept all put() and get() calls
     * to the global object. If this object returns Undefined on get() the
     * request will be passed on the global object.
     * @deprecated
     */
    void setFilter(const KJSO &f);
    /**
     * Return a handle to the filter object (see @ref setFilter()).
     * Null if no filter has been installed.
     * @deprecated
     */
    KJSO filter() const;
    /**
     * Returns the extra user data set for this global object. Null by default.
     * Typical usage if you need to query any info related to the currently
     * running interpreter:
     *
     *    MyMainWindow *m = (MyMainWindow*)Global::current().extra();
     */
    void *extra() const;
    /**
     * Set the extra user data for this global object. It's not used by the
     * interpreter itself and can therefore be used to bind arbitrary data
     * to each interpreter instance.
     */
    void setExtra(void *e);
  private:
    Global(void *);
    void init();
    void clear();
  };

  /**
   * @short Factory methods for error objects.
   */
  class Error {
  public:
    /**
     * Factory method for error objects. The error will be registered globally
     * and the execution will continue as if a "throw" statement was
     * encountered.
     * @param e Type of error.
     * @param m Optional error message.
     * @param l Optional line number.
     */
    static KJSO create(ErrorType e, const char *m = 0, int l = -1);
    /**
     * Same as above except the different return type (which is not casted
     * here).
     */
    static Object createObject(ErrorType e, const char *m = 0, int l = -1);
  };

}; // namespace

#endif
