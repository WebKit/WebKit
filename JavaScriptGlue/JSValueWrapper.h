#ifndef __JSValueWrapper_h
#define __JSValueWrapper_h

/*
	JSValueWrapper.h
*/

#include "JSUtils.h"
#include "JSBase.h"
#include "JSObject.h"

class JSValueWrapper {
	public:
		JSValueWrapper(const Value& inValue, ExecState *inExec);
		virtual ~JSValueWrapper();

		Value& GetValue();
		ExecState* GetExecState() const;
	
		ProtectedValue fValue;
		ExecState* fExec;

		static void GetJSObectCallBacks(JSObjectCallBacks& callBacks);
			
	private:
		static void JSObjectDispose(void* data);
		static CFArrayRef JSObjectCopyPropertyNames(void* data);
		static JSObjectRef JSObjectCopyProperty(void* data, CFStringRef propertyName);
		static void JSObjectSetProperty(void* data, CFStringRef propertyName, JSObjectRef jsValue);
		static JSObjectRef JSObjectCallFunction(void* data, JSObjectRef thisObj, CFArrayRef args);
		static CFTypeRef JSObjectCopyCFValue(void* data);
		static void JSObjectMark(void* data);
};


#endif