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


#ifndef _KJS_OBJECT_H_
#define _KJS_OBJECT_H_

// Objects

// maximum global call stack size. Protects against accidental or
// malicious infinite recursions. Define to -1 if you want no limit.
#define KJS_MAX_STACK 1000

#include "value.h"
#include "types.h"

namespace KJS {

  class ObjectImpPrivate;
  class PropertyMap;
  class HashTable;
  class HashEntry;
  class ListImp;

  // ECMA 262-3 8.6.1
  // Attributes (only applicable to the Object type)
  enum Attribute { None       = 0,
                   ReadOnly   = 1 << 1, // property can be only read, not written
                   DontEnum   = 1 << 2, // property doesn't appear in (for .. in ..)
                   DontDelete = 1 << 3, // property can't be deleted
                   Internal   = 1 << 4, // an internal property, set to by pass checks
                   Function   = 1 << 5 }; // property is a function - only used by static hashtables

  /**
   * Class Information
   */
  struct ClassInfo {
    /**
     * A string denoting the class name. Example: "Window".
     */
    const char* className;
    /**
     * Pointer to the class information of the base class.
     * 0L if there is none.
     */
    const ClassInfo *parentClass;
    /**
     * Static hash-table of properties.
     */
    const HashTable *propHashTable;
    /**
     * Reserved for future extension.
     */
    void *dummy;
  };

  /**
   * Represents an Object. This is a wrapper for ObjectImp
   */
  class Object : public Value {
  public:
    Object();
    explicit Object(ObjectImp *v);
    Object(const Object &v);
    virtual ~Object();

    Object& operator=(const Object &v);

    virtual const ClassInfo *classInfo() const;
    bool inherits(const ClassInfo *cinfo) const;

    /**
     * Converts a Value into an Object. If the value's type is not ObjectType,
     * a null object will be returned (i.e. one with it's internal pointer set
     * to 0). If you do not know for sure whether the value is of type
     * ObjectType, you should check the @ref isNull() methods afterwards before
     * calling any methods on the Object.
     *
     * @return The value converted to an object
     */
    static Object dynamicCast(const Value &v);

    /**
     * Returns the prototype of this object. Note that this is not the same as
     * the "prototype" property.
     *
     * See ECMA 8.6.2
     *
     * @return The object's prototype
     */
    Value prototype() const;

    /**
     * Returns the class name of the object
     *
     * See ECMA 8.6.2
     *
     * @return The object's class name
     */
    UString className() const;

    /**
     * Retrieves the specified property from the object. If neither the object
     * or any other object in it's prototype chain have the property, this
     * function will return Undefined.
     *
     * See ECMA 8.6.2.1
     *
     * @param exec The current execution state
     * @param propertyName The name of the property to retrieve
     *
     * @return The specified property, or Undefined
     */
    Value get(ExecState *exec, const UString &propertyName) const;

    /**
     * Sets the specified property.
     *
     * See ECMA 8.6.2.2
     *
     * @param exec The current execution state
     * @param propertyName The name of the property to set
     * @param propertyValue The value to set
     */
    void put(ExecState *exec, const UString &propertyName,
             const Value &value, int attr = None);

    /**
     * Used to check whether or not a particular property is allowed to be set
     * on an object
     *
     * See ECMA 8.6.2.3
     *
     * @param exec The current execution state
     * @param propertyName The name of the property
     * @return true if the property can be set, otherwise false
     */
    bool canPut(ExecState *exec, const UString &propertyName) const;

    /**
     * Checks to see whether the object (or any object in it's prototype chain)
     * has a property with the specified name.
     *
     * See ECMA 8.6.2.4
     *
     * @param exec The current execution state
     * @param propertyName The name of the property to check for
     * @return true if the object has the property, otherwise false
     */
    bool hasProperty(ExecState *exec, const UString &propertyName) const;

    /**
     * Removes the specified property from the object.
     *
     * See ECMA 8.6.2.5
     *
     * @param exec The current execution state
     * @param propertyName The name of the property to delete
     * @return true if the property was successfully deleted or did not
     * exist on the object. false if deleting the specified property is not
     * allowed.
     */
    bool deleteProperty(ExecState *exec, const UString &propertyName);

