/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Revision 1 (March 4, 2004):
 * Initial proposal.
 *
 * Revision 2 (March 10, 2004):
 * All calls into script were made asynchronous.  Results are
 * provided via the NPScriptResultFunctionPtr callback.
 *
 * Revision 3 (March 10, 2004):
 * Corrected comments to not refer to class retain/release FunctionPtrs.
 *
 * Revision 4 (March 11, 2004):
 * Added additional convenience NPN_SetExceptionWithUTF8().
 * Changed NPHasPropertyFunctionPtr and NPHasMethodFunctionPtr to take NPClass
 * pointers instead of NPObject pointers.
 * Added NPIsValidIdentifier().
 *
 * Revision 5 (March 17, 2004):
 * Added context parameter to result callbacks from ScriptObject functions.
 *
 * Revision 6 (March 29, 2004):
 * Renamed functions implemented by user agent to NPN_*.  Removed _ from
 * type names.
 * Renamed "JavaScript" types to "Script".
 *
 * Revision 7 (April 21, 2004):
 * NPIdentifier becomes a void*, was int32_t
 * Remove NP_IsValidIdentifier, renamed NP_IdentifierFromUTF8 to NP_GetIdentifier
 * Added NPVariant and modified functions to use this new type.
 */
#ifndef _NP_RUNTIME_H_
#define _NP_RUNTIME_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
    This API is used to facilitate binding code written in C to script
    objects.  The API in this header does not assume the presence of a
    user agent.  That is, it can be used to bind C code to scripting environments
    outside of the context of a user agent.  
    
    However, the normal use of the this API is in the context of a scripting
    environment running in a browser or other user agent.  In particular it is used
    to support the extended Netscape script-ability API for plugins (NP-SAP).
    NP-SAP is an extension of the Netscape plugin API.  As such we have adopted the
    use of the "NP" prefix for this API.  
*/


/*
    Data passed between 'C' and script is always wrapped in an NPObject.
    The 'interface' of an NPObject is described by an NPClass.
*/
typedef struct NPObject NPObject;
typedef struct NPClass NPClass;

typedef char NPUTF8;
typedef struct _NPString {
    const NPUTF8 *UTF8Characters;
    uint32_t UTF8Length;
} NPString;

#ifndef _NPAPI_H_
// Ack!  Temporary hack to get build working.
typedef unsigned char NPBool;
#endif
  
typedef enum {
    NPVariantVoidType,
    NPVariantNullType,
    NPVariantUndefinedType,
    NPVariantBoolType,
    NPVariantInt32Type,
    NPVariantDoubleType,
    NPVariantStringType,
    NPVariantObjectType
} NPVariantType;

typedef struct _NPVariant {
    NPVariantType type;
    union {
        NPBool boolValue;
        int32_t intValue;
        double doubleValue;
        NPString stringValue;
        NPObject *objectValue;
    } value;
} NPVariant;

/*
    NPN_ReleaseVariantValue is called on all 'out' parameters references.
    Specifically it is called on variants that are resultant out parameters
    in NPGetPropertyFunctionPtr and NPInvokeFunctionPtr.  Resultant variants
    from these two functions should be initialized using the
    NPN_InitializeVariantXXX() functions.
    
    After calling NPReleaseVariantValue, the type of the variant will
    be set to NPVariantUndefinedType.
*/
void NPN_ReleaseVariantValue (NPVariant *variant);

NPBool NPN_VariantIsVoid (const NPVariant *variant);
NPBool NPN_VariantIsNull (const NPVariant *variant);
NPBool NPN_VariantIsUndefined (const NPVariant *variant);
NPBool NPN_VariantIsBool (const NPVariant *variant);
NPBool NPN_VariantIsInt32 (const NPVariant *variant);
NPBool NPN_VariantIsDouble (const NPVariant *variant);
NPBool NPN_VariantIsString (const NPVariant *variant);
NPBool NPN_VariantIsObject (const NPVariant *variant);
NPBool NPN_VariantToBool (const NPVariant *variant, NPBool *result);
NPBool NPN_VariantToInt32 (const NPVariant *variant, int32_t *result);
NPBool NPN_VariantToDouble (const NPVariant *variant, double *result);
NPString NPN_VariantToString (const NPVariant *variant);
NPString NPN_VariantToStringCopy (const NPVariant *variant);
NPBool NPN_VariantToObject (const NPVariant *variant, NPObject **result);

