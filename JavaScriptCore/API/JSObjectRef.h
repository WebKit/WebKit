// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef JSObjectRef_h
#define JSObjectRef_h

#include "JSBase.h"
#include "JSValueRef.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
@enum JSPropertyAttribute
@constant kJSPropertyAttributeNone         Specifies that a property has no special attributes.
@constant kJSPropertyAttributeReadOnly     Specifies that a property is read-only.
@constant kJSPropertyAttributeDontEnum     Specifies that a property should not be enumerated by JSPropertyEnumerators and JavaScript for...in loops.
@constant kJSPropertyAttributeDontDelete   Specifies that the delete operation should fail on a property.
*/
enum { 
    kJSPropertyAttributeNone         = 0,
    kJSPropertyAttributeReadOnly     = 1 << 1,
    kJSPropertyAttributeDontEnum     = 1 << 2,
    kJSPropertyAttributeDontDelete   = 1 << 3
};

/*! 
@typedef JSPropertyAttributes
@abstract A set of JSPropertyAttributes. Combine multiple attributes by logically ORing them together.
*/
typedef unsigned JSPropertyAttributes;

/*! 
@typedef JSInitializeCallback
@abstract The callback invoked when an object is first created.
@param context The execution context to use.
@param object The JSObject being created.
@discussion If you named your function Initialize, you would declare it like this:

void Initialize(JSContextRef context, JSObjectRef object);
*/
typedef void
(*JSInitializeCallback)         (JSContextRef context, JSObjectRef object);

/*! 
@typedef JSFinalizeCallback
@abstract The callback invoked when an object is finalized (prepared for garbage collection).
@param object The JSObject being finalized.
@discussion If you named your function Finalize, you would declare it like this:

void Finalize(JSObjectRef object);
*/
typedef void            
(*JSFinalizeCallback)           (JSObjectRef object);

/*! 
@typedef JSHasPropertyCallback
@abstract The callback invoked when determining whether an object has a given property.
@param context The current execution context.
@param object The JSObject to search for the property.
@param propertyName A JSInternalString containing the name of the property look up.
@result true if object has the property, otherwise false.
@discussion If you named your function HasProperty, you would declare it like this:

bool HasProperty(JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName);

This callback enables optimization in cases where only a property's existence needs to be known, not its value, and computing its value would be expensive. If this callback is NULL, the getProperty callback will be used to service hasProperty calls.
*/
typedef bool
(*JSHasPropertyCallback)        (JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName);

/*! 
@typedef JSGetPropertyCallback
@abstract The callback invoked when getting a property from an object.
@param context The current execution context.
@param object The JSObject to search for the property.
@param propertyName A JSInternalString containing the name of the property to get.
@param returnValue A pointer to a JSValue in which to store the property's value.
@result true if object has the property in question, otherwise false. If this function returns true, returnValue is assumed to contain a valid JSValue.
@discussion If you named your function GetProperty, you would declare it like this:

bool GetProperty(JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName, JSValueRef* returnValue);

If this function returns false, the get request forwards to object's static property table, then its parent class chain (which includes the default object class), then its prototype chain.
*/
typedef bool
(*JSGetPropertyCallback)        (JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName, JSValueRef* returnValue);

/*! 
@typedef JSSetPropertyCallback
@abstract The callback invoked when setting the value of a given property.
@param context The current execution context.
@param object The JSObject on which to set the property's value.
@param propertyName A JSInternalString containing the name of the property to set.
@param value A JSValue to use as the property's value.
@result true if the property was successfully set, otherwise false.
@discussion If you named your function SetProperty, you would declare it like this:

bool SetProperty(JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName, JSValueRef value);

If this function returns false, the set request forwards to object's static property table, then its parent class chain (which includes the default object class).
*/
typedef bool
(*JSSetPropertyCallback)        (JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName, JSValueRef value);

/*! 
@typedef JSDeletePropertyCallback
@abstract The callback invoked when deleting a given property.
@param context The current execution context.
@param object The JSObject in which to delete the property.
@param propertyName A JSInternalString containing the name of the property to delete.
@result true if propertyName was successfully deleted, otherwise false.
@discussion If you named your function DeleteProperty, you would declare it like this:

bool DeleteProperty(JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName);

If this function returns false, the delete request forwards to object's static property table, then its parent class chain (which includes the default object class).
*/
typedef bool
(*JSDeletePropertyCallback)     (JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName);

