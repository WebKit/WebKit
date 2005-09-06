/*
	JSGlue.cpp
*/

#include "JavaScriptGlue.h"
#include "JSUtils.h"
#include "JSBase.h"
#include "JSObject.h"
#include "JSRun.h"

static CFTypeRef sJSCFNullRef = NULL;

static void CFJSObjectDispose(void* data);
static JSObjectRef CFJSObjectCopyProperty(void* data, CFStringRef propertyName);
static void CFJSObjectSetProperty(void* data, CFStringRef propertyName, JSObjectRef jsValue);
static CFTypeRef CFJSObjectCopyCFValue(void* data);
static UInt8 CFJSObjectEqual(void* data1, void* data2);
static CFArrayRef CFJSObjectCopyPropertyNames(void* data);

void* JSCFRetain(CFAllocatorRef allocator, const void *value);
void JSCFRelease(CFAllocatorRef allocator, const void *value);


void JSSetCFNull(CFTypeRef nullRef)
{
	ReleaseCFType(sJSCFNullRef);
	sJSCFNullRef = RetainCFType(nullRef);
}

CFTypeRef JSGetCFNull(void)
{
	return sJSCFNullRef;
}

/*
	JSRetain
*/
JSTypeRef JSRetain(JSTypeRef ref)
{
	if (ref)
	{
		JSBase* ptr = (JSBase*)ref;
		ptr->Retain();
	}
	return ref;
}

/*
	JSRelease
*/
void JSRelease(JSTypeRef ref)
{
	if (ref)
	{
		JSBase* ptr = (JSBase*)ref;
		ptr->Release();
	}
}

/*
	JSCopyDescription
*/
CFStringRef	JSCopyDescription(JSTypeRef ref)
{
	CFStringRef result = NULL;
	if (ref)
	{
		JSBase* ptr = (JSBase*)ref;
		ptr->CopyDescription();
	}
	return result;
}

/*
	JSEqual
*/
UInt8	JSEqual(JSTypeRef ref1, JSTypeRef ref2)
{
	UInt8 result = false;
	if (ref1 && ref2)
	{
		JSBase* ptr = (JSBase*)ref1;
		result = ptr->Equal((JSBase*)ref2);
	}
	return result;
}


/*
	JSGetTypeID
*/
JSTypeID JSGetTypeID(JSTypeRef ref)
{
	JSTypeID result = kJSInvalidTypeID;
	if (ref)
	{
		JSBase* ptr = (JSBase*)ref;
		result = ptr->GetTypeID();
	}
	return result;
}


/*
	JSGetRetainCount
*/
CFIndex JSGetRetainCount(JSTypeRef ref)
{
	CFIndex result = -1;
	if (ref)
	{
		JSBase* ptr = (JSBase*)ref;
		result = ptr->RetainCount();
	}
	return result;
}



/*
	JSObjectCreate
*/
JSObjectRef JSObjectCreate(void* data, JSObjectCallBacksPtr callBacks)
{
	JSObjectRef result = JSObjectCreateInternal(data, callBacks, NULL, kJSUserObjectDataTypeUnknown);
	return result;
}

/*
	JSObjectCreateInternal
*/
JSObjectRef JSObjectCreateInternal(void* data, JSObjectCallBacksPtr callBacks, JSObjectMarkProcPtr markProc, int type)
{
	JSObjectRef result = NULL;
	JSUserObject* ptr = new	JSUserObject(callBacks, markProc, data, type);
	result = (JSObjectRef)ptr;
	return result;
}

/*
	JSObjectCopyCFValue
*/
CFTypeRef JSObjectCopyCFValue(JSObjectRef ref)
{
	CFTypeRef result = NULL;
	JSUserObject* ptr = (JSUserObject*)ref;
	if (ptr && (ptr->GetTypeID() == kJSObjectTypeID))
	{
		result = ptr->CopyCFValue();
	}
	return result;
}

/*
	JSObjectGetData
*/
void* JSObjectGetData(JSObjectRef ref)
{
	void* result = NULL;
	JSUserObject* ptr = (JSUserObject*)ref;
	if (ptr && (ptr->GetTypeID() == kJSObjectTypeID))
	{
		result = ptr->GetData();
	}
	return result;
}


/*
	JSObjectCopyProperty
*/
JSObjectRef JSObjectCopyProperty(JSObjectRef ref, CFStringRef propertyName)
{
	JSObjectRef result = NULL;
	JSUserObject* ptr = (JSUserObject*)ref;
	if (ptr && (ptr->GetTypeID() == kJSObjectTypeID))
	{
		result = (JSObjectRef)ptr->CopyProperty(propertyName);
	}
	return result;
}


/*
	JSObjectSetProperty
*/
void JSObjectSetProperty(JSObjectRef ref, CFStringRef propertyName, JSObjectRef value)
{
	JSUserObject* ptr = (JSUserObject*)ref;
	if (ptr && (ptr->GetTypeID() == kJSObjectTypeID))
	{
		ptr->SetProperty(propertyName, (JSUserObject*)value);
	}
}


/*
	JSObjectCallFunction
*/
JSObjectRef JSObjectCallFunction(JSObjectRef ref, JSObjectRef thisObj, CFArrayRef args)
{
	JSObjectRef result = NULL;
	JSUserObject* ptr = (JSUserObject*)ref;
	if (ptr && (ptr->GetTypeID() == kJSObjectTypeID))
	{
		result = (JSObjectRef)ptr->CallFunction((JSUserObject*)thisObj, args);
	}
	return result;
}


/*
	JSRunCreate
*/
JSRunRef JSRunCreate(CFStringRef jsSource, JSFlags inFlags)
{
	JSRunRef result = NULL;
	if (jsSource)
	{
		result = (JSRunRef) new JSRun(jsSource, inFlags);
	}
	return result;
}

/*
	JSRunCopySource
*/
CFStringRef JSRunCopySource(JSRunRef ref)
{
	CFStringRef result = NULL;
	JSRun* ptr = (JSRun*)ref;
	if (ptr)
	{
		result = UStringToCFString(ptr->GetSource());
	}
	return result;
}


/*
	JSRunCopyGlobalObject
*/
JSObjectRef JSRunCopyGlobalObject(JSRunRef ref)
{
	JSObjectRef result = NULL;
	JSRun* ptr = (JSRun*)ref;
	if (ptr)
	{
		Object globalObject = ptr->GlobalObject();
		result = (JSObjectRef)KJSValueToJSObject(globalObject, ptr->GetInterpreter()->globalExec());
	}
	return result;
}

/*
	JSRunEvaluate
*/
JSObjectRef JSRunEvaluate(JSRunRef ref)
{
	JSObjectRef result = NULL;
	JSRun* ptr = (JSRun*)ref;
	if (ptr)
	{
		Completion completion = ptr->Evaluate();
#if JAG_PINK_OR_LATER

		if (completion.isValueCompletion())
		{
			result = (JSObjectRef)KJSValueToJSObject(completion.value(), ptr->GetInterpreter()->globalExec());
		}

		if (completion.complType() == Throw)	
		{
			JSFlags flags = ptr->Flags();
			if (flags & kJSFlagDebug)
			{
				CFTypeRef error = JSObjectCopyCFValue(result);
				if (error)
				{
					CFShow(error);
					CFRelease(error);
				}
			}
		}

#else
		result = (JSObjectRef)KJSValueToJSObject(completion, ptr->GetInterpreter()->globalExec());		
#endif
	}
	return result;
}

/*
	JSRunCheckSyntax
	Return true if no syntax error
*/
bool JSRunCheckSyntax(JSRunRef ref)
{
#if JAG_PINK_OR_LATER
	bool result = false;
	JSRun* ptr = (JSRun*)ref;
	if (ptr)
	{
            JSLockInterpreter();
            result = ptr->CheckSyntax();
            JSUnlockInterpreter();
	}
	return result;
#else
	return true;
#endif
}

/*
	JSCollect - trigger garbage collection
*/
void JSCollect(void)
{
#if JAG_PINK_OR_LATER
	Interpreter::lock();
	Collector::collect();
	Interpreter::unlock(); 
#endif
}

/*
	JSTypeGetCFArrayCallBacks
*/
void JSTypeGetCFArrayCallBacks(CFArrayCallBacks* outCallBacks)
{
	if (outCallBacks)
	{
		outCallBacks->version = 1;
		outCallBacks->retain = (CFArrayRetainCallBack)JSCFRetain;
		outCallBacks->release = (CFArrayReleaseCallBack)JSCFRelease;
		outCallBacks->copyDescription = (CFArrayCopyDescriptionCallBack)JSCopyDescription;
		outCallBacks->equal = (CFArrayEqualCallBack)JSEqual;
	}
}


/*
	JSCFRetain
*/
void* JSCFRetain(CFAllocatorRef allocator, const void *value)
{
	JSRetain((JSTypeRef)value);
	return (void*)value;
}

/*
	JSCFRelease
*/
void JSCFRelease(CFAllocatorRef allocator, const void *value)
{
	JSRelease((JSTypeRef)value);
}


/*
	JSObjectCreateWithCFType
*/
JSObjectRef JSObjectCreateWithCFType(CFTypeRef inRef)
{
	JSObjectCallBacks callBacks;
	JSObjectRef cfJSObject = nil;
	if (inRef)
	{
		callBacks.dispose = CFJSObjectDispose;
		callBacks.equal = CFJSObjectEqual;
		callBacks.copyCFValue = CFJSObjectCopyCFValue;
		callBacks.copyProperty = CFJSObjectCopyProperty;
		callBacks.setProperty = CFJSObjectSetProperty;
		callBacks.callFunction = NULL;
		callBacks.copyPropertyNames = CFJSObjectCopyPropertyNames;
		cfJSObject = JSObjectCreateInternal((void*)CFRetain(inRef), &callBacks, NULL, kJSUserObjectDataTypeCFType );
	}
	return cfJSObject;
}

/*
	CFJSObjectDispose
*/
void CFJSObjectDispose(void* data)
{
	if (data)
	{
		CFRelease((JSTypeRef)data);
	}
}

CFArrayRef JSObjectCopyPropertyNames(JSObjectRef ref)
{
	CFArrayRef result = NULL;
	JSUserObject* ptr = (JSUserObject*)ref;
	if (ptr && (ptr->GetTypeID() == kJSObjectTypeID))
	{
		result = ptr->CopyPropertyNames();
	}
	return result;
}
/*
	CFJSObjectCopyProperty
*/
JSObjectRef CFJSObjectCopyProperty(void* data, CFStringRef propertyName)
{
	JSObjectRef result = NULL;
	if (data && propertyName)
	{
		CFTypeRef cfResult = NULL;
		if (CFGetTypeID(data) == CFDictionaryGetTypeID())
		{
			if (CFStringCompare(propertyName, CFSTR("length"), 0) == kCFCompareEqualTo)
			{
				int len = CFDictionaryGetCount((CFDictionaryRef)data);
				cfResult = CFNumberCreate(NULL, kCFNumberIntType, &len);
			}
			else
			{
				cfResult = RetainCFType(CFDictionaryGetValue((CFDictionaryRef)data, propertyName));
			}
		}
		else if (CFGetTypeID(data) == CFArrayGetTypeID())
		{
			if (CFStringCompare(propertyName, CFSTR("length"), 0) == kCFCompareEqualTo)
			{
				int len = CFArrayGetCount((CFArrayRef)data);
				cfResult = CFNumberCreate(NULL, kCFNumberIntType, &len);
			}
			else
			{
				SInt32 index = CFStringGetIntValue(propertyName);	
				CFIndex arrayCount = CFArrayGetCount((CFArrayRef)data);
				if (index >= 0 && index < arrayCount)
				{
					cfResult = RetainCFType(CFArrayGetValueAtIndex((CFArrayRef)data, index));
				}
			}
		}
		else if (CFGetTypeID(data) == CFStringGetTypeID())
		{
			if (CFStringCompare(propertyName, CFSTR("length"), 0) == kCFCompareEqualTo)
			{
				int len = CFStringGetLength((CFStringRef)data);
				cfResult = CFNumberCreate(NULL, kCFNumberIntType, &len);
			}
		}
		if (cfResult)
		{
			result = JSObjectCreateWithCFType(cfResult);
			CFRelease(cfResult);
		}
	}
	return result;
}


/*
	CFJSObjectSetProperty
*/
void CFJSObjectSetProperty(void* data, CFStringRef propertyName, JSObjectRef jsValue)
{
	if (data && propertyName)
	{
		CFTypeRef cfValue = JSObjectCopyCFValue(jsValue);

		if (cfValue)
		{
			if (CFGetTypeID(data) == CFDictionaryGetTypeID())
			{
				CFDictionarySetValue((CFMutableDictionaryRef)data, propertyName, cfValue);
			}
			else if (CFGetTypeID(data) == CFArrayGetTypeID())
			{
				SInt32 index = CFStringGetIntValue(propertyName);	
				CFIndex arrayCount = CFArrayGetCount((CFArrayRef)data);
				if (index >= 0)
				{
					for (; arrayCount < index; arrayCount++)
					{
						CFArrayAppendValue((CFMutableArrayRef)data, GetCFNull());
					}
					CFArraySetValueAtIndex((CFMutableArrayRef)data, index, cfValue);					
				}
			}		
			CFRelease(cfValue);
		}
		else
		{
			if (CFGetTypeID(data) == CFDictionaryGetTypeID())
			{
				CFDictionaryRemoveValue((CFMutableDictionaryRef)data, propertyName);
			}
			else if (CFGetTypeID(data) == CFArrayGetTypeID())
			{
				SInt32 index = CFStringGetIntValue(propertyName);				
				CFIndex arrayCount = CFArrayGetCount((CFArrayRef)data);
				if (index >= 0)
				{
					for (; arrayCount < index; arrayCount++)
					{
						CFArrayAppendValue((CFMutableArrayRef)data, GetCFNull());
					}
					CFArraySetValueAtIndex((CFMutableArrayRef)data, index, GetCFNull());					
				}				
			}
		}
	}
}


/*
	CFJSObjectCopyCFValue
*/
CFTypeRef CFJSObjectCopyCFValue(void* data)
{
	CFTypeRef result = NULL;
	if (data)
	{
		result = (CFTypeRef)CFRetain(data);
	}
	return result;
}

/*
	CFJSObjectCopyCFValue
*/
UInt8 CFJSObjectEqual(void* data1, void* data2)
{
	UInt8 result = false;
	if (data1 && data2)
	{
		CFEqual((CFTypeRef)data1, (CFTypeRef)data2);
	}
	return result;
}


/*
	CFJSObjectCopyPropertyNames
*/
CFArrayRef CFJSObjectCopyPropertyNames(void* data)
{
	CFMutableArrayRef result = NULL;
	if (data)
	{
		CFTypeID cfType = CFGetTypeID(data);
		if (cfType == CFDictionaryGetTypeID())
		{
			CFIndex count = CFDictionaryGetCount((CFDictionaryRef)data);
			if (count)
			{
				CFTypeRef* keys = (CFTypeRef*)malloc(sizeof(CFTypeRef)*count);
				if (keys)
				{
					int i;
					CFDictionaryGetKeysAndValues((CFDictionaryRef)data, (const void **)keys, NULL);
					for (i = 0; i < count; i++)
					{
						CFStringRef key = (CFStringRef)keys[i];
						if (CFGetTypeID(key) != CFStringGetTypeID()) continue;

						if (!result) result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
						if (!result) continue;
						
						CFArrayAppendValue(result, key);
					}
					free(keys);
				}
			}
		}
	}
	return result;
}




CFMutableArrayRef JSCreateCFArrayFromJSArray(CFArrayRef array)
{
	CFIndex count = array ? CFArrayGetCount(array) : 0;
	CFMutableArrayRef cfArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFIndex i;

	for (i = 0; cfArray && i <  count; i++)
	{
		JSObjectRef jsValue = (JSObjectRef)CFArrayGetValueAtIndex(array, i);
		CFTypeRef cfvalue = JSObjectCopyCFValue(jsValue);
		if (cfvalue)
		{
			CFArrayAppendValue(cfArray, cfvalue);
			CFRelease(cfvalue);
		}
		else
		{
			CFArrayAppendValue(cfArray, GetCFNull());
		}
	}
	return cfArray;
}

CFMutableArrayRef JSCreateJSArrayFromCFArray(CFArrayRef array)
{
	CFIndex count = array ? CFArrayGetCount(array) : 0;
	CFArrayCallBacks arrayCallbacks;
	CFMutableArrayRef jsArray;
	CFIndex i;

	JSTypeGetCFArrayCallBacks(&arrayCallbacks);
	jsArray = CFArrayCreateMutable(NULL, 0, &arrayCallbacks);

	for (i = 0; array && i <  count; i++)
	{
		CFTypeRef cfValue = (CFTypeRef)CFArrayGetValueAtIndex(array, i);
		JSObjectRef jsValue = JSObjectCreateWithCFType(cfValue);
		
		if (!jsValue) jsValue = JSObjectCreateWithCFType(GetCFNull());
		if (jsValue)
		{
			CFArrayAppendValue(jsArray, jsValue);
			JSRelease(jsValue);
		}
	}
	return jsArray;
}


void JSLockInterpreter()
{
#if JAG_PINK_OR_LATER
	Interpreter::lock();
#endif
}


void JSUnlockInterpreter()
{
#if JAG_PINK_OR_LATER
	Interpreter::unlock();
#endif
}

