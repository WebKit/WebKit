#include "UserObjectImp.h"
#include "JavaScriptCore/reference_list.h"

const ClassInfo UserObjectImp::info = {"UserObject", 0, 0, 0};

class UserObjectPrototypeImp : public UserObjectImp {
  public:
    UserObjectPrototypeImp();
    static UserObjectPrototypeImp* GlobalUserObjectPrototypeImp();
  private:
    static UserObjectPrototypeImp* sUserObjectPrototypeImp;
};

UserObjectPrototypeImp* UserObjectPrototypeImp::sUserObjectPrototypeImp = 0;

UserObjectPrototypeImp::UserObjectPrototypeImp()
  : UserObjectImp()
{
}

UserObjectPrototypeImp* UserObjectPrototypeImp::GlobalUserObjectPrototypeImp()
{
    if (!sUserObjectPrototypeImp)
    {
            sUserObjectPrototypeImp  = new UserObjectPrototypeImp();
            static ProtectedPtr<UserObjectPrototypeImp> protectPrototype;
    }
    return sUserObjectPrototypeImp;
}


UserObjectImp::UserObjectImp(): ObjectImp(), fJSUserObject(0)
{
}

UserObjectImp::UserObjectImp(JSUserObject* userObject) :
    ObjectImp(UserObjectPrototypeImp::GlobalUserObjectPrototypeImp()),
    fJSUserObject((JSUserObject*)userObject->Retain())
{
}

UserObjectImp::~UserObjectImp()
{
    if (fJSUserObject)
        fJSUserObject->Release();
}

const ClassInfo * UserObjectImp::classInfo() const
{
    return &info;
}

bool UserObjectImp::implementsCall() const
{
    return fJSUserObject ? fJSUserObject->ImplementsCall() : false;
}

ValueImp *UserObjectImp::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
    ValueImp *result = Undefined();
    JSUserObject* jsThisObj = KJSValueToJSObject(thisObj, exec);
    if (jsThisObj) {
        CFIndex argCount = args.size();
        CFArrayCallBacks arrayCallBacks;
        JSTypeGetCFArrayCallBacks(&arrayCallBacks);
        CFMutableArrayRef jsArgs = CFArrayCreateMutable(0, 0, &arrayCallBacks);
        if (jsArgs) {
            for (CFIndex i = 0; i < argCount; i++) {
                JSUserObject* jsArg = KJSValueToJSObject(args[i], exec);
                CFArrayAppendValue(jsArgs, (void*)jsArg);
                jsArg->Release();
            }
        }

        JSUserObject* jsResult;
        { // scope
            InterpreterLock::DropAllLocks dropLocks;

            // implementsCall should have guarded against a NULL fJSUserObject.
            assert(fJSUserObject);
            jsResult = fJSUserObject->CallFunction(jsThisObj, jsArgs);
        }

        if (jsResult) {
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
    ReferenceList list = ObjectImp::propList(exec, recursive);
    JSUserObject* ptr = GetJSUserObject();
    if (ptr) {
        CFArrayRef cfPropertyNames = ptr->CopyPropertyNames();
        if (cfPropertyNames) {
            CFIndex count = CFArrayGetCount(cfPropertyNames);
            CFIndex i;
            for (i = 0; i < count; i++) {
                CFStringRef propertyName = (CFStringRef)CFArrayGetValueAtIndex(cfPropertyNames, i);
                list.append(Reference(this, CFStringToIdentifier(propertyName)));
            }
            CFRelease(cfPropertyNames);
        }
    }

    return list;
}

ValueImp *UserObjectImp::userObjectGetter(ExecState *, const Identifier& propertyName, const PropertySlot& slot)
{
    UserObjectImp *thisObj = static_cast<UserObjectImp *>(slot.slotBase());
    // getOwnPropertySlot should have guarded against a null fJSUserObject.
    assert(thisObj->fJSUserObject);
    
    CFStringRef cfPropName = IdentifierToCFString(propertyName);
    JSUserObject *jsResult = thisObj->fJSUserObject->CopyProperty(cfPropName);
    ReleaseCFType(cfPropName);
    ValueImp *result = JSObjectKJSValue(jsResult);
    jsResult->Release();

    return result;
}

bool UserObjectImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (!fJSUserObject)
        return false;

    CFStringRef cfPropName = IdentifierToCFString(propertyName);
    JSUserObject *jsResult = fJSUserObject->CopyProperty(cfPropName);
    ReleaseCFType(cfPropName);
    if (jsResult) {
        slot.setCustom(this, userObjectGetter);
        jsResult->Release();
        return true;
    } else {
        ValueImp *kjsValue = toPrimitive(exec);
        if (kjsValue->type() != NullType && kjsValue->type() != UndefinedType) {
            ObjectImp *kjsObject = kjsValue->toObject(exec);
            if (kjsObject->getPropertySlot(exec, propertyName, slot))
                return true;
        }
    }
    return ObjectImp::getOwnPropertySlot(exec, propertyName, slot);
}

