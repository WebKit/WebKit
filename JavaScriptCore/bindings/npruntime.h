/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
 *
 * Revision 1 (March 4, 2004):
 * Initial proposal.
 *
 * Revision 2 (March 10, 2004):
 * All calls into JavaScript were made asynchronous.  Results are
 * provided via the NP_JavaScriptResultFunctionPtr callback.
 *
 * Revision 3 (March 10, 2004):
 * Corrected comments to not refer to class retain/release FunctionPtrs.
 *
 * Revision 4 (March 11, 2004):
 * Added additional convenience NPN_SetExceptionWithUTF8().
 * Changed NP_HasPropertyFunctionPtr and NP_HasMethodFunctionPtr to take NPClass
 * pointers instead of NPObject pointers.
 * Added NP_IsValidIdentifier().
 *
 * Revision 5 (March 17, 2004):
 * Added context parameter to result callbacks from JavaScriptObject functions.
 *
 * Revision 6 (March 29, 2004):
 * Renamed functions implemented by user agent to NPN_*.  Removed _ from
 * type names.
 * Renamed "JavaScript" types to "Script".
 *
 */
#ifndef _NP_RUNTIME_H_
#define _NP_RUNTIME_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
    This API is used to facilitate binding code written in 'C' to JavaScript
    objects.  In particular it is used to support the extended Netscape 
    script-ability API for plugins (NP-SAP).  NP-SAP is an extension of the 
    Netscape plugin API.  As such we have adopted the use of the "NP_" prefix
    for this API.  
    
    The following NP-SAP entry points were added to the Netscape plugin API:

    typedef NPObject *(*NPP_GetNativeObjectForJavaScript) (NPP instance);
    typedef NPScriptObject *(*NPN_GetWindowJavaScriptObject) (NPP instance);
    typedef NPScriptObject *(*NPN_GetInstanceJavaScriptObject) (NPP instance);
    
    See NP_SAP.h for more details regarding the above FunctionPtrs.
*/


/*
    Data passed between 'C' and JavaScript is always wrapped in an NPObject.
    The FunctionPtr on an NPObject is described by an NPClass.
*/
typedef struct NPObject NPObject;
typedef struct NPClass NPClass;

/*
    A NPScriptObject wraps a JavaScript Object in an NPObject.
*/
typedef NPObject NPScriptObject;

/*
	Type mappings:

	JavaScript		to			C
	Boolean						NPBoolean	
	Number						NPNumber
	String						NPString
	Undefined					NPUndefined
	Null						NPNull
	Object (including Array)	NPScriptObject
	Object wrapper              NPObject


	C				to			JavaScript
	NPBoolean					Boolean	
	NPNumber					Number
	NPString					String
	NPUndefined				Undefined
	NPNull						Null
	NPArray					Array (restricted)
	NPScriptObject			Object
	other NPObject             Object wrapper

*/

typedef uint32_t NPIdentifier;

/*
    NPUTF8 strings are null terminated.
*/
typedef char NPUTF8;

/*
    NPObjects have methods and properties.  Methods and properties are named with NPIdentifiers.
    These identifiers may be reflected in JavaScript.  NPIdentifiers can be compared using ==.
    
    NP_IsValidIdentifier will return true if an identifier for the name has already been
    assigned with either NPIdentifierFromUTF8() or NP_GetIdentifiers();
*/
NPIdentifier NPN_IdentifierFromUTF8 (const NPUTF8 *name);
bool NPN_IsValidIdentifier (const NPUTF8 *name);
void NPN_GetIdentifiers (const NPUTF8 **names, int nameCount, NPIdentifier *identifiers);

/*
    The returned NPUTF8 should NOT be freed.
*/
const NPUTF8 *NPN_UTF8FromIdentifier (NPIdentifier identifier);

/*
    NPObject behavior is implemented using the following set of callback FunctionPtrs.
*/
typedef NPObject *(*NPAllocateFunctionPtr)();
typedef void (*NPDeallocateFunctionPtr)(NPObject *obj);
typedef void (*NPInvalidateFunctionPtr)();
typedef bool (*NPHasMethodFunctionPtr)(NPClass *theClass, NPIdentifier name);
typedef NPObject *(*NPInvokeFunctionPtr)(NPObject *obj, NPIdentifier name, NPObject **args, unsigned argCount);
typedef bool (*NPHasPropertyFunctionPtr)(NPClass *theClass, NPIdentifier name);
typedef NPObject *(*NPGetPropertyFunctionPtr)(NPObject *obj, NPIdentifier name);
typedef void (*NPSetPropertyFunctionPtr)(NPObject *obj, NPIdentifier name, NPObject *value);

/*
    NPObjects returned by create, retain, invoke, and getProperty 
    pass a reference count to the caller.  That is, the callee adds a reference
    count which passes to the caller.  It is the caller's responsibility
    to release the returned object.

    NPInvokeFunctionPtr FunctionPtrs may return 0 to indicate a void result.
    
    NPInvalidateFunctionPtr is called by the plugin container when the plugin is
    shutdown.  Any attempt to message a NPScriptObject instance after the invalidate
    FunctionPtr has been called will result in undefined behavior, even if the
    plugin is still retaining those NPScriptObject instances.
    (The runtime will typically return immediately, with 0 or NULL, from an attempt to
    dispatch to a NPScriptObject, but this behavior should not be depended upon.)
*/
struct NPClass
{
    uint32_t structVersion;
    NPAllocateFunctionPtr allocate;
    NPDeallocateFunctionPtr deallocate;
    NPInvalidateFunctionPtr invalidate;
    NPHasMethodFunctionPtr hasMethod;
    NPInvokeFunctionPtr invoke;
    NPHasPropertyFunctionPtr hasProperty;
    NPGetPropertyFunctionPtr getProperty;
    NPSetPropertyFunctionPtr setProperty;
};
typedef struct NPClass NPClass;

#define kNPClassStructVersion1 1
#define kNPClassStructVersionCurrent kNPClassStructVersion1

struct NPObject {
    NPClass *_class;
    uint32_t referenceCount;
    // Additional space may be allocated here by types of NPObjects
};

/*
    If the class has an allocate FunctionPtr this function invokes that FunctionPtr,
    otherwise a NPObject is allocated and returned.  If a class has an allocate
    FunctionPtr it is the responsibility of that FunctionPtr to set the initial retain
    count to 1.
*/
NPObject *NPN_CreateObject (NPClass *aClass);

/*
    Increment the NPObject's reference count.
*/
NPObject *NPN_RetainObject (NPObject *obj);

/*
    Decremented the NPObject's reference count.  If the reference
    count goes to zero, the class's destroy FunctionPtr is invoke if
    specified, otherwise the object is free()ed directly.
*/
void NPN_ReleaseObject (NPObject *obj);

/*
    Built-in data types.  These classes can be passed to NPN_IsKindOfClass().
*/
extern NPClass *NPBooleanClass;
extern NPClass *NPNullClass;
extern NPClass *NPUndefinedClass;
extern NPClass *NPArrayClass;
extern NPClass *NPNumberClass;
extern NPClass *NPStringClass;
extern NPClass *NPScriptObjectClass;

typedef NPObject NPBoolean;
typedef NPObject NPNull;
typedef NPObject NPUndefined;
typedef NPObject NPArray;
typedef NPObject NPNumber;
typedef NPObject NPString;

/*
    Functions to access JavaScript Objects represented by NPScriptObject.
    
    Calls to JavaScript objects are asynchronous.  If a function returns a value, it
    will be supplied via the NP_JavaScriptResultFunctionPtr callback.
    
    Calls made from plugin code to JavaScript may be made from any thread.
    
    Calls made from JavaScript to the plugin will always be made on the main
    user agent thread, this include calls to NP_JavaScriptResultFunctionPtr callbacks.
*/
typedef void (*NPScriptResultFunctionPtr)(NPObject *obj, void *resultContext);

