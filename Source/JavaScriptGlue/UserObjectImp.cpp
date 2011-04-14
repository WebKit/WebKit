/*
 * Copyright (C) 2005, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "UserObjectImp.h"

#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/PropertyNameArray.h>

const ClassInfo UserObjectImp::s_info = { "UserObject", &JSNonFinalObject::s_info, 0, 0 };

UserObjectImp::UserObjectImp(JSGlobalData& globalData, Structure* structure, JSUserObject* userObject)
    : JSNonFinalObject(globalData, structure)
    , fJSUserObject((JSUserObject*)userObject->Retain())
{
}

UserObjectImp::~UserObjectImp()
{
    if (fJSUserObject)
        fJSUserObject->Release();
}

CallType UserObjectImp::getCallData(CallData& callData)
{
    return fJSUserObject ? fJSUserObject->getCallData(callData) : CallTypeNone;
}

JSValue UserObjectImp::callAsFunction(ExecState *exec)
{
    JSValue result = jsUndefined();
    JSUserObject* jsThisObj = KJSValueToJSObject(exec->hostThisValue().toThisObject(exec), exec);
    if (jsThisObj) {
        CFIndex argCount = exec->argumentCount();
        CFArrayCallBacks arrayCallBacks;
        JSTypeGetCFArrayCallBacks(&arrayCallBacks);
        CFMutableArrayRef jsArgs = CFArrayCreateMutable(0, 0, &arrayCallBacks);
        if (jsArgs) {
            for (CFIndex i = 0; i < argCount; i++) {
                JSUserObject* jsArg = KJSValueToJSObject(exec->argument(i), exec);
                CFArrayAppendValue(jsArgs, (void*)jsArg);
                jsArg->Release();
            }
        }

        JSUserObject* jsResult;
        { // scope
            JSGlueAPICallback apiCallback(exec);

            // getCallData should have guarded against a NULL fJSUserObject.
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


void UserObjectImp::getOwnPropertyNames(ExecState *exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSUserObject* ptr = GetJSUserObject();
    if (ptr) {
        CFArrayRef cfPropertyNames = ptr->CopyPropertyNames();
        if (cfPropertyNames) {
            CFIndex count = CFArrayGetCount(cfPropertyNames);
            CFIndex i;
            for (i = 0; i < count; i++) {
                CFStringRef propertyName = (CFStringRef)CFArrayGetValueAtIndex(cfPropertyNames, i);
                propertyNames.add(CFStringToIdentifier(propertyName, exec));
            }
            CFRelease(cfPropertyNames);
        }
    }
    JSObject::getOwnPropertyNames(exec, propertyNames, mode);
}

JSValue UserObjectImp::userObjectGetter(ExecState*, JSValue slotBase, const Identifier& propertyName)
{
    UserObjectImp *thisObj = static_cast<UserObjectImp *>(asObject(slotBase));
    // getOwnPropertySlot should have guarded against a null fJSUserObject.
    assert(thisObj->fJSUserObject);
    
    CFStringRef cfPropName = IdentifierToCFString(propertyName);
    JSUserObject *jsResult = thisObj->fJSUserObject->CopyProperty(cfPropName);
    ReleaseCFType(cfPropName);
    JSValue result = JSObjectKJSValue(jsResult);
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
        JSValue kjsValue = toPrimitive(exec);
        if (!kjsValue.isUndefinedOrNull()) {
            JSObject* kjsObject = kjsValue.toObject(exec);
            if (kjsObject->getPropertySlot(exec, propertyName, slot))
                return true;
        }
    }
    return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}

void UserObjectImp::put(ExecState *exec, const Identifier &propertyName, JSValue value, PutPropertySlot&)
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

JSValue UserObjectImp::toPrimitive(ExecState *exec, PreferredPrimitiveType) const
{
    JSValue result = jsUndefined();
    JSUserObject* jsObjPtr = KJSValueToJSObject(toObject(exec, exec->lexicalGlobalObject()), exec);
    CFTypeRef cfValue = jsObjPtr ? jsObjPtr->CopyCFValue() : 0;
    if (cfValue) {
        CFTypeID cfType = CFGetTypeID(cfValue);  // toPrimitive
        if (cfValue == GetCFNull()) {
            result = jsNull();
        }
        else if (cfType == CFBooleanGetTypeID()) {
            if (cfValue == kCFBooleanTrue) {
                result = jsBoolean(true);
            } else {
                result = jsBoolean(false);
            }
        } else if (cfType == CFStringGetTypeID()) {
            result = jsString(exec, CFStringToUString((CFStringRef)cfValue));
        } else if (cfType == CFNumberGetTypeID()) {
            double d = 0.0;
            CFNumberGetValue((CFNumberRef)cfValue, kCFNumberDoubleType, &d);
            result = jsNumber(d);
        } else if (cfType == CFURLGetTypeID()) {
            CFURLRef absURL = CFURLCopyAbsoluteURL((CFURLRef)cfValue);
            if (absURL) {
                result = jsString(exec, CFStringToUString(CFURLGetString(absURL)));
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
    JSUserObject* jsObjPtr = KJSValueToJSObject(toObject(exec, exec->lexicalGlobalObject()), exec);
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
    JSUserObject* jsObjPtr = KJSValueToJSObject(toObject(exec, exec->lexicalGlobalObject()), exec);
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
    JSUserObject* jsObjPtr = KJSValueToJSObject(toObject(exec, exec->lexicalGlobalObject()), exec);
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
                CFStringRef cfNumStr;
                double d = 0;
                CFNumberGetValue((CFNumberRef)cfValue, kCFNumberDoubleType, &d);
                if (CFNumberIsFloatType((CFNumberRef)cfValue))
                {
                    cfNumStr = CFStringCreateWithFormat(0, 0, CFSTR("%f"), d);
                }
                else
                {
                    cfNumStr = CFStringCreateWithFormat(0, 0, CFSTR("%.0f"), d);
                }
                result = CFStringToUString(cfNumStr);
                ReleaseCFType(cfNumStr);
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

void UserObjectImp::markChildren(MarkStack& markStack)
{
    JSObject::markChildren(markStack);
    if (fJSUserObject)
        fJSUserObject->Mark();
}