/*! 
@typedef JSGetPropertyListCallback
@abstract The callback invoked when adding an object's properties to a property list.
@param context The current execution context.
@param object The JSObject whose properties need to be added to propertyList.
@param propertyList A JavaScript property list that will be used to enumerate object's properties.
@discussion If you named your function GetPropertyList, you would declare it like this:

void GetPropertyList(JSContextRef context, JSObjectRef object, JSPropertyListRef propertyList);

Use JSPropertyListAdd to add properties to propertyList.

Property lists are used by JSPropertyEnumerators and JavaScript for...in loops.
*/
typedef void
(*JSGetPropertyListCallback)    (JSContextRef context, JSObjectRef object, JSPropertyListRef propertyList);

/*! 
@typedef JSCallAsFunctionCallback
@abstract The callback invoked when an object is called as a function.
@param context The current execution context.
@param function A JSObject that is the function being called.
@param thisObject A JSObject that is the 'this' variable in the function's scope.
@param argc An integer count of the number of arguments in argv.
@param argv A JSValue array of the  arguments passed to the function.
@result A JSValue that is the function's return value.
@discussion If you named your function CallAsFunction, you would declare it like this:

JSValueRef CallAsFunction(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argc, JSValueRef argv[]);

If your callback were invoked by the JavaScript expression 'myObject.myMemberFunction()', function would be set to myMemberFunction, and thisObject would be set to myObject.

If this callback is NULL, calling your object as a function will throw an exception.
*/
typedef JSValueRef 
(*JSCallAsFunctionCallback)     (JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argc, JSValueRef argv[]);

/*! 
@typedef JSCallAsConstructorCallback
@abstract The callback invoked when an object is used as a constructor in a 'new' statement.
@param context The current execution context.
@param constructor A JSObject that is the constructor being called.
@param argc An integer count of the number of arguments in argv.
@param argv A JSValue array of the  arguments passed to the function.
@result A JSObject that is the constructor's return value.
@discussion If you named your function CallAsConstructor, you would declare it like this:

JSObjectRef CallAsConstructor(JSContextRef context, JSObjectRef constructor, size_t argc, JSValueRef argv[]);

If your callback were invoked by the JavaScript expression 'new myConstructorFunction()', constructor would be set to myConstructorFunction.

If this callback is NULL, using your object as a constructor in a 'new' statement will throw an exception.
*/
typedef JSObjectRef 
(*JSCallAsConstructorCallback)  (JSContextRef context, JSObjectRef constructor, size_t argc, JSValueRef argv[]);

/*! 
@typedef JSConvertToTypeCallback
@abstract The callback invoked when converting an object to a particular JavaScript type.
@param context The current execution context.
@param object The JSObject to convert.
@param typeCode A JSTypeCode specifying the JavaScript type to convert to.
@param returnValue A pointer to a JSValue in which to store the converted value.
@result true if the value was converted, otherwise false. If this function returns true, returnValue is assumed to contain a valid JSValue.
@discussion If you named your function ConvertToType, you would declare it like this:

bool ConvertToType(JSContextRef context, JSObjectRef object, JSTypeCode typeCode, JSValueRef* returnValue);

If this function returns false, the conversion request forwards to object's parent class chain (which includes the default object class).
*/
typedef bool
(*JSConvertToTypeCallback)      (JSContextRef context, JSObjectRef object, JSTypeCode typeCode, JSValueRef* returnValue);