void NPN_Call (NPScriptObject *obj, NPIdentifier methodName, NPObject **args, unsigned argCount, NPScriptResultFunctionPtr resultCallback);
void NPN_Evaluate (NPScriptObject *obj, NPString *script, NPScriptResultFunctionPtr resultCallback, void *resultContext);
void NPN_GetProperty (NPScriptObject *obj, NPIdentifier  propertyName, NPScriptResultFunctionPtr resultCallback, void *resultContext);
void NPN_SetProperty (NPScriptObject *obj, NPIdentifier  propertyName, NPObject *value);
void NPN_RemoveProperty (NPScriptObject *obj, NPIdentifier propertyName);
void NPN_ToString (NPScriptObject *obj, NPScriptResultFunctionPtr result, void *resultContext);
void NPN_GetPropertyAtIndex (NPScriptObject *obj, int32_t index, NPScriptResultFunctionPtr resultCallback, void *resultContext);
void NPN_SetPropertyAtIndex (NPScriptObject *obj, unsigned index, NPObject *value);

/*
    Functions for dealing with data types.
*/
NPNumber *NPN_CreateNumberWithInt (int i);
NPNumber *NPN_CreateNumberWithFloat (float f);
NPNumber *NPN_CreateNumberWithDouble (double d);
int NPN_IntFromNumber (NPNumber *obj);
float NPN_FloatFromNumber (NPNumber *obj);
double NPN_DoubleFromNumber (NPNumber *obj);

/*
    NPN_CreateStringWithUTF8() takes a UTF8 string and length.  -1 may be passed for the
    length if the string is null terminated.
*/
NPString *NPN_CreateStringWithUTF8 (const NPUTF8 *utf8String, int32_t length);

/*
    Memory returned from NPUTF8FromString must be deallocated
    by calling NP_DeallocateUTF8.
*/
NPUTF8 *NPN_UTF8FromString (NPString *obj);
void NPN_DeallocateUTF8 (NPUTF8 *UTF8Buffer);
int32_t NPN_StringLength (NPString *obj);

NPBoolean *NPN_CreateBoolean (bool f);
bool NPN_BoolFromBoolean (NPBoolean *aBool);

/*
    NPNull returns a NPNull singleton.
*/
NPNull *NPN_GetNull();

/*
    NPUndefined returns a NPUndefined singleton.
*/
NPUndefined *NPN_GetUndefined ();

/*
    NPArrays are immutable.  They are used to pass arguments to 
    JavaScription FunctionPtrs that expect arrays, or to export 
    arrays of properties.  NPArray is represented in JavaScript
    by a restricted Array.  The Array in JavaScript is read-only,
    only has index accessors, and may not be resized.
    
    Objects added to arrays are retained by the array.
*/
NPArray *NPN_CreateArray (NPObject **, int32_t count);
NPArray *NPN_CreateArrayV (int32_t count, ...);

/*
    Objects returned by NPN_ObjectAtIndex pass a reference count
    to the caller.  The caller must release the object.
*/
NPObject *NPN_ObjectAtIndex (NPArray *array, int32_t index);

/*
    Returns true if the object is a kind of class as specified by
    aClass.
*/
bool NPN_IsKindOfClass (const NPObject *obj, const NPClass *aClass);

/*
    NPN_SetException may be called to trigger a JavaScript exception upon return
    from entry points into NPObjects.  A reference count of the message passes
    to the callee.  Typical usage:

    NPString *message = NP_CreateStringWithUTF8("invalid type");
    NPN_SetException (obj, mesage);
    NP_ReleaseObject (message);
    
    NPN_SetExceptionWithUTF8() take an UTF8 string and a length.  -1 may be passed for
    the length if the string is null terminated.
*/
void NPN_SetExceptionWithUTF8 (NPObject *obj, const NPUTF8 *message, int32_t length);
void NPN_SetException (NPObject *obj, NPString *message);