/*
    NPVariants initialized with the NPN_InitializeVariantXXX() functions
    must be released using the NPN_ReleaseVariantValue() function.
*/
void NPN_InitializeVariantAsVoid (NPVariant *variant);
void NPN_InitializeVariantAsNull (NPVariant *variant);
void NPN_InitializeVariantAsUndefined (NPVariant *variant);
void NPN_InitializeVariantWithBool (NPVariant *variant, NPBool value);
void NPN_InitializeVariantWithInt32 (NPVariant *variant, int32_t value);
void NPN_InitializeVariantWithDouble (NPVariant *variant, double value);

/*
    NPN_InitializeVariantWithString() does not copy string data.  However
    the string data will be deallocated by calls to NPReleaseVariantValue().
*/
void NPN_InitializeVariantWithString (NPVariant *variant, const NPString *value);

/*
    NPN_InitializeVariantWithStringCopy() will copy string data.  The string data
    will be deallocated by calls to NPReleaseVariantValue().
*/
void NPN_InitializeVariantWithStringCopy (NPVariant *variant, const NPString *value);

/*
    NPN_InitializeVariantWithObject() retains the NPObject.  The object will be released
    by calls to NPReleaseVariantValue();
*/
void NPN_InitializeVariantWithObject (NPVariant *variant, NPObject *value);

void NPN_InitializeVariantWithVariant (NPVariant *destination, const NPVariant *source);

/*
	Type mappings (JavaScript types have been used for illustration
    purposes):

	script    to                C
	Boolean                     NPVariant (with type NPVariantBoolType) 
	Number                      NPVariant (with type NPVariantDoubleType)
	String                      NPVariant (with type NPVariantStringType)
	Undefined                   NPVariant (with type NPVariantUndefinedType)
	Null                        NPVariant (with type NPVariantNullType)
	Object (including Array)    NPVariant (with type NPVariantObjectType, objectValue will be a NPObject)
	Object (NPObject wrapper)   NPVariant (with type NPVariantObjectType)


	C          to                                         script
	NPVariant (with type NPVariantBoolType)               Boolean	
	NPVariant (with type NPVariantInt32Type)              Number
	NPVariant (with type NPVariantDoubleType)             Number
	NPVariant (with type NPVariantStringType)             String
	NPVariant (with type NPVariantUndefinedType)          Undefined
	NPVariant (with type NPVariantNullType)               Null
	NPArray                                               Array (restricted)
	NObject                                               Object or (NPObject wrapper)

*/

typedef void *NPIdentifier;

/*
    NPObjects have methods and properties.  Methods and properties are
    identified with NPIdentifiers.  These identifiers may be reflected
    in script.  NPIdentifiers can be either strings or integers, IOW,
    methods and properties can be identified by either strings or
    integers (i.e. foo["bar"] vs foo[1]). NPIdentifiers can be
    compared using ==.  In case of any errors, the requested
    NPIdentifier(s) will be NULL.
*/
NPIdentifier NPN_GetStringIdentifier(const NPUTF8 *name);
void NPN_GetStringIdentifiers(const NPUTF8 **names, int32_t nameCount, NPIdentifier *identifiers);
NPIdentifier NPN_GetIntIdentifier(int32_t intid);
NPBool NPN_IdentifierIsString(NPIdentifier identifier);
NPUTF8 *NPN_UTF8FromIdentifier (NPIdentifier identifier);