    /**
     * Converts the object into a primitive value. The value return may differ
     * depending on the supplied hint
     *
     * See ECMA 8.6.2.6
     *
     * @param exec The current execution state
     * @param hint The desired primitive type to convert to
     * @return A primitive value converted from the objetc. Note that the
     * type of primitive value returned may not be the same as the requested
     * hint.
     */
    Value defaultValue(ExecState *exec, Type hint) const;

    /**
     * Whether or not the object implements the construct() method. If this
     * returns false you should not call the construct() method on this
     * object (typically, an assertion will fail to indicate this).
     *
     * @return true if this object implements the construct() method, otherwise
     * false
     */
    bool implementsConstruct() const;

    /**
     * Creates a new object based on this object. Typically this means the
     * following:
     * 1. A new object is created
     * 2. The prototype of the new object is set to the value of this object's
     *    "prototype" property
     * 3. The call() method of this object is called, with the new object
     *    passed as the this value
     * 4. The new object is returned
     *
     * In some cases, Host objects may differ from these semantics, although
     * this is discouraged.
     *
     * If an error occurs during construction, the execution state's exception
     * will be set. This can be tested for with @ref ExecState::hadException().
     * Under some circumstances, the exception object may also be returned.
     *
     * Note: This function should not be called if implementsConstruct() returns
     * false, in which case it will result in an assertion failure.
     *
     * @param exec The current execution state
     * @param args The arguments to be passed to call() once the new object has
     * been created
     * @return The newly created &amp; initialized object
     */
    Object construct(ExecState *exec, const List &args);

    /**
     * Whether or not the object implements the call() method. If this returns
     * false you should not call the call() method on this object (typically,
     * an assertion will fail to indicate this).
     *
     * @return true if this object implements the call() method, otherwise
     * false
     */
    bool implementsCall() const;


    /**
     * Calls this object as if it is a function.
     *
     * Note: This function should not be called if implementsCall() returns
     * false, in which case it will result in an assertion failure.
     *
     * See ECMA 8.6.2.3
     *
     * @param exec The current execution state
     * @param thisObj The obj to be used as "this" within function execution.
     * Note that in most cases this will be different from the C++ "this"
     * object. For example, if the ECMAScript code "window.location.toString()"
     * is executed, call() will be invoked on the C++ object which implements
     * the toString method, with the thisObj being window.location
     * @param args List of arguments to be passed to the function
     * @return The return value from the function
     */
    Value call(ExecState *exec, Object &thisObj, const List &args);

    /**
     * Whether or not the object implements the hasInstance() method. If this
     * returns false you should not call the hasInstance() method on this
     * object (typically, an assertion will fail to indicate this).
     *
     * @return true if this object implements the hasInstance() method,
     * otherwise false
     */
    bool implementsHasInstance() const;

    /**
     * Checks whether value delegates behaviour to this object. Used by the
     * instanceof operator.
     *
     * @param exec The current execution state
     * @param value The value to check
     * @return true if value delegates behaviour to this object, otherwise
     * false
     */
    Boolean hasInstance(ExecState *exec, const Value &value);

    /**
     * Returns the scope of this object. This is used when execution declared
     * functions - the execution context for the function is initialized with
     * extra object in it's scope. An example of this is functions declared
     * inside other functions:
     *
     * function f() {
     *
     *   function b() {
     *     return prototype;
     *   }
     *
     *   var x = 4;
     *   // do some stuff
     * }
     * f.prototype = new String();
     *
     * When the function f.b is executed, its scope will include properties of
     * f. So in the example above the return value of f.b() would be the new
     * String object that was assigned to f.prototype.
     *
     * @param exec The current execution state
     * @return The function's scope
     */
    const List scope() const;
    void setScope(const List &s);

