#include "UserObjectImp.h"

const ClassInfo UserObjectImp::info = {"UserObject", 0, 0, 0};

class UserObjectPrototypeImp : public UserObjectImp {
  public:
    UserObjectPrototypeImp();
	static UserObjectPrototypeImp* GlobalUserObjectPrototypeImp();
  private:
	static UserObjectPrototypeImp* sUserObjectPrototypeImp;
};

UserObjectPrototypeImp* UserObjectPrototypeImp::sUserObjectPrototypeImp = NULL;

UserObjectPrototypeImp::UserObjectPrototypeImp()
  : UserObjectImp()
{
}

UserObjectPrototypeImp* UserObjectPrototypeImp::GlobalUserObjectPrototypeImp()
{
	if (!sUserObjectPrototypeImp)
	{
		sUserObjectPrototypeImp = new UserObjectPrototypeImp();
		static ProtectedValue protectPrototype = Value(sUserObjectPrototypeImp);
	}
	return sUserObjectPrototypeImp;
}


UserObjectImp::UserObjectImp(): ObjectImp(), fJSUserObject(NULL)
{
}

UserObjectImp::UserObjectImp(JSUserObject* userObject) : 
	ObjectImp(Object(UserObjectPrototypeImp::GlobalUserObjectPrototypeImp())), 
	fJSUserObject((JSUserObject*)userObject->Retain()) 
{ 
}

UserObjectImp::~UserObjectImp() 
{
	if (fJSUserObject)
	{
		fJSUserObject->Release(); 
	}
}
    
const ClassInfo	* UserObjectImp::classInfo() const 
{
	return &info; 
}
        
bool	UserObjectImp::implementsCall() const 
{
	return fJSUserObject ? fJSUserObject->ImplementsCall() : false; 
}

Value	UserObjectImp::call(ExecState *exec, Object &thisObj, const List &args)
{
	Value result = Undefined();
	JSUserObject* jsThisObj = KJSValueToJSObject(thisObj, exec);
	if (jsThisObj)
	{
		CFIndex argCount = args.size();
		CFArrayCallBacks arrayCallBacks;
		JSTypeGetCFArrayCallBacks(&arrayCallBacks);
		CFMutableArrayRef jsArgs = CFArrayCreateMutable(NULL, 0, &arrayCallBacks);
		if (jsArgs)
		{
			for (CFIndex i = 0; i < argCount;  i++)
			{
				JSUserObject* jsArg = KJSValueToJSObject(args[i], exec);
				CFArrayAppendValue(jsArgs, (void*)jsArg);
				jsArg->Release();
			}
		}

#if 0
		JSUserObject* jsResult = fJSUserObject->CallFunction(jsThisObj, jsArgs);
#else
		int lockCount = Interpreter::lockCount();
		int i;
		for (i = 0; i < lockCount; i++)
		{
			Interpreter::unlock();
		}

		JSUserObject* jsResult = fJSUserObject->CallFunction(jsThisObj, jsArgs);

		for (i = 0; i < lockCount; i++)
		{
			Interpreter::lock();
		}
#endif
		if (jsResult)
		{
			result = JSObjectKJSValue(jsResult);
			jsResult->Release();
		}

		ReleaseCFType(jsArgs);
		jsThisObj->Release();
	}
	return result;
}


ReferenceList UserObjectImp::propList(ExecState *exec, bool recursive)
{
	ReferenceList propList;
	JSUserObject* ptr = GetJSUserObject();
	if (ptr)
	{
		Object theObject = toObject(exec);
		CFArrayRef propertyNames = ptr->CopyPropertyNames();
		if (propertyNames)
		{
			CFIndex count = CFArrayGetCount(propertyNames);
			CFIndex i;
			for (i = 0; i < count; i++)
			{
				CFStringRef propertyName = (CFStringRef)CFArrayGetValueAtIndex(propertyNames, i);
				propList.append(Reference(theObject, CFStringToIdentifier(propertyName)));
			}
			CFRelease(propertyNames);
		}
	}
	return propList;
}

bool UserObjectImp::hasProperty(ExecState *exec, const Identifier &propertyName) const
{
	Value prop = get(exec, propertyName);
	if (prop.type() != UndefinedType)
	{
		return true;
	}
	return ObjectImp::hasProperty(exec, propertyName);
}

#if JAG_PINK_OR_LATER
Value	UserObjectImp::get(ExecState *exec, const Identifier &propertyName) const
{
	Value result = Undefined();
	CFStringRef	cfPropName = IdentifierToCFString(propertyName);
	JSUserObject* jsResult = fJSUserObject->CopyProperty(cfPropName);
	if (jsResult)
	{
		result = JSObjectKJSValue(jsResult);
		jsResult->Release();
	}
	else
	{
		Value kjsValue = toPrimitive(exec);
		if (kjsValue.type() != NullType && kjsValue.type() != UndefinedType)
		{
			Object kjsObject = kjsValue.toObject(exec);
			Value kjsObjectProp = kjsObject.get(exec, propertyName);
			result = kjsObjectProp;
		}
	}
	ReleaseCFType(cfPropName);
	return result;

}

