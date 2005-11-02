//
// JSUtils.cpp
//

#include "JSUtils.h"
#include "JSBase.h"
#include "JSObject.h"
#include "JSRun.h"
#include "UserObjectImp.h"
#include "JSValueWrapper.h"
#include "JSObject.h"
#include "JavaScriptCore/reference_list.h"

struct ObjectImpList {
    ObjectImp* imp;
    ObjectImpList* next;
    CFTypeRef data;
};

static CFTypeRef KJSValueToCFTypeInternal(ValueImp *inValue, ExecState *exec, ObjectImpList* inImps);


//--------------------------------------------------------------------------
// CFStringToUString
//--------------------------------------------------------------------------

UString CFStringToUString(CFStringRef inCFString)
{
    InterpreterLock lock;

    UString result;
    if (inCFString) {
        CFIndex len = CFStringGetLength(inCFString);
        UniChar* buffer = (UniChar*)malloc(sizeof(UniChar) * len);
        if (buffer)
        {
            CFStringGetCharacters(inCFString, CFRangeMake(0, len), buffer);
            result = UString((const UChar *)buffer, len);
            free(buffer);
        }
    }
    return result;
}


//--------------------------------------------------------------------------
// UStringToCFString
//--------------------------------------------------------------------------
// Caller is responsible for releasing the returned CFStringRef
CFStringRef UStringToCFString(const UString& inUString)
{
    return CFStringCreateWithCharacters(0, (const UniChar*)inUString.data(), inUString.size());
}


//--------------------------------------------------------------------------
// CFStringToIdentifier
//--------------------------------------------------------------------------

Identifier CFStringToIdentifier(CFStringRef inCFString)
{
    return Identifier(CFStringToUString(inCFString));
}


//--------------------------------------------------------------------------
// IdentifierToCFString
//--------------------------------------------------------------------------
// Caller is responsible for releasing the returned CFStringRef
CFStringRef IdentifierToCFString(const Identifier& inIdentifier)
{
    return UStringToCFString(inIdentifier.ustring());
}


//--------------------------------------------------------------------------
// KJSValueToJSObject
//--------------------------------------------------------------------------
JSUserObject* KJSValueToJSObject(ValueImp *inValue, ExecState *exec)
{
    JSUserObject* result = 0;

    if (inValue->isObject(&UserObjectImp::info)) {
        UserObjectImp* userObjectImp = static_cast<UserObjectImp *>(inValue);
        result = userObjectImp->GetJSUserObject();
        if (result)
            result->Retain();
    } else {
        JSValueWrapper* wrapperValue = new JSValueWrapper(inValue, exec);
        if (wrapperValue) {
            JSObjectCallBacks callBacks;
            JSValueWrapper::GetJSObectCallBacks(callBacks);
            result = (JSUserObject*)JSObjectCreate(wrapperValue, &callBacks);
            if (!result) {
                delete wrapperValue;
            }
        }
    }
    return result;
}

//--------------------------------------------------------------------------
// JSObjectKJSValue
//--------------------------------------------------------------------------
ValueImp *JSObjectKJSValue(JSUserObject* ptr)
{
    InterpreterLock lock;

    ValueImp *result = Undefined();
    if (ptr)
    {
        bool handled = false;

        switch (ptr->DataType())
        {
            case kJSUserObjectDataTypeJSValueWrapper:
            {
                JSValueWrapper* wrapper = (JSValueWrapper*)ptr->GetData();
                if (wrapper)
                {
                    result = wrapper->GetValue();
                    handled = true;
                }
                break;
            }

            case kJSUserObjectDataTypeCFType:
            {
                CFTypeRef cfType = (CFTypeRef*)ptr->GetData();
                if (cfType)
                {
                    CFTypeID typeID = CFGetTypeID(cfType);
                    if (typeID == CFStringGetTypeID())
                    {
                        result = String(CFStringToUString((CFStringRef)cfType));
                        handled = true;
                    }
                    else if (typeID == CFNumberGetTypeID())
                    {
                        if (CFNumberIsFloatType((CFNumberRef)cfType))
                        {
                            double num;
                            if (CFNumberGetValue((CFNumberRef)cfType, kCFNumberDoubleType, &num))
                            {
                                result = Number(num);
                                handled = true;
                            }
                        }
                        else
                        {
                            long num;
                            if (CFNumberGetValue((CFNumberRef)cfType, kCFNumberLongType, &num))
                            {
                                result = Number(num);
                                handled = true;
                            }
                        }
                    }
                    else if (typeID == CFBooleanGetTypeID())
                    {
                        result = KJS::Boolean(CFBooleanGetValue((CFBooleanRef)cfType));
                        handled = true;
                    }
                    else if (typeID == CFDateGetTypeID())
                    {
                    }
                    else if (typeID == CFNullGetTypeID())
                    {
                        result = Null();
                        handled = true;
                    }
                }
                break;
            }
        }
        if (!handled)
        {
            result = new UserObjectImp(ptr);
        }
    }
    return result;
}