void UserObjectImp::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr)
{
    if (!fJSUserObject)
        return;
    
    CFStringRef cfPropName = IdentifierToCFString(propertyName);
    JSUserObject *jsValueObj = KJSValueToJSObject(value, exec);

    fJSUserObject->SetProperty(cfPropName, jsValueObj);

    if (jsValueObj) jsValueObj->Release();
    ReleaseCFType(cfPropName);
}

JSUserObject* UserObjectImp::GetJSUserObject() const
{
    return fJSUserObject;
}

ValueImp *UserObjectImp::toPrimitive(ExecState *exec, Type preferredType) const
{
    ValueImp *result = Undefined();
    JSUserObject* jsObjPtr = KJSValueToJSObject(toObject(exec), exec);
    CFTypeRef cfValue = jsObjPtr ? jsObjPtr->CopyCFValue() : 0;
    if (cfValue) {
        CFTypeID cfType = CFGetTypeID(cfValue);  // toPrimitive
        if (cfValue == GetCFNull()) {
            result = Null();
        }
        else if (cfType == CFBooleanGetTypeID()) {
            if (cfValue == kCFBooleanTrue) {
                result = KJS::Boolean(true);
            } else {
                result = KJS::Boolean(false);
            }
        } else if (cfType == CFStringGetTypeID()) {
            result = KJS::String(CFStringToUString((CFStringRef)cfValue));
        } else if (cfType == CFNumberGetTypeID()) {
            double d = 0.0;
            CFNumberGetValue((CFNumberRef)cfValue, kCFNumberDoubleType, &d);
            result = KJS::Number(d);
        } else if (cfType == CFURLGetTypeID()) {
            CFURLRef absURL = CFURLCopyAbsoluteURL((CFURLRef)cfValue);
            if (absURL) {
                result = KJS::String(CFStringToUString(CFURLGetString(absURL)));
                ReleaseCFType(absURL);
            }
        }
        ReleaseCFType(cfValue);
    }
    if (jsObjPtr)
        jsObjPtr->Release();
    return result;
}


bool UserObjectImp::toBoolean(ExecState *exec) const
{
    bool result = false;
    JSUserObject* jsObjPtr = KJSValueToJSObject(toObject(exec), exec);
    CFTypeRef cfValue = jsObjPtr ? jsObjPtr->CopyCFValue() : 0;
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
    CFTypeRef cfValue = jsObjPtr ? jsObjPtr->CopyCFValue() : 0;
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
    CFTypeRef cfValue = jsObjPtr ? jsObjPtr->CopyCFValue() : 0;
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
            else if (CFNumberCompare(kCFNumberPositiveInfinity, (CFNumberRef)cfValue, 0) == 0)
            {
                result = "Infinity";
            }
            else if (CFNumberCompare(kCFNumberNegativeInfinity, (CFNumberRef)cfValue, 0) == 0)
            {
                result = "-Infinity";
            }
            else
            {
                CFStringRef cfNumStr = 0;
                if (CFNumberIsFloatType((CFNumberRef)cfValue))
                {
                    double d = 0;
                    CFNumberGetValue((CFNumberRef)cfValue, kCFNumberDoubleType, &d);
                    cfNumStr = CFStringCreateWithFormat(0, 0, CFSTR("%f"), (float)d);
                }
                else
                {
                    int i = 0;
                    CFNumberGetValue((CFNumberRef)cfValue, kCFNumberIntType, &i);
                    cfNumStr = CFStringCreateWithFormat(0, 0, CFSTR("%d"), (int)i);
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
    ObjectImp::mark();
    if (fJSUserObject)
        fJSUserObject->Mark();
}