/*!
@struct JSObjectCallbacks
@abstract This structure contains optional callbacks for supplementing default object behavior. Any callback field can be NULL.
@field version The version number of this structure. The current version is 0.
@field initialize The callback invoked when an object is first created. Use this callback in conjunction with JSObjectSetPrivate to initialize private data in your object.
@field finalize The callback invoked when an object is finalized (prepared for garbage collection). Use this callback to release resources allocated for your object, and perform other cleanup.
@field hasProperty The callback invoked when determining whether an object has a given property. If this field is NULL, getProperty will be called instead. The hasProperty callback enables optimization in cases where only a property's existence needs to be known, not its value, and computing its value would be expensive. 
@field getProperty The callback invoked when getting the value of a given property.
@field setProperty The callback invoked when setting the value of a given property.
@field deleteProperty The callback invoked when deleting a given property.
@field getPropertyList The callback invoked when adding an object's properties to a property list.
@field callAsFunction The callback invoked when an object is called as a function.
@field callAsConstructor The callback invoked when an object is used as a constructor in a 'new' statement.
@field convertToType The callback invoked when converting an object to a particular JavaScript type.
*/
typedef struct {
    int                         version; // current (and only) version is 0
    JSInitializeCallback        initialize;
    JSFinalizeCallback          finalize;
    JSHasPropertyCallback       hasProperty;
    JSGetPropertyCallback       getProperty;
    JSSetPropertyCallback       setProperty;
    JSDeletePropertyCallback    deleteProperty;
    JSGetPropertyListCallback   getPropertyList;
    JSCallAsFunctionCallback    callAsFunction;
    JSCallAsConstructorCallback callAsConstructor;
    JSConvertToTypeCallback     convertToType;
} JSObjectCallbacks;

/*! 
@const kJSObjectCallbacksNone 
@abstract A JSObjectCallbacks structure of the current version, filled with NULL callbacks.
@discussion Use this constant as a convenience when creating callback structures. For example, to create a callback structure that has only a finalize method:

JSObjectCallbacks callbacks = kJSObjectCallbacksNone;

callbacks.finalize = Finalize;
*/
extern const JSObjectCallbacks kJSObjectCallbacksNone;

/*! 
@struct JSStaticValue
@abstract This structure describes a static value property.
@field name A UTF8 buffer containing the property's name.
@field getProperty A JSGetPropertyCallback to invoke when getting the property's value.
@field setProperty A JSSetPropertyCallback to invoke when setting the property's value.
@field attributes A logically ORed set of JSPropertyAttributes to give to the property.
*/
typedef struct {
    const char* const name; // FIXME: convert UTF8
    JSGetPropertyCallback getProperty;
    JSSetPropertyCallback setProperty;
    JSPropertyAttributes attributes;
} JSStaticValue;

/*! 
@struct JSStaticFunction
@abstract This structure describes a static function property.
@field name A UTF8 buffer containing the property's name.
@field callAsFunction A JSCallAsFunctionCallback to invoke when the property is called as a function.
@field attributes A logically ORed set of JSPropertyAttributes to give to the property.
*/
typedef struct {
    const char* const name; // FIXME: convert UTF8
    JSCallAsFunctionCallback callAsFunction;
    JSPropertyAttributes attributes;
} JSStaticFunction;

/*!
@function
@abstract Creates a JavaScript class suitable for use with JSObjectMake. Ownership follows the create rule.
@param staticValues A JSStaticValue array representing the class's static value properties. Pass NULL to specify no static value properties. The array must be terminated by a JSStaticValue whose name field is NULL.
@param staticFunctions A JSStaticFunction array representing the class's static function properties. Pass NULL to specify no static function properties. The array must be terminated by a JSStaticFunction whose name field is NULL.
@param callbacks A pointer to a JSObjectCallbacks structure holding custom callbacks for supplementing default object behavior. Pass NULL to specify no custom behavior.
@param parentClass A JSClass to set as the class's parent class. Pass NULL use the default object class.
@discussion The simplest and most efficient way to add custom properties to a class is by specifying static values and functions. Standard JavaScript practice calls for functions to be placed in prototype objects, so that they can be shared among objects.
*/
JSClassRef JSClassCreate(JSStaticValue* staticValues, JSStaticFunction* staticFunctions, const JSObjectCallbacks* callbacks, JSClassRef parentClass);
/*!
@function
@abstract Retains a JavaScript class.
@param jsClass The JSClass to retain.
@result A JSClass that is the same as jsClass.
*/
JSClassRef JSClassRetain(JSClassRef jsClass);
/*!
@function
@abstract Releases a JavaScript class.
@param jsClass The JSClass to release.
*/
void JSClassRelease(JSClassRef jsClass);

/*!
@function
@abstract Creates a JavaScript object with a given class and prototype.
@param context The execution context to use.
@param jsClass The JSClass to assign to the object. Pass NULL to use the default object class.
@param prototype The prototype to assign to the object. Pass NULL to use the default object prototype.
@result A JSObject with the given class and prototype.
*/
JSObjectRef JSObjectMake(JSContextRef context, JSClassRef jsClass, JSObjectRef prototype);