    /**
     * Returns a List of References to all the properties of the object. Used
     * in "for x in y" statements. The list is created new, so it can be freely
     * modified without affecting the object's properties. It should be deleted
     * by the caller.
     *
     * Subclasses can override this method in ObjectImpl to provide the
     * appearance of
     * having extra properties other than those set specifically with put().
     *
     * @param exec The current execution state
     * @param recursive Whether or not properties in the object's prototype
     * chain should be
     * included in the list.
     * @return A List of References to properties of the object.
     **/
    List propList(ExecState *exec, bool recursive = true);

    /**
     * Returns the internal value of the object. This is used for objects such
     * as String and Boolean which are wrappers for native types. The interal
     * value is the actual value represented by the wrapper objects.
     *
     * @see ECMA 8.6.2
     * @return The internal value of the object
     */
    Value internalValue() const;

    /**
     * Sets the internal value of the object
     *
     * @see internalValue()
     *
     * @param v The new internal value
     */
    void setInternalValue(const Value &v);
  };

  class ObjectImp : public ValueImp {
  public:
    /**
     * Creates a new ObjectImp with the specified prototype
     *
     * @param proto The prototype
     */
    ObjectImp(const Object &proto);

    /**
     * Creates a new ObjectImp with a prototype of Null()
     * (that is, the ECMAScript "null" value, not a null Object).
     *
     */
    ObjectImp();

    virtual ~ObjectImp();

    virtual void mark();

    Type type() const;

    /**
     * A pointer to a ClassInfo struct for this class. This provides a basic
     * facility for run-time type information, and can be used to check an
     * object's class an inheritance (see @ref inherits()). This should
     * always return a statically declared pointer, or 0 to indicate that
     * there is no class information.
     *
     * This is primarily useful if you have application-defined classes that you
     * wish to check against for casting purposes.
     *
     * For example, to specify the class info for classes FooImp and BarImp,
     * where FooImp inherits from BarImp, you would add the following in your
     * class declarations:
     *
     *   class BarImp : public ObjectImp {
     *     virtual const ClassInfo *classInfo() const { return &info; }
     *     static const ClassInfo info;
     *     // ...
     *   };
     *
     *   class FooImp : public ObjectImp {
     *     virtual const ClassInfo *classInfo() const { return &info; }
     *     static const ClassInfo info;
     *     // ...
     *   };
     *
     * And in your source file:
     *
     *   const ClassInfo BarImp::info = {0, 0, 0}; // no parent class
     *   const ClassInfo FooImp::info = {&BarImp::info, 0, 0};
     *
     * @see inherits()
     */
    virtual const ClassInfo *classInfo() const;

    /**
     * Checks whether this object inherits from the class with the specified
     * classInfo() pointer. This requires that both this class and the other
     * class return a non-NULL pointer for their classInfo() methods (otherwise
     * it will return false).
     *
     * For example, for two ObjectImp pointers obj1 and obj2, you can check
     * if obj1's class inherits from obj2's class using the following:
     *
     *   if (obj1->inherits(obj2->classInfo())) {
     *     // ...
     *   }
     *
     * If you have a handle to a statically declared ClassInfo, such as in the
     * @ref classInfo() example, you can check for inheritance without needing
     * an instance of the other class:
     *
     *   if (obj1->inherits(FooImp::info)) {
     *     // ...
     *   }
     *
     * @param cinfo The ClassInfo pointer for the class you want to check
     * inheritance against.
     * @return true if this object's class inherits from class with the
     * ClassInfo pointer specified in cinfo
     */
    bool inherits(const ClassInfo *cinfo) const;

    // internal properties (ECMA 262-3 8.6.2)

    /**
     * Implementation of the [[Prototype]] internal property (implemented by
     * all Objects)
     *
     * @see Object::prototype()
     */
    Value prototype() const;
    void setPrototype(const Value &proto);

    /**
     * Implementation of the [[Class]] internal property (implemented by all
     * Objects)
     *
     * The default implementation uses classInfo().
     * You should either implement @ref classInfo(), or
     * if you simply need a classname, you can reimplement @ref className()
     * instead.
     *
     * @see Object::className()
     */
    virtual UString className() const;

    /**
     * Implementation of the [[Get]] internal property (implemented by all
     * Objects)
     *
     * @see Object::get()
     */
    // [[Get]] - must be implemented by all Objects
    virtual Value get(ExecState *exec, const UString &propertyName) const;

    /**
     * Implementation of the [[Put]] internal property (implemented by all
     * Objects)
     *
     * @see Object::put()
     */
    virtual void put(ExecState *exec, const UString &propertyName,
                     const Value &value, int attr = None);

    /**
     * Implementation of the [[CanPut]] internal property (implemented by all
     * Objects)
     *
     * @see Object::canPut()
     */
    virtual bool canPut(ExecState *exec, const UString &propertyName) const;

    /**
     * Implementation of the [[HasProperty]] internal property (implemented by
     * all Objects)
     *
     * @see Object::hasProperty()
     */
    virtual bool hasProperty(ExecState *exec,
			     const UString &propertyName) const;

    /**
     * Implementation of the [[Delete]] internal property (implemented by all
     * Objects)
     *
     * @see Object::deleteProperty()
     */
    virtual bool deleteProperty(ExecState *exec,
                                const UString &propertyName);

    /**
     * Remove all properties from this object.
     * This doesn't take DontDelete into account, and isn't in the ECMA spec.
     * It's simply a quick way to remove everything before destroying.
     */
    void deleteAllProperties( ExecState * );

    /**
     * Implementation of the [[DefaultValue]] internal property (implemented by
     * all Objects)
     *
     * @see Object::defaultValue()
     */
    virtual Value defaultValue(ExecState *exec, Type hint) const;

    virtual bool implementsConstruct() const;
    /**
     * Implementation of the [[Construct]] internal property
     *
     * @see Object::construct()
     */
    virtual Object construct(ExecState *exec, const List &args);

    virtual bool implementsCall() const;
    /**
     * Implementation of the [[Call]] internal property
     *
     * @see Object::call()
     */
    virtual Value call(ExecState *exec, Object &thisObj,
                       const List &args);

    virtual bool implementsHasInstance() const;
    /**
     * Implementation of the [[HasInstance]] internal property
     *
     * @see Object::hasInstance()
     */
    virtual Boolean hasInstance(ExecState *exec, const Value &value);

    /**
     * Implementation of the [[Scope]] internal property
     *
     * @see Object::scope()
     */
    const List scope() const;
    void setScope(const List &s);

    List propList(ExecState *exec, bool recursive = true);

    Value internalValue() const;
    void setInternalValue(const Value &v);

    Value toPrimitive(ExecState *exec,
                      Type preferredType = UnspecifiedType) const;
    bool toBoolean(ExecState *exec) const;
    double toNumber(ExecState *exec) const;
    int toInteger(ExecState *exec) const;
    int toInt32(ExecState *exec) const;
    unsigned int toUInt32(ExecState *exec) const;
    unsigned short toUInt16(ExecState *exec) const;
    UString toString(ExecState *exec) const;
    Object toObject(ExecState *exec) const;

    ValueImp* getDirect(const UString& propertyName) const;
  private:
    const HashEntry* findPropertyHashEntry( const UString& propertyName ) const;
    ObjectImpPrivate *_od;
    PropertyMap *_prop;
    ValueImp *_proto;
    ValueImp *_internalValue;
    ListImp *_scope;
  };

  /**
   * Types of Native Errors available. For custom errors, GeneralError
   * should be used.
   */
  enum ErrorType { GeneralError   = 0,
                   EvalError      = 1,
                   RangeError     = 2,
                   ReferenceError = 3,
                   SyntaxError    = 4,
                   TypeError      = 5,
                   URIError       = 6};

  /**
   * @short Factory methods for error objects.
   */
  class Error {
  public:
    /**
     * Factory method for error objects.
     *
     * @param exec The current execution state
     * @param errtype Type of error.
     * @param message Optional error message.
     * @param lineno Optional line number.
     * @param lineno Optional source id.
     */
    static Object create(ExecState *exec, ErrorType errtype = GeneralError,
                         const char *message = 0, int lineno = -1,
                         int sourceId = -1);

    /**
     * Array of error names corresponding to @ref ErrorType
     */
    static const char * const * const errorNames;
  };

}; // namespace

#endif // _KJS_OBJECT_H_
