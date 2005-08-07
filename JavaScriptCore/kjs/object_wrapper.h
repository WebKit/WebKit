
#ifndef _KJS_OBJECT_WRAPPER_H_
#define _KJS_OBJECT_WRAPPER_H_

#include "value.h"
#include "reference_list.h"

namespace KJS {

  class ClassInfo;
  class ObjectImp;
  class ScopeChain;
  class SavedProperties;

  // ECMA 262-3 8.6.1
  // Attributes (only applicable to the Object type)
  enum Attribute { None       = 0,
                   ReadOnly   = 1 << 1, // property can be only read, not written
                   DontEnum   = 1 << 2, // property doesn't appear in (for .. in ..)
                   DontDelete = 1 << 3, // property can't be deleted
                   Internal   = 1 << 4, // an internal property, set to by pass checks
                   Function   = 1 << 5 }; // property is a function - only used by static hashtables

  /**
   * Represents an Object. This is a wrapper for ObjectImp
   */
  class Object : public Value {
  public:
    Object() { }
    Object(ObjectImp *);
    operator ObjectImp *() const { return imp(); }
    
    ObjectImp *imp() const;

    const ClassInfo *classInfo() const;
    bool inherits(const ClassInfo *cinfo) const;

    /**
     * Converts a Value into an Object. If the value's type is not ObjectType,
     * a null object will be returned (i.e. one with it's internal pointer set
     * to 0). If you do not know for sure whether the value is of type
     * ObjectType, you should check the isValid() methods afterwards before
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
    Value get(ExecState *exec, const Identifier &propertyName) const;
    Value get(ExecState *exec, unsigned propertyName) const;

    /**
     * Sets the specified property.
     *
     * See ECMA 8.6.2.2
     *
     * @param exec The current execution state
     * @param propertyName The name of the property to set
     * @param propertyValue The value to set
     */
    void put(ExecState *exec, const Identifier &propertyName,
             const Value &value, int attr = None);
    void put(ExecState *exec, unsigned propertyName,
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
    bool canPut(ExecState *exec, const Identifier &propertyName) const;

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
    bool hasProperty(ExecState *exec, const Identifier &propertyName) const;

    /**
     * Checks to see whether the object has a property with the specified name.
     *
     * See ECMA 15.2.4.5
     *
     * @param exec The current execution state
     * @param propertyName The name of the property to check for
     * @return true if the object has the property, otherwise false
     */
    bool hasOwnProperty(ExecState *exec, const Identifier &propertyName) const;

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
    bool deleteProperty(ExecState *exec, const Identifier &propertyName);
    bool deleteProperty(ExecState *exec, unsigned propertyName);

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
    Object construct(ExecState *exec, const List &args);
    Object construct(ExecState *exec, const List &args, const UString &sourceURL, int lineNumber);

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
     * Checks whether value delegates behavior to this object. Used by the
     * instanceof operator.
     *
     * @param exec The current execution state
     * @param value The value to check
     * @return true if value delegates behavior to this object, otherwise
     * false
     */
    Boolean hasInstance(ExecState *exec, const Value &value);

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
    const ScopeChain &scope() const;
    void setScope(const ScopeChain &s);

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
    ReferenceList propList(ExecState *exec, bool recursive = true);

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

    void saveProperties(SavedProperties &p) const;
    void restoreProperties(const SavedProperties &p);
  };

} // namespace

#endif // _KJS_OBJECT_WRAPPER_H_