/*
    NPObject behavior is implemented using the following set of callback functions.
    
    The NPVariant *result parameter of NPInvokeFunctionPtr and NPGetPropertyFunctionPtr functions
    should be initialized using one of the NPN_InitializeVariantXXX functions.  The variant result
    of the two functions will be released using NPN_ReleaseVariantValue().
*/
typedef NPObject *(*NPAllocateFunctionPtr)();
typedef void (*NPDeallocateFunctionPtr)(NPObject *obj);
typedef void (*NPInvalidateFunctionPtr)(NPObject *obj);
typedef NPBool (*NPHasMethodFunctionPtr)(NPClass *theClass, NPIdentifier name);
typedef NPBool (*NPInvokeFunctionPtr)(NPObject *obj, NPIdentifier name, const NPVariant *args, unsigned argCount, NPVariant *result);
typedef NPBool (*NPHasPropertyFunctionPtr)(NPClass *theClass, NPIdentifier name);
typedef NPBool (*NPGetPropertyFunctionPtr)(NPObject *obj, NPIdentifier name, NPVariant *result);
typedef NPBool (*NPSetPropertyFunctionPtr)(NPObject *obj, NPIdentifier name, const NPVariant *value);

/*
    NPObjects returned by create have a reference count of one.  It is the caller's responsibility
    to release the returned object.

    NPInvokeFunctionPtr function may return false to indicate a the method could not be invoked.
    
    NPGetPropertyFunctionPtr and NPSetPropertyFunctionPtr may return false to indicate a property doesn't
    exist.
    
    NPInvalidateFunctionPtr is called by the scripting environment when the native code is
    shutdown.  Any attempt to message a NPObject instance after the invalidate
    callback has been called will result in undefined behavior, even if the
    native code is still retaining those NPObject instances.
    (The runtime will typically return immediately, with 0 or NULL, from an attempt to
    dispatch to a NPObject, but this behavior should not be depended upon.)
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

#define kNPClassStructVersion1 1
#define kNPClassStructVersionCurrent kNPClassStructVersion1

struct NPObject {
    NPClass *_class;
    uint32_t referenceCount;
    // Additional space may be allocated here by types of NPObjects
};

/*
    If the class has an allocate function, NPN_CreateObject invokes that function,
    otherwise a NPObject is allocated and returned.  If a class has an allocate
    function it is the responsibility of that implementation to set the initial retain
    count to 1.
*/
NPObject *NPN_CreateObject (NPClass *aClass);

/*
    Increment the NPObject's reference count.
*/
NPObject *NPN_RetainObject (NPObject *obj);

/*
    Decremented the NPObject's reference count.  If the reference
    count goes to zero, the class's destroy function is invoke if
    specified, otherwise the object is freed directly.
*/
void NPN_ReleaseObject (NPObject *obj);

/*
    Functions to access script objects represented by NPObject.
    
    Calls to script objects are asynchronous.  If a function returns a value, it
    will be supplied via the NPScriptResultFunctionPtr callback.
    
    Calls made from plugin code to script may be made from any thread.
    
    Calls made from script to the plugin will always be made on the main
    user agent thread, this include calls to NPScriptResultFunctionPtr callbacks.
*/
NPBool NPN_Call (NPObject *obj, NPIdentifier methodName, const NPVariant *args, unsigned argCount, NPVariant *result);
NPBool NPN_Evaluate (NPObject *obj, NPString *script, NPVariant *result);
NPBool NPN_GetProperty (NPObject *obj, NPIdentifier  propertyName, NPVariant *result);
NPBool NPN_SetProperty (NPObject *obj, NPIdentifier  propertyName, const NPVariant *value);
NPBool NPN_RemoveProperty (NPObject *obj, NPIdentifier propertyName);

/*
    NPN_SetException may be called to trigger a script exception upon return
    from entry points into NPObjects.
*/
void NPN_SetException (NPObject *obj, NPString *message);

#ifdef __cplusplus
}
#endif

#endif
