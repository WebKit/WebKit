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


#ifndef _KJS_OBJECT_H_
#define _KJS_OBJECT_H_

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

#include "types.h"
#include "interpreter.h"
#include "reference_list.h"
#include "property_map.h"
#include "property_slot.h"
#include "scope_chain.h"

namespace KJS {

  class HashTable;
  class HashEntry;
  class ListImp;

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
  
  inline Object Value::toObject(ExecState *exec) const { return rep->dispatchToObject(exec); }
  
  class ObjectImp : public ValueImp {
  public:
    /**
     * Creates a new ObjectImp with the specified prototype
     *
     * @param proto The prototype
     */
    ObjectImp(const Object &proto);
    ObjectImp(ObjectImp *proto);

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
     * You should either implement classInfo(), or
     * if you simply need a classname, you can reimplement className()
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
    Value get(ExecState *exec, const Identifier &propertyName) const;
    Value get(ExecState *exec, unsigned propertyName) const;

    bool getProperty(ExecState *exec, const Identifier& propertyName, Value& result) const;
    bool getProperty(ExecState *exec, unsigned propertyName, Value& result) const;

    bool getPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    bool getPropertySlot(ExecState *, unsigned, PropertySlot&);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual bool getOwnPropertySlot(ExecState *, unsigned index, PropertySlot&);

    /**
     * Implementation of the [[Put]] internal property (implemented by all
     * Objects)
     *
     * @see Object::put()
     */
    virtual void put(ExecState *exec, const Identifier &propertyName,
                     const Value &value, int attr = None);
    virtual void put(ExecState *exec, unsigned propertyName,
                     const Value &value, int attr = None);

    /**
     * Implementation of the [[CanPut]] internal property (implemented by all
     * Objects)
     *
     * @see Object::canPut()
     */
    virtual bool canPut(ExecState *exec, const Identifier &propertyName) const;

    /**
     * Implementation of the [[HasProperty]] internal property (implemented by
     * all Objects)
     *
     * @see Object::hasProperty()
     */
    bool hasProperty(ExecState *exec, const Identifier &propertyName) const;
    bool hasProperty(ExecState *exec, unsigned propertyName) const;

    virtual bool hasOwnProperty(ExecState *exec, const Identifier &propertyName) const;

    /**
     * Implementation of the [[Delete]] internal property (implemented by all
     * Objects)
     *
     * @see Object::deleteProperty()
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
    virtual Object construct(ExecState *exec, const List &args, const UString &sourceURL, int lineNumber);

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
    const ScopeChain &scope() const { return _scope; }
    void setScope(const ScopeChain &s) { _scope = s; }

    virtual ReferenceList propList(ExecState *exec, bool recursive = true);

    Value internalValue() const;
    void setInternalValue(const Value &v);
    void setInternalValue(ValueImp *v);

    Value toPrimitive(ExecState *exec,
                      Type preferredType = UnspecifiedType) const;
    bool toBoolean(ExecState *exec) const;
    double toNumber(ExecState *exec) const;
    UString toString(ExecState *exec) const;
    Object toObject(ExecState *exec) const;

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
      if (proto->dispatchType() != ObjectType)
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
    static Object create(ExecState *exec, ErrorType errtype = GeneralError,
                         const char *message = 0, int lineno = -1,
                         int sourceId = -1, const UString *sourceURL = 0);

    /**
     * Array of error names corresponding to ErrorType
     */
    static const char * const * const errorNames;
  };

  inline bool ValueImp::isObject(const ClassInfo *info) const
    { return isObject() && static_cast<const ObjectImp *>(this)->inherits(info); }

  inline ObjectImp *ValueImp::asObject()
    { return isObject() ? static_cast<ObjectImp *>(this) : 0; }

  inline Object::Object(ObjectImp *o) : Value(o) { }

  inline ObjectImp *Object::imp() const
    { return static_cast<ObjectImp *>(Value::imp()); }

  inline const ClassInfo *Object::classInfo() const
    { return imp()->classInfo(); }

  inline bool Object::inherits(const ClassInfo *cinfo) const
    { return imp()->inherits(cinfo); }

  inline Value Object::prototype() const
    { return Value(imp()->prototype()); }

  inline UString Object::className() const
    { return imp()->className(); }

  inline Value Object::get(ExecState *exec, const Identifier &propertyName) const
    { return imp()->get(exec,propertyName); }

  inline Value Object::get(ExecState *exec, unsigned propertyName) const
    { return imp()->get(exec,propertyName); }

  inline void Object::put(ExecState *exec, const Identifier &propertyName, const Value &value, int attr)
    { imp()->put(exec,propertyName,value,attr); }

  inline void Object::put(ExecState *exec, unsigned propertyName, const Value &value, int attr)
    { imp()->put(exec,propertyName,value,attr); }

  inline bool Object::canPut(ExecState *exec, const Identifier &propertyName) const
    { return imp()->canPut(exec,propertyName); }

  inline bool Object::hasProperty(ExecState *exec, const Identifier &propertyName) const
    { return imp()->hasProperty(exec, propertyName); }

  inline bool Object::hasOwnProperty(ExecState *exec, const Identifier &propertyName) const
    { return imp()->hasOwnProperty(exec, propertyName); }

  inline bool Object::deleteProperty(ExecState *exec, const Identifier &propertyName)
    { return imp()->deleteProperty(exec,propertyName); }

  inline bool Object::deleteProperty(ExecState *exec, unsigned propertyName)
    { return imp()->deleteProperty(exec,propertyName); }

  inline Value Object::defaultValue(ExecState *exec, Type hint) const
    { return imp()->defaultValue(exec,hint); }

  inline bool Object::implementsConstruct() const
    { return imp()->implementsConstruct(); }

  inline Object Object::construct(ExecState *exec, const List &args)
    { return imp()->construct(exec,args); }
  
  inline Object Object::construct(ExecState *exec, const List &args, const UString &sourceURL, int lineNumber)
  { return imp()->construct(exec,args,sourceURL,lineNumber); }

  inline bool Object::implementsCall() const
    { return imp()->implementsCall(); }

  inline bool Object::implementsHasInstance() const
    { return imp()->implementsHasInstance(); }

  inline Boolean Object::hasInstance(ExecState *exec, const Value &value)
    { return imp()->hasInstance(exec,value); }

  inline const ScopeChain &Object::scope() const
    { return imp()->scope(); }

  inline void Object::setScope(const ScopeChain &s)
    { imp()->setScope(s); }

  inline ReferenceList Object::propList(ExecState *exec, bool recursive)
    { return imp()->propList(exec,recursive); }

  inline Value Object::internalValue() const
    { return imp()->internalValue(); }

  inline void Object::setInternalValue(const Value &v)
    { imp()->setInternalValue(v); }

  inline void Object::saveProperties(SavedProperties &p) const
    { imp()->saveProperties(p); }
  inline void Object::restoreProperties(const SavedProperties &p)
    { imp()->restoreProperties(p); }

} // namespace

#endif // _KJS_OBJECT_H_