/*
    Example usage:
    
    typedef NPObject MyFunctionPtrObject;

    typedef struct
    {
        NPObject object;
        // Properties needed by MyFunctionPtrObject are added here.
        int numChapters;
        ...
    } MyFunctionPtrObject

    void stop(MyFunctionPtrObject *obj)
    {
        ...
    }

    void start(MyFunctionPtrObject *obj)
    {
        ...
    }

    void setChapter(MyFunctionPtrObject *obj, int chapter)
    {
        ...
    }

    int getChapter(MyFunctionPtrObject *obj)
    {
        ...
    }

    static NPIdentifier stopIdentifier;
    static NPIdentifier startIdentifier;
    static NPIdentifier setChapterIdentifier;
    static NPIdentifier getChapterIdentifier;
    static NPIdentifier numChaptersIdentifier;

    static void initializeIdentifiers()
    {
        stopIdentifier = NPIdentifierFromUTF8 ("stop");
        startIdentifier = NPIdentifierFromUTF8 ("start");
        setChapterIdentifier = NPIdentifierFromUTF8 ("setChapter");
        getChapterIdentifier = NPIdentifierFromUTF8 ("getChapter");
        numChaptersIdentifier = NPIdentifierFromUTF8 ("numChapters");
    }

    bool myFunctionPtrHasProperty (MyFunctionPtrObject *obj, NPIdentifier name)
    {
        if (name == numChaptersIdentifier){
            return true;
        }
        return false;
    }

    bool myFunctionPtrHasMethod (MyFunctionPtrObject *obj, NPIdentifier name)
    {
        if (name == stopIdentifier ||
            name == startIdentifier ||
            name == setChapterIdentifier ||
            name == getChapterIdentifier) {
            return true;
        }
        return false;
    }

    NPObject *myFunctionPtrGetProperty (MyFunctionPtrObject *obj, NPIdentifier name)
    {
        if (name == numChaptersIdentifier){
            return NPN_CreateNumberWithInt(obj->numChapters); 
        }
        return 0;
    }

    void myFunctionPtrSetProperty (MyFunctionPtrObject *obj, NPIdentifier name, NPObject *value)
    {
        if (name == numChaptersIdentifier){
            obj->numChapters = NPN_IntFromNumber(obj)
        }
    }

    NPObject *myFunctionPtrInvoke (MyFunctionPtrObject *obj, NPIdentifier name, NPObject **args, unsigned argCount)
    {

        if (name == stopIdentifier){
            stop(obj);
        }
        else if (name == startIdentifier){
            start(obj);
        }
        else if (name == setChapterIdentifier){
            if (NPN_IsKindOfClass (args[0], NPNumberClass)) {
                setChapter (obj, NPN_IntFromNumber(args[0]));
            }
            else {
                NPN_SetException (obj, NP_CreateStringWithUTF8 ("invalid type"));
            }
        }
        else if (name == getChapterIdentifier){
            return NPN_CreateNumberWithInt (getChapter (obj));
        }
        return 0;
    }

    NPObject *myFunctionPtrAllocate ()
    {
        MyFunctionPtrObject *newInstance = (MyFunctionPtrObject *)malloc (sizeof(MyFunctionPtrObject));
        
        if (stopIdentifier == 0)
            initializeIdentifiers();
            
        return (NPObject *)newInstance;
    }

    void myFunctionPtrInvalidate ()
    {
        // Make sure we've released any remainging references to JavaScript
        // objects.
    }
    
    void myFunctionPtrDeallocate (MyFunctionPtrObject *obj) 
    {
        free ((void *)obj);
    }
    
    static NPClass _myFunctionPtr = { 
        (NPAllocateFunctionPtr) myFunctionPtrAllocate, 
        (NPDeallocateFunctionPtr) myFunctionPtrDeallocate, 
        (NPInvalidateFunctionPtr) myFunctionPtrInvalidate,
        (NPHasMethodFunctionPtr) myFunctionPtrHasMethod,
        (NPInvokeFunctionPtr) myFunctionPtrInvoke,
        (NPHasPropertyFunctionPtr) myFunctionPtrHasProperty,
        (NPGetPropertyFunctionPtr) myFunctionPtrGetProperty,
        (NPSetPropertyFunctionPtr) myFunctionPtrSetProperty,
    };
    static NPClass *myFunctionPtr = &_myFunctionPtr;

    // myGetNativeObjectForJavaScript would be set as the entry point for
    // the plugin's NPP_GetNativeObjectForJavaScript function.
    // It is invoked by the plugin container, i.e. the browser.
    NPObject *myGetNativeObjectForJavaScript(NPP instance)
    {
        NPObject *myFunctionPtrObject = NPN_CreateObject (myFunctionPtr);
        return myFunctionPtrObject;
    }

*/


#ifdef __cplusplus
}
#endif

#endif