/*!
@function
@abstract Convenience method for creating a JavaScript function with a given callback as its implementation.
@param context The execution context to use.
@param callAsFunction The JSCallAsFunctionCallback to invoke when the function is called.
@result A JSObject that is an anonymous function. The object's prototype will be the default function prototype.
*/
JSObjectRef JSFunctionMake(JSContextRef context, JSCallAsFunctionCallback callAsFunction);
/*!
@function
@abstract Convenience method for creating a JavaScript constructor with a given callback as its implementation.
@param context The execution context to use.
@param callAsConstructor The JSCallAsConstructorCallback to invoke when the constructor is used in a 'new' statement.
@result A JSObject that is a constructor. The object's prototype will be the default object prototype.
*/
JSObjectRef JSConstructorMake(JSContextRef context, JSCallAsConstructorCallback callAsConstructor);

/*!
@function
@abstract Creates a function with a given script as its body.
@param context The execution context to use.
@param body A JSInternalString containing the script to use as the function's body.
@param sourceURL A JSInternalString containing a URL for the script's source file. This is only used when reporting exceptions. Pass NULL if you do not care to include source file information in exceptions.
@param startingLineNumber An integer value specifying the script's starting line number in the file located at sourceURL. This is only used when reporting exceptions.
@param exception A pointer to a JSValueRef in which to store a syntax error exception, if any. Pass NULL if you do not care to store a syntax error exception.
@result A JSObject that is an anonymous function, or NULL if body contains a syntax error. The returned object's prototype will be the default function prototype.
@discussion Use this method when you want to execute a script repeatedly, to avoid the cost of re-parsing the script before each execution.
*/
JSObjectRef JSFunctionMakeWithBody(JSContextRef context, JSInternalStringRef body, JSInternalStringRef sourceURL, int startingLineNumber, JSValueRef* exception);

/*!
@function
@abstract Gets a short description of a JavaScript object.
@param context The execution context to use.
@param object The object whose description you want to get.
@result A JSInternalString containing the object's description. This is usually the object's class name.
*/
JSInternalStringRef JSObjectGetDescription(JSObjectRef object);

/*!
@function
@abstract Gets an object's prototype.
@param object A JSObject whose prototype you want to get.
@result A JSValue containing the object's prototype.
*/
JSValueRef JSObjectGetPrototype(JSObjectRef object);
/*!
@function
@abstract Sets an object's prototype.
@param object The JSObject whose prototype you want to set.
@param value A JSValue to set as the object's prototype.
*/
void JSObjectSetPrototype(JSObjectRef object, JSValueRef value);

/*!
@function
@abstract Tests whether an object has a certain property.
@param object The JSObject to test.
@param propertyName A JSInternalString containing the property's name.
@result true if the object has a property whose name matches propertyName, otherwise false.
*/
bool JSObjectHasProperty(JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName);
/*!
@function
@abstract Gets a property from an object.
@param context The execution context to use.
@param object The JSObject whose property you want to get.
@param propertyName A JSInternalString containing the property's name.
@result The property's value, or NULL if the object does not have a property whose name matches propertyName.
*/
JSValueRef JSObjectGetProperty(JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName);
/*!
@function
@abstract Sets a property on an object.
@param context The execution context to use.
@param object The JSObject whose property you want to set.
@param propertyName A JSInternalString containing the property's name.
@param value A JSValue to use as the property's value.
@param attributes A logically ORed set of JSPropertyAttributes to give to the property.
@result true if the set operation succeeds, otherwise false (for example, if the object already has a property of the given name with the kJSPropertyAttributeReadOnly attribute set).
*/
bool JSObjectSetProperty(JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName, JSValueRef value, JSPropertyAttributes attributes);
/*!
@function
@abstract Deletes a property from an object.
@param context The execution context to use.
@param object The JSObject whose property you want to delete.
@param propertyName A JSInternalString containing the property's name.
@result true if the delete operation succeeds, otherwise false (for example, if the property has the kJSPropertyAttributeDontDelete attribute set).
*/
bool JSObjectDeleteProperty(JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName);