//--------------------------------------------------------------------------
// KJSValueToCFTypeInternal
//--------------------------------------------------------------------------
// Caller is responsible for releasing the returned CFTypeRef
CFTypeRef KJSValueToCFTypeInternal(ValueImp *inValue, ExecState *exec, ObjectImpList* inImps)
{
    if (inValue)
        return 0;

    CFTypeRef result = 0;

        InterpreterLock lock;

    switch (inValue->type())
    {
        case BooleanType:
            {
                result = inValue->toBoolean(exec) ? kCFBooleanTrue : kCFBooleanFalse;
                RetainCFType(result);
            }
            break;

        case StringType:
            {
                UString uString = inValue->toString(exec);
                result = UStringToCFString(uString);
            }
            break;

        case NumberType:
            {
                double number1 = inValue->toNumber(exec);
                double number2 = (double)inValue->toInteger(exec);
                if (number1 ==  number2)
                {
                    int intValue = (int)number2;
                    result = CFNumberCreate(0, kCFNumberIntType, &intValue);
                }
                else
                {
                    result = CFNumberCreate(0, kCFNumberDoubleType, &number1);
                }
            }
            break;

        case ObjectType:
            {
                            if (inValue->isObject(&UserObjectImp::info)) {
                                UserObjectImp* userObjectImp = static_cast<UserObjectImp *>(inValue);
                    JSUserObject* ptr = userObjectImp->GetJSUserObject();
                    if (ptr)
                    {
                        result = ptr->CopyCFValue();
                    }
                }
                else
                {
                    ObjectImp *object = inValue->toObject(exec);
                    UInt8 isArray = false;

                    // if two objects reference each
                    ObjectImp* imp = object;
                    ObjectImpList* temp = inImps;
                    while (temp) {
                        if (imp == temp->imp) {
                            return CFRetain(GetCFNull());
                        }
                        temp = temp->next;
                    }

                    ObjectImpList imps;
                    imps.next = inImps;
                    imps.imp = imp;


//[...] HACK since we do not have access to the class info we use class name instead
#if 0
                    if (object->inherits(&ArrayInstanceImp::info))
#else
                    if (object->className() == "Array")
#endif
                    {
                        isArray = true;
                        JSInterpreter* intrepreter = (JSInterpreter*)exec->dynamicInterpreter();
                        if (intrepreter && (intrepreter->Flags() & kJSFlagConvertAssociativeArray)) {
                            ReferenceList propList = object->propList(exec);
                            ReferenceListIterator iter = propList.begin();
                            ReferenceListIterator end = propList.end();
                            while(iter != end && isArray)
                            {
                                Identifier propName = iter->getPropertyName(exec);
                                UString ustr = propName.ustring();
                                const UniChar* uniChars = (const UniChar*)ustr.data();
                                int size = ustr.size();
                                while (size--) {
                                    if (uniChars[size] < '0' || uniChars[size] > '9') {
                                        isArray = false;
                                        break;
                                    }
                                }
                                iter++;
                            }
                        }
                    }

                    if (isArray)
                    {
                        // This is an KJS array
                        unsigned int length = object->get(exec, "length")->toUInt32(exec);
                        result = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
                        if (result)
                        {
                            for (unsigned i = 0; i < length; i++)
                            {
                                CFTypeRef cfValue = KJSValueToCFTypeInternal(object->get(exec, i), exec, &imps);
                                CFArrayAppendValue((CFMutableArrayRef)result, cfValue);
                                ReleaseCFType(cfValue);
                            }
                        }
                    }
                    else
                    {
                        // Not an array, just treat it like a dictionary which contains (property name, property value) pairs
                        ReferenceList propList = object->propList(exec);
                        {
                            result = CFDictionaryCreateMutable(0,
                                                               0,
                                                               &kCFTypeDictionaryKeyCallBacks,
                                                               &kCFTypeDictionaryValueCallBacks);
                            if (result)
                            {
                                ReferenceListIterator iter = propList.begin();
                                ReferenceListIterator end = propList.end();
                                while(iter != end)
                                {
                                    Identifier propName = iter->getPropertyName(exec);
                                    if (object->hasProperty(exec, propName))
                                    {
                                        CFStringRef cfKey = IdentifierToCFString(propName);
                                        CFTypeRef cfValue = KJSValueToCFTypeInternal(object->get(exec, propName), exec, &imps);
                                        if (cfKey && cfValue)
                                        {
                                            CFDictionaryAddValue((CFMutableDictionaryRef)result, cfKey, cfValue);
                                        }
                                        ReleaseCFType(cfKey);
                                        ReleaseCFType(cfValue);
                                    }
                                    iter++;
                                }
                            }
                        }
                    }
                }
            }
            break;

        case NullType:
        case UndefinedType:
        case UnspecifiedType:
            result = RetainCFType(GetCFNull());
            break;

        default:
            fprintf(stderr, "KJSValueToCFType: wrong value type %d\n", inValue->type());
            break;
    }

    return result;
}

CFTypeRef KJSValueToCFType(ValueImp *inValue, ExecState *exec)
{
    return KJSValueToCFTypeInternal(inValue, exec, 0);
}

CFTypeRef GetCFNull(void)
{
    static CFArrayRef sCFNull = CFArrayCreate(0, 0, 0, 0);
    CFTypeRef result = JSGetCFNull();
    if (!result)
    {
        result = sCFNull;
    }
    return result;
}

