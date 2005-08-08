// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003 Apple Computer, Inc.
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

#ifndef KJS_OBJECT_H
#define KJS_OBJECT_H

// Objects

// maximum global call stack size. Protects against accidental or
// malicious infinite recursions. Define to -1 if you want no limit.
#if APPLE_CHANGES
// Given OS X stack sizes we run out of stack at about 350 levels.
// If we improve our stack usage, we can bump this number.
#define KJS_MAX_STACK 100
#else
#define KJS_MAX_STACK 1000
#endif

#include "interpreter.h"
#include "property_map.h"
#include "property_slot.h"
#include "scope_chain.h"

namespace KJS {

  class HashTable;
  class HashEntry;
  class ListImp;

  // ECMA 262-3 8.6.1
  // Property attributes
  enum Attribute { None       = 0,
                   ReadOnly   = 1 << 1, // property can be only read, not written
                   DontEnum   = 1 << 2, // property doesn't appear in (for .. in ..)
                   DontDelete = 1 << 3, // property can't be deleted
                   Internal   = 1 << 4, // an internal property, set to bypass checks
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
  
  class ObjectImp : public AllocatedValueImp {
  public:
    /**
     * Creates a new ObjectImp with the specified prototype
     *
     * @param proto The prototype
     */
    ObjectImp(ObjectImp *proto);

    /**
     * Creates a new ObjectImp with a prototype of Null()
     * (that is, the ECMAScript "null" value, not a null object pointer).
     *
     */
    ObjectImp();

    virtual void mark();

    Type type() const;

    /**
     * A pointer to a ClassInfo struct for this class. This provides a basic
     * facility for run-time type information, and can be used to check an
     * object's class an inheritance (see inherits()). This should
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
     * \code
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
     * \endcode
     *
     * And in your source file:
     *
     * \code
     *   const ClassInfo BarImp::info = {"Bar", 0, 0, 0}; // no parent class
     *   const ClassInfo FooImp::info = {"Foo", &BarImp::info, 0, 0};
     * \endcode
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
     * classInfo() example, you can check for inheritance without needing
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
     * Returns the prototype of this object. Note that this is not the same as
     * the "prototype" property.
     *
     * See ECMA 8.6.2
     *
     * @return The object's prototype
     */
    /**
     * Implementation of the [[Prototype]] internal property (implemented by
     * all Objects)
     */
    ValueImp *prototype() const;
    void setPrototype(ValueImp *proto);

    /**
     * Returns the class name of the object
     *
     * See ECMA 8.6.2
     *
     * @return The object's class name
     */
    /**
     * Implementation of the [[Class]] internal property (implemented by all
     * Objects)
     *
     * The default implementation uses classInfo().
     * You should either implement classInfo(), or
     * if you simply need a classname, you can reimplement className()
     * instead.
     */
    virtual UString className() const;

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
    /**
     * Implementation of the [[Get]] internal property (implemented by all
     * Objects)
     */
    // [[Get]] - must be implemented by all Objects
    ValueImp *get(ExecState *exec, const Identifier &propertyName) const;
    ValueImp *get(ExecState *exec, unsigned propertyName) const;

    bool getProperty(ExecState *exec, const Identifier& propertyName, ValueImp*& result) const;
    bool getProperty(ExecState *exec, unsigned propertyName, ValueImp*& result) const;

    bool getPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    bool getPropertySlot(ExecState *, unsigned, PropertySlot&);

    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual bool getOwnPropertySlot(ExecState *, unsigned index, PropertySlot&);

    /**
     * Sets the specified property.
     *
     * See ECMA 8.6.2.2
     *
     * @param exec The current execution state
     * @param propertyName The name of the property to set
     * @param propertyValue The value to set
     */
    /**
     * Implementation of the [[Put]] internal property (implemented by all
     * Objects)
     */
    virtual void put(ExecState *exec, const Identifier &propertyName,
                     ValueImp *value, int attr = None);
    virtual void put(ExecState *exec, unsigned propertyName,
                     ValueImp *value, int attr = None);

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
    /**
     * Implementation of the [[CanPut]] internal property (implemented by all
     * Objects)
     */
    virtual bool canPut(ExecState *exec, const Identifier &propertyName) const;

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
    /**
     * Implementation of the [[HasProperty]] internal property (implemented by
     * all Objects)
     */
    bool hasProperty(ExecState *exec,
			     const Identifier &propertyName) const;
    bool hasProperty(ExecState *exec, unsigned propertyName) const;

