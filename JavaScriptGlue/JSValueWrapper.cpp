//
// JSValueWrapper.cpp
//
#include "JSValueWrapper.h"
#include "JavaScriptCore/IdentifierSequencedSet.h"

JSValueWrapper::JSValueWrapper(ValueImp *inValue, ExecState *inExec) 
	: fValue(inValue), fExec(inExec) 
{ 
}

JSValueWrapper::~JSValueWrapper()
{ 
}

ValueImp *JSValueWrapper::GetValue() 
{ 
	return fValue; 
}
ExecState* JSValueWrapper::GetExecState() const 
{
	return fExec; 
}
	

void JSValueWrapper::GetJSObectCallBacks(JSObjectCallBacks& callBacks)
{
	callBacks.dispose = (JSObjectDisposeProcPtr)JSValueWrapper::JSObjectDispose;
	callBacks.equal = (JSObjectEqualProcPtr)NULL;
	callBacks.copyPropertyNames = (JSObjectCopyPropertyNamesProcPtr)JSValueWrapper::JSObjectCopyPropertyNames;
	callBacks.copyCFValue = (JSObjectCopyCFValueProcPtr)JSValueWrapper::JSObjectCopyCFValue;
	callBacks.copyProperty = (JSObjectCopyPropertyProcPtr)JSValueWrapper::JSObjectCopyProperty;
	callBacks.setProperty = (JSObjectSetPropertyProcPtr)JSValueWrapper::JSObjectSetProperty;
	callBacks.callFunction = (JSObjectCallFunctionProcPtr)JSValueWrapper::JSObjectCallFunction;
}
			
void JSValueWrapper::JSObjectDispose(void* data)
{
	JSValueWrapper* ptr = (JSValueWrapper*)data;
	delete ptr;
}
	

CFArrayRef JSValueWrapper::JSObjectCopyPropertyNames(void* data)
{
    InterpreterLock lock;

	CFMutableArrayRef result = NULL;
	JSValueWrapper* ptr = (JSValueWrapper*)data;
	if (ptr)
	{
		ExecState* exec = ptr->GetExecState();
		ObjectImp *object = ptr->GetValue()->toObject(exec);
		IdentifierSequencedSet list;
                object->getPropertyNames(exec, list);
		IdentifierSequencedSetIterator iterator = list.begin();

		while (iterator != list.end()) {
			Identifier name = *iterator;
			CFStringRef nameStr = IdentifierToCFString(name);

			if (!result)
			{
				result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			}
			if (result && nameStr)
			{
				CFArrayAppendValue(result, nameStr);
			}
			ReleaseCFType(nameStr);
			++iterator;
		}

	}
	return result;
}


JSObjectRef JSValueWrapper::JSObjectCopyProperty(void* data, CFStringRef propertyName)
{
    InterpreterLock lock;

	JSObjectRef result = NULL;
	JSValueWrapper* ptr = (JSValueWrapper*)data;
	if (ptr)
	{
		ExecState* exec = ptr->GetExecState();
		ValueImp *propValue = ptr->GetValue()->toObject(exec)->get(exec, CFStringToIdentifier(propertyName));
		JSValueWrapper* wrapperValue = new JSValueWrapper(propValue, exec);

		JSObjectCallBacks callBacks;
		GetJSObectCallBacks(callBacks);
		result = JSObjectCreateInternal(wrapperValue, &callBacks, JSValueWrapper::JSObjectMark, kJSUserObjectDataTypeJSValueWrapper);

		if (!result)
		{
			delete wrapperValue;
		}
	}
	return result;
}

void JSValueWrapper::JSObjectSetProperty(void* data, CFStringRef propertyName, JSObjectRef jsValue)
{
    InterpreterLock lock;

	JSValueWrapper* ptr = (JSValueWrapper*)data;
	if (ptr)
	{
		ExecState* exec = ptr->GetExecState();	
		ValueImp *value = JSObjectKJSValue((JSUserObject*)jsValue);
		ObjectImp *objValue = ptr->GetValue()->toObject(exec);
		objValue->put(exec, CFStringToIdentifier(propertyName), value);
	}
}

JSObjectRef JSValueWrapper::JSObjectCallFunction(void* data, JSObjectRef thisObj, CFArrayRef args)
{
    InterpreterLock lock;

	JSObjectRef result = NULL;
	JSValueWrapper* ptr = (JSValueWrapper*)data;
	if (ptr)
	{
		ExecState* exec = ptr->GetExecState();	
	
		ValueImp *value = JSObjectKJSValue((JSUserObject*)thisObj);
		ObjectImp *ksjThisObj = value->toObject(exec);
		ObjectImp *objValue = ptr->GetValue()->toObject(exec);

		List listArgs;
		CFIndex argCount = args ? CFArrayGetCount(args) : 0;
		for (CFIndex i = 0; i < argCount; i++)
		{
			JSObjectRef jsArg = (JSObjectRef)CFArrayGetValueAtIndex(args, i);
			ValueImp *kgsArg = JSObjectKJSValue((JSUserObject*)jsArg);
			listArgs.append(kgsArg);
		}

		ValueImp *resultValue = objValue->call(exec, ksjThisObj, listArgs);
		JSValueWrapper* wrapperValue = new JSValueWrapper(resultValue, ptr->GetExecState());
		JSObjectCallBacks callBacks;
		GetJSObectCallBacks(callBacks);
		result = JSObjectCreate(wrapperValue, &callBacks);
		if (!result)
		{
			delete wrapperValue;
		}
	}
	return result;
}

CFTypeRef JSValueWrapper::JSObjectCopyCFValue(void* data)
{
    InterpreterLock lock;

	CFTypeRef result = NULL;
	JSValueWrapper* ptr = (JSValueWrapper*)data;
	if (ptr)
	{
		result = KJSValueToCFType(ptr->fValue, ptr->fExec);
	}
	return result;
}

void JSValueWrapper::JSObjectMark(void* data)
{
	JSValueWrapper* ptr = (JSValueWrapper*)data;
	if (ptr)
	{
		ptr->fValue->mark();
	}
}