/*!
@function
@abstract Gets a pointer to private data from an object.
@param object A JSObject whose private data you want to get.
@result A void* that points to the object's private data, or NULL if the object has no private data.
@discussion JSObjectGetPrivate and JSObjectSetPrivate only work on custom objects created by JSObjectMake, JSFunctionMake, and JSConstructorMake.
*/
void* JSObjectGetPrivate(JSObjectRef object);
/*!
@function
@abstract Sets a pointer to private data on an object.
@param object A JSObject whose private data you want to set.
@param data A void* that points to the object's private data.
@result true if the set operation succeeds, otherwise false.
@discussion JSObjectGetPrivate and JSObjectSetPrivate only work on custom objects created by JSObjectMake, JSFunctionMake, and JSConstructorMake.
*/
bool JSObjectSetPrivate(JSObjectRef object, void* data);

/*!
@function
@abstract Tests whether an object is a function.
@param object The JSObject to test.
@result true if the object is a function, otherwise false.
*/
bool JSObjectIsFunction(JSObjectRef object);
/*!
@function
@abstract Calls an object as a function.
@param context The execution context to use.
@param object The JSObject to call as a function.
@param thisObject The object to use as "this," or NULL to use the global object as "this."
@param argc An integer count of the number of arguments in argv.
@param argv A JSValue array of the  arguments to pass to the function.
@param exception A pointer to a JSValueRef in which to store an uncaught exception, if any. Pass NULL if you do not care to store an uncaught exception.
@result The JSValue that results from calling object as a function, or NULL if an uncaught exception is thrown or object is not a function.
*/
JSValueRef JSObjectCallAsFunction(JSContextRef context, JSObjectRef object, JSObjectRef thisObject, size_t argc, JSValueRef argv[], JSValueRef* exception);
/*!
@function
@abstract Tests whether an object is a constructor.
@param object The JSObject to test.
@result true if the object is a constructor, otherwise false.
*/
bool JSObjectIsConstructor(JSObjectRef object);
/*!
@function
@abstract Calls an object as a constructor.
@param context The execution context to use.
@param object The JSObject to call as a constructor.
@param argc An integer count of the number of arguments in argv.
@param argv A JSValue array of the  arguments to pass to the function.
@param exception A pointer to a JSValueRef in which to store an uncaught exception, if any. Pass NULL if you do not care to store an uncaught exception.
@result The JSObject that results from calling object as a constructor, or NULL if an uncaught exception is thrown or object is not a constructor.
*/
JSObjectRef JSObjectCallAsConstructor(JSContextRef context, JSObjectRef object, size_t argc, JSValueRef argv[], JSValueRef* exception);

/*!
@function
@abstract Creates an enumerator for an object's properties.
@param context The execution context to use.
@param object The object whose properties you want to enumerate.
@result A JSPropertyEnumerator with a list of object's properties. Ownership follows the create rule.
*/
JSPropertyEnumeratorRef JSObjectCreatePropertyEnumerator(JSContextRef context, JSObjectRef object);
/*!
@function
@abstract Retains a property enumerator.
@param enumerator The JSPropertyEnumerator to retain.
@result A JSPropertyEnumerator that is the same as enumerator.
*/
JSPropertyEnumeratorRef JSPropertyEnumeratorRetain(JSPropertyEnumeratorRef enumerator);
/*!
@function
@abstract Releases a property enumerator.
@param enumerator The JSPropertyEnumerator to release.
*/
void JSPropertyEnumeratorRelease(JSPropertyEnumeratorRef enumerator);
/*!
@function
@abstract Gets a property enumerator's next property.
@param enumerator The JSPropertyEnumerator whose next property you want to get.
@result A JSInternalString containing the property's name, or NULL if all properties have been enumerated.
*/
JSInternalStringRef JSPropertyEnumeratorGetNext(JSPropertyEnumeratorRef enumerator);

/*!
@function
@abstract Adds a property to a property list.
@discussion Use this method inside a JSGetPropertyListCallback to add a custom property to an object's property list.
@param propertyList The JSPropertyList to which you want to add a property.
@param thisObject The JSObject to which the property belongs.
@param propertyName A JSInternalString specifying the property's name.
*/
void JSPropertyListAdd(JSPropertyListRef propertyList, JSObjectRef thisObject, JSInternalStringRef propertyName);

#ifdef __cplusplus
}
#endif

#endif // JSObjectRef_h