void	UserObjectImp::put(ExecState *exec, const Identifier &propertyName, const Value &value, int attr)
{
	CFStringRef	cfPropName = IdentifierToCFString(propertyName);
	JSUserObject* jsValueObj = KJSValueToJSObject(value, exec);

	fJSUserObject->SetProperty(cfPropName, jsValueObj);

	if (jsValueObj) jsValueObj->Release();	
	ReleaseCFType(cfPropName);
}
#else	//JAG_PINK_OR_LATER
Value	UserObjectImp::get(ExecState* exec, const UString& propertyName) const
{
	Value result = Undefined();
	CFStringRef	cfPropName = UStringToCFString(propertyName);
	JSUserObject* jsResult = fJSUserObject->CopyProperty(cfPropName);
	if (jsResult)
	{
		result = JSObjectKJSValue(jsResult);
		jsResult->Release();
	}
	else
	{
		Value kjsValue = toPrimitive(exec);
		if (kjsValue != Null() && kjsValue != Undefined())
		{
			Object kjsObject = kjsValue.toObject(exec);
			Value kjsObjectProp = kjsObject.get(exec, propertyName);
			result = kjsObjectProp;
		}
	}
	ReleaseCFType(cfPropName);
	return result;

}

void	UserObjectImp::put(ExecState* exec, const UString& propertyName, const Value& value, int attr)
{
	CFStringRef	cfPropName = UStringToCFString(propertyName);
	JSUserObject* jsValueObj = KJSValueToJSObject(value, exec);

	fJSUserObject->SetProperty(cfPropName, jsValueObj);

	if (jsValueObj) jsValueObj->Release();	
	ReleaseCFType(cfPropName);
}
#endif	//JAG_PINK_OR_LATER
        
JSUserObject* UserObjectImp::GetJSUserObject() const 
{ 
	return fJSUserObject; 
}



Value UserObjectImp::toPrimitive(ExecState *exec, Type preferredType) const
{
	Value result = Undefined();
	JSUserObject* jsObjPtr = KJSValueToJSObject(toObject(exec), exec);
	CFTypeRef cfValue = jsObjPtr ? jsObjPtr->CopyCFValue() : NULL;
	if (cfValue)
	{
		CFTypeID cfType = CFGetTypeID(cfValue);  // toPrimitive
		if (cfValue == GetCFNull())
		{
			result = Null();
		}
		else if (cfType == CFBooleanGetTypeID())
		{
			if (cfValue == kCFBooleanTrue)
			{
				result = KJS::Boolean(true);
			}
			else 
			{
				result = KJS::Boolean(false);
			}
		}
		else if (cfType == CFStringGetTypeID())
		{
			result = KJS::String(CFStringToUString((CFStringRef)cfValue));
		}
		else if (cfType == CFNumberGetTypeID())
		{
			double d = 0.0;
			CFNumberGetValue((CFNumberRef)cfValue, kCFNumberDoubleType, &d);
			result = KJS::Number(d);
		}
		else if (cfType == CFURLGetTypeID())
		{
			CFURLRef absURL = CFURLCopyAbsoluteURL((CFURLRef)cfValue);
			if (absURL)
			{
				result = KJS::String(CFStringToUString(CFURLGetString(absURL)));
				ReleaseCFType(absURL);
			}
		}		
		ReleaseCFType(cfValue);
	}
	if (jsObjPtr) jsObjPtr->Release();
	return result;
}


bool UserObjectImp::toBoolean(ExecState *exec) const
{
	bool result = false;
	JSUserObject* jsObjPtr = KJSValueToJSObject(toObject(exec), exec);
	CFTypeRef cfValue = jsObjPtr ? jsObjPtr->CopyCFValue() : NULL;
	if (cfValue)
	{
		CFTypeID cfType = CFGetTypeID(cfValue);  // toPrimitive
		if (cfValue == GetCFNull())
		{
			//
		}
		else if (cfType == CFBooleanGetTypeID())
		{
			if (cfValue == kCFBooleanTrue)
			{
				result = true;
			}
		}
		else if (cfType == CFStringGetTypeID())
		{
			if (CFStringGetLength((CFStringRef)cfValue))
			{
				result = true;
			}
		}
		else if (cfType == CFNumberGetTypeID())
		{
			if (cfValue != kCFNumberNaN)
			{
				double d;
				if (CFNumberGetValue((CFNumberRef)cfValue, kCFNumberDoubleType, &d))
				{
					if (d != 0)
					{
						result = true;
					}
				}
			}
		}
		else if (cfType == CFArrayGetTypeID())
		{
			if (CFArrayGetCount((CFArrayRef)cfValue))
			{
				result = true;
			}
		}
		else if (cfType == CFDictionaryGetTypeID())
		{
			if (CFDictionaryGetCount((CFDictionaryRef)cfValue))
			{
				result = true;
			}
		}
		else if (cfType == CFSetGetTypeID())
		{
			if (CFSetGetCount((CFSetRef)cfValue))
			{
				result = true;
			}
		}
		else if (cfType == CFURLGetTypeID())
		{
			CFURLRef absURL = CFURLCopyAbsoluteURL((CFURLRef)cfValue);
			if (absURL)
			{
				CFStringRef cfStr = CFURLGetString(absURL);
				if (cfStr && CFStringGetLength(cfStr))
				{
					result = true;
				}
				ReleaseCFType(absURL);
			}
		}
	}
	if (jsObjPtr) jsObjPtr->Release();
	ReleaseCFType(cfValue);
	return result;
}