    /**
     * Checks to see whether the object has a property with the specified name.
     *
     * See ECMA 15.2.4.5
     *
     * @param exec The current execution state
     * @param propertyName The name of the property to check for
     * @return true if the object has the property, otherwise false
     */
    virtual bool hasOwnProperty(ExecState *exec, const Identifier &propertyName) const;
    virtual bool hasOwnProperty(ExecState *exec, unsigned propertyName) const;

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
    /**
     * Implementation of the [[Delete]] internal property (implemented by all
     * Objects)
     */
    virtual bool deleteProperty(ExecState *exec,
                                const Identifier &propertyName);
    virtual bool deleteProperty(ExecState *exec, unsigned propertyName);

    /**
     * Remove all properties from this object.
     * This doesn't take DontDelete into account, and isn't in the ECMA spec.
     * It's simply a quick way to remove everything before destroying.
     */
    void deleteAllProperties(ExecState *);

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
    /**
     * Implementation of the [[DefaultValue]] internal property (implemented by
     * all Objects)
     */
    virtual ValueImp *defaultValue(ExecState *exec, Type hint) const;

    /**
     * Whether or not the object implements the construct() method. If this
     * returns false you should not call the construct() method on this
     * object (typically, an assertion will fail to indicate this).
     *
     * @return true if this object implements the construct() method, otherwise
     * false
     */
    virtual bool implementsConstruct() const;

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
     * will be set. This can be tested for with ExecState::hadException().
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
    /**
     * Implementation of the [[Construct]] internal property
     */
    virtual ObjectImp *construct(ExecState *exec, const List &args);
    virtual ObjectImp *construct(ExecState *exec, const List &args, const UString &sourceURL, int lineNumber);

    /**
     * Whether or not the object implements the call() method. If this returns
     * false you should not call the call() method on this object (typically,
     * an assertion will fail to indicate this).
     *
     * @return true if this object implements the call() method, otherwise
     * false
     */
    virtual bool implementsCall() const;

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
     * object. For example, if the ECMAScript code "window.location->toString()"
     * is executed, call() will be invoked on the C++ object which implements
     * the toString method, with the thisObj being window.location
     * @param args List of arguments to be passed to the function
     * @return The return value from the function
     */
    /**
     * Implementation of the [[Call]] internal property
     */
    ValueImp *call(ExecState *exec, ObjectImp *thisObj, const List &args);
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args);

    /**
     * Whether or not the object implements the hasInstance() method. If this
     * returns false you should not call the hasInstance() method on this
     * object (typically, an assertion will fail to indicate this).
     *
     * @return true if this object implements the hasInstance() method,
     * otherwise false
     */
    virtual bool implementsHasInstance() const;

    /**
     * Checks whether value delegates behavior to this object. Used by the
     * instanceof operator.
     *
     * @param exec The current execution state
     * @param value The value to check
     * @return true if value delegates behavior to this object, otherwise
     * false
     */
    /**
     * Implementation of the [[HasInstance]] internal property
     */
    virtual bool hasInstance(ExecState *exec, ValueImp *value);

    /**
     * Returns the scope of this object. This is used when execution declared
     * functions - the execution context for the function is initialized with
     * extra object in it's scope. An example of this is functions declared
     * inside other functions:
     *
     * \code
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
     * \endcode
     *
     * When the function f.b is executed, its scope will include properties of
     * f. So in the example above the return value of f.b() would be the new
     * String object that was assigned to f.prototype.
     *
     * @param exec The current execution state
     * @return The function's scope
     */
    /**
     * Implementation of the [[Scope]] internal property
     */
    const ScopeChain &scope() const { return _scope; }
    void setScope(const ScopeChain &s) { _scope = s; }

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
    virtual ReferenceList propList(ExecState *exec, bool recursive = true);

    /**
     * Returns the internal value of the object. This is used for objects such
     * as String and Boolean which are wrappers for native types. The interal
     * value is the actual value represented by the wrapper objects.
     *
     * @see ECMA 8.6.2
     * @return The internal value of the object
     */
    ValueImp *internalValue() const;

    /**
     * Sets the internal value of the object
     *
     * @see internalValue()
     *
     * @param v The new internal value
     */

    void setInternalValue(ValueImp *v);

    ValueImp *toPrimitive(ExecState *exec, Type preferredType = UnspecifiedType) const;
    bool toBoolean(ExecState *exec) const;
    double toNumber(ExecState *exec) const;
    UString toString(ExecState *exec) const;
    ObjectImp *toObject(ExecState *exec) const;

    // This get method only looks at the property map.
    // A bit like hasProperty(recursive=false), this doesn't go to the prototype.
    // This is used e.g. by lookupOrCreateFunction (to cache a function, we don't want
    // to look up in the prototype, it might already exist there)
    ValueImp *getDirect(const Identifier& propertyName) const
        { return _prop.get(propertyName); }
    ValueImp **getDirectLocation(const Identifier& propertyName)
        { return _prop.getLocation(propertyName); }
    void putDirect(const Identifier &propertyName, ValueImp *value, int attr = 0);
    void putDirect(const Identifier &propertyName, int value, int attr = 0);
    
    void saveProperties(SavedProperties &p) const { _prop.save(p); }
    void restoreProperties(const SavedProperties &p) { _prop.restore(p); }

  protected:
    PropertyMap _prop;
  private:
    const HashEntry* findPropertyHashEntry( const Identifier& propertyName ) const;
    ValueImp *_proto;
    ValueImp *_internalValue;
    ScopeChain _scope;
  };

  // it may seem crazy to inline a function this large but it makes a big difference
  // since this is function very hot in variable lookup
  inline bool ObjectImp::getPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
  {
    ObjectImp *imp = this;

    while (true) {
      if (imp->getOwnPropertySlot(exec, propertyName, slot))
        return true;
      
      ValueImp *proto = imp->_proto;
      if (!proto->isObject())
        break;
      
      imp = static_cast<ObjectImp *>(proto);
    }
    
    return false;
  }

  // it may seem crazy to inline a function this large, especially a virtual function,
  // but it makes a big difference to property lookup if subclasses can inline their
  // superclass call to this
  inline bool ObjectImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
  {
      ValueImp **impLocation = getDirectLocation(propertyName);
      if (impLocation) {
        slot.setValueSlot(this, impLocation);
        return true;
      }

      // non-standard netscape extension
      if (propertyName == exec->dynamicInterpreter()->specialPrototypeIdentifier()) {
        slot.setValueSlot(this, &_proto);
        return true;
      }

      return false;
  }

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

  ObjectImp *error(ExecState *exec, ErrorType type = GeneralError,
    const char *message = 0, int lineno = -1, int sourceId = -1, const UString *sourceURL = 0);

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
    static ObjectImp *create(ExecState *exec, ErrorType errtype = GeneralError,
                             const char *message = 0, int lineno = -1,
                             int sourceId = -1, const UString *sourceURL = 0);

    /**
     * Array of error names corresponding to ErrorType
     */
    static const char * const * const errorNames;
  };

    inline bool AllocatedValueImp::isObject(const ClassInfo *info) const
        { return isObject() && static_cast<const ObjectImp *>(this)->inherits(info); }

inline ObjectImp::ObjectImp(ObjectImp *proto)
    : _proto(proto), _internalValue(0)
{
}

inline ObjectImp::ObjectImp()
    : _proto(jsNull()), _internalValue(0)
{
}

inline ValueImp *ObjectImp::internalValue() const
{
    return _internalValue;
}

inline void ObjectImp::setInternalValue(ValueImp *v)
{
    _internalValue = v;
}

inline ValueImp *ObjectImp::prototype() const
{
    return _proto;
}

inline void ObjectImp::setPrototype(ValueImp *proto)
{
    _proto = proto;
}

inline bool ObjectImp::inherits(const ClassInfo *info) const
{
    for (const ClassInfo *ci = classInfo(); ci; ci = ci->parentClass)
        if (ci == info)
            return true;
    return false;
}

} // namespace

#endif // KJS_OBJECT_H