double UserObjectImp::toNumber(ExecState *exec) const
{
	double result = 0;
	JSUserObject* jsObjPtr = KJSValueToJSObject(toObject(exec), exec);
	CFTypeRef cfValue = jsObjPtr ? jsObjPtr->CopyCFValue() : NULL;
	if (cfValue)
	{
		CFTypeID cfType = CFGetTypeID(cfValue);
		
		if (cfValue == GetCFNull())
		{
			//
		}
		else if (cfType == CFBooleanGetTypeID())
		{
			if (cfValue == kCFBooleanTrue)
			{
				result = 1;
			}
		}
		else if (cfType == CFStringGetTypeID())
		{
			result = CFStringGetDoubleValue((CFStringRef)cfValue);
		}
		else if (cfType == CFNumberGetTypeID())
		{
			CFNumberGetValue((CFNumberRef)cfValue, kCFNumberDoubleType, &result);		
		}
	}
	ReleaseCFType(cfValue);
	if (jsObjPtr) jsObjPtr->Release();
	return result;
}

UString UserObjectImp::toString(ExecState *exec) const
{
	UString result;
	JSUserObject* jsObjPtr = KJSValueToJSObject(toObject(exec), exec);
	CFTypeRef cfValue = jsObjPtr ? jsObjPtr->CopyCFValue() : NULL;
	if (cfValue)
	{
		CFTypeID cfType = CFGetTypeID(cfValue);
		if (cfValue == GetCFNull())
		{
			//
		}
		else if (cfType == CFBooleanGetTypeID())
		{
			if (cfValue == kCFBooleanTrue)
			{
				result = "true";
			}
			else
			{
				result = "false";
			}
		}
		else if (cfType == CFStringGetTypeID())
		{
			result = CFStringToUString((CFStringRef)cfValue);
		}
		else if (cfType == CFNumberGetTypeID())
		{
			if (cfValue == kCFNumberNaN)
			{
				result = "Nan";
			}
			else if (CFNumberCompare(kCFNumberPositiveInfinity, (CFNumberRef)cfValue, NULL) == 0)
			{
				result = "Infinity";
			}
			else if (CFNumberCompare(kCFNumberNegativeInfinity, (CFNumberRef)cfValue, NULL) == 0)
			{
				result = "-Infinity";
			}
			else
			{
				CFStringRef cfNumStr = NULL;
				if (CFNumberIsFloatType((CFNumberRef)cfValue))
				{
					double d = 0;
					CFNumberGetValue((CFNumberRef)cfValue, kCFNumberDoubleType, &d);
					cfNumStr = CFStringCreateWithFormat(NULL, NULL, CFSTR("%f"), (float)d);
				}
				else
				{
					int i = 0;
					CFNumberGetValue((CFNumberRef)cfValue, kCFNumberIntType, &i);
					cfNumStr = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), (int)i);
				}
				
				if (cfNumStr)
				{
					result = CFStringToUString(cfNumStr);
					ReleaseCFType(cfNumStr);
				}				
			}
		}
		else if (cfType == CFArrayGetTypeID())
		{
			//
		}
		else if (cfType == CFDictionaryGetTypeID())
		{
			//
		}
		else if (cfType == CFSetGetTypeID())
		{
			//
		}
		else if (cfType == CFURLGetTypeID())
		{
			CFURLRef absURL = CFURLCopyAbsoluteURL((CFURLRef)cfValue);
			if (absURL)
			{
				CFStringRef cfStr = CFURLGetString(absURL);
				if (cfStr)
				{
					result = CFStringToUString(cfStr);
				}
				ReleaseCFType(absURL);
			}
		}		
	}
	ReleaseCFType(cfValue);
	if (jsObjPtr) jsObjPtr->Release();
	return result;
}

void UserObjectImp::mark()
{
	ObjectImp::mark(); // call parent to mark self
	if (fJSUserObject)
	{
		fJSUserObject->Mark(); // mark child
	}
}
