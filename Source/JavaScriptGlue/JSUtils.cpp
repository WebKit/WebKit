/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
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
#include "JSUtils.h"

#include "JSBase.h"
#include "JSObject.h"
#include "JSRun.h"
#include "JSValueWrapper.h"
#include "UserObjectImp.h"
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/PropertyNameArray.h>
#include <JavaScriptCore/WTFThreadData.h>

struct ObjectImpList {
    JSObject* imp;
    ObjectImpList* next;
    CFTypeRef data;
};

static CFTypeRef KJSValueToCFTypeInternal(JSValue inValue, ExecState *exec, ObjectImpList* inImps);
static JSGlueGlobalObject* getThreadGlobalObject();

//--------------------------------------------------------------------------
// CFStringToUString
//--------------------------------------------------------------------------

UString CFStringToUString(CFStringRef inCFString)
{
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
    return CFStringCreateWithCharacters(0, (const UniChar*)inUString.characters(), inUString.length());
}


//--------------------------------------------------------------------------
// CFStringToIdentifier
//--------------------------------------------------------------------------

Identifier CFStringToIdentifier(CFStringRef inCFString, ExecState* exec)
{
    return Identifier(exec, CFStringToUString(inCFString));
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
JSUserObject* KJSValueToJSObject(JSValue inValue, ExecState *exec)
{
    JSUserObject* result = 0;

    if (inValue.inherits(&UserObjectImp::info)) {
        UserObjectImp* userObjectImp = static_cast<UserObjectImp *>(asObject(inValue));
        result = userObjectImp->GetJSUserObject();
        if (result)
            result->Retain();
    } else {
        JSValueWrapper* wrapperValue = new JSValueWrapper(inValue);
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
JSValue JSObjectKJSValue(JSUserObject* ptr)
{
    JSGlueAPIEntry entry;

    JSValue result = jsUndefined();
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
                        result = jsString(getThreadGlobalExecState(), CFStringToUString((CFStringRef)cfType));
                        handled = true;
                    }
                    else if (typeID == CFNumberGetTypeID())
                    {
                        double num;
                        CFNumberGetValue((CFNumberRef)cfType, kCFNumberDoubleType, &num);
                        result = jsNumber(num);
                        handled = true;
                    }
                    else if (typeID == CFBooleanGetTypeID())
                    {
                        result = jsBoolean(CFBooleanGetValue((CFBooleanRef)cfType));
                        handled = true;
                    }
                    else if (typeID == CFNullGetTypeID())
                    {
                        result = jsNull();
                        handled = true;
                    }
                }
                break;
            }
        }
        if (!handled)
        {
            ExecState* exec = getThreadGlobalExecState();
            result = new (exec) UserObjectImp(getThreadGlobalObject()->userObjectStructure(), ptr);
        }
    }
    return result;
}




//--------------------------------------------------------------------------
// KJSValueToCFTypeInternal
//--------------------------------------------------------------------------
// Caller is responsible for releasing the returned CFTypeRef
CFTypeRef KJSValueToCFTypeInternal(JSValue inValue, ExecState *exec, ObjectImpList* inImps)
{
    if (!inValue)
        return 0;

    CFTypeRef result = 0;

    JSGlueAPIEntry entry;

        if (inValue.isBoolean())
            {
                result = inValue.toBoolean(exec) ? kCFBooleanTrue : kCFBooleanFalse;
                RetainCFType(result);
                return result;
            }

        if (inValue.isString())
            {
                UString uString = inValue.toString(exec);
                result = UStringToCFString(uString);
                return result;
            }

        if (inValue.isNumber())
            {
                double number1 = inValue.toNumber(exec);
                double number2 = (double)inValue.toInteger(exec);
                if (number1 ==  number2)
                {
                    int intValue = (int)number2;
                    result = CFNumberCreate(0, kCFNumberIntType, &intValue);
                }
                else
                {
                    result = CFNumberCreate(0, kCFNumberDoubleType, &number1);
                }
                return result;
            }

        if (inValue.isObject())
            {
                if (inValue.inherits(&UserObjectImp::info)) {
                    UserObjectImp* userObjectImp = static_cast<UserObjectImp *>(asObject(inValue));
                    JSUserObject* ptr = userObjectImp->GetJSUserObject();
                    if (ptr)
                    {
                        result = ptr->CopyCFValue();
                    }
                }
                else
                {
                    JSObject *object = inValue.toObject(exec);
                    UInt8 isArray = false;

                    // if two objects reference each
                    JSObject* imp = object;
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
                        JSGlueGlobalObject* globalObject = static_cast<JSGlueGlobalObject*>(exec->dynamicGlobalObject());
                        if (globalObject && (globalObject->Flags() & kJSFlagConvertAssociativeArray)) {
                            PropertyNameArray propNames(exec);
                            object->getPropertyNames(exec, propNames);
                            PropertyNameArray::const_iterator iter = propNames.begin();
                            PropertyNameArray::const_iterator end = propNames.end();
                            while(iter != end && isArray)
                            {
                                Identifier propName = *iter;
                                UString ustr = propName.ustring();
                                const UniChar* uniChars = (const UniChar*)ustr.characters();
                                int size = ustr.length();
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
                        unsigned int length = object->get(exec, Identifier(exec, "length")).toUInt32(exec);
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
                        PropertyNameArray propNames(exec);
                        object->getPropertyNames(exec, propNames);
                        {
                            result = CFDictionaryCreateMutable(0,
                                                               0,
                                                               &kCFTypeDictionaryKeyCallBacks,
                                                               &kCFTypeDictionaryValueCallBacks);
                            if (result)
                            {
                                PropertyNameArray::const_iterator iter = propNames.begin();
                                PropertyNameArray::const_iterator end = propNames.end();
                                while(iter != end)
                                {
                                    Identifier propName = *iter;
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
                return result;
            }

    if (inValue.isUndefinedOrNull())
        {
            result = RetainCFType(GetCFNull());
            return result;
        }

    ASSERT_NOT_REACHED();
    return 0;
}

CFTypeRef KJSValueToCFType(JSValue inValue, ExecState *exec)
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

/*
 * This is a slight hack. The JSGlue API has no concept of execution state.
 * However, execution state is an inherent part of JS, and JSCore requires it.
 * So, we keep a single execution state for the whole thread and supply it
 * where necessary.

 * The execution state holds two things: (1) exceptions; (2) the global object. 
 * JSGlue has no API for accessing exceptions, so we just discard them. As for
 * the global object, JSGlue includes no calls that depend on it. Its property
 * getters and setters are per-object; they don't walk up the enclosing scope. 
 * Functions called by JSObjectCallFunction may reference values in the enclosing 
 * scope, but they do so through an internally stored scope chain, so we don't 
 * need to supply the global scope.
 */      

static pthread_key_t globalObjectKey;
static pthread_once_t globalObjectKeyOnce = PTHREAD_ONCE_INIT;

static void unprotectGlobalObject(void* data) 
{
    JSGlueAPIEntry entry;
    gcUnprotect(static_cast<JSGlueGlobalObject*>(data));
}

static void initializeGlobalObjectKey()
{
    pthread_key_create(&globalObjectKey, unprotectGlobalObject);
}

JSGlobalData* getThreadGlobalData()
{
    return &JSGlobalData::sharedInstance();
}

static JSGlueGlobalObject* getThreadGlobalObject()
{
    pthread_once(&globalObjectKeyOnce, initializeGlobalObjectKey);
    JSGlueGlobalObject* globalObject = static_cast<JSGlueGlobalObject*>(pthread_getspecific(globalObjectKey));
    if (!globalObject) {
        globalObject = new (getThreadGlobalData()) JSGlueGlobalObject(JSGlueGlobalObject::createStructure(jsNull()));
        gcProtect(globalObject);
        pthread_setspecific(globalObjectKey, globalObject);
    }
    return globalObject;
}

ExecState* getThreadGlobalExecState()
{
    ExecState* exec = getThreadGlobalObject()->globalExec();

    // Discard exceptions -- otherwise an exception would forestall JS 
    // evaluation throughout the thread
    exec->clearException();
    return exec;
}

JSGlueAPIEntry::JSGlueAPIEntry()
    : m_lock(LockForReal)
    , m_storedIdentifierTable(wtfThreadData().currentIdentifierTable())
{
    wtfThreadData().setCurrentIdentifierTable(getThreadGlobalData()->identifierTable);
}

JSGlueAPIEntry::~JSGlueAPIEntry()
{
    wtfThreadData().setCurrentIdentifierTable(m_storedIdentifierTable);
}

JSGlueAPICallback::JSGlueAPICallback(ExecState* exec)
    : m_dropLocks(exec)
{
    wtfThreadData().resetCurrentIdentifierTable();
}

JSGlueAPICallback::~JSGlueAPICallback()
{
    wtfThreadData().setCurrentIdentifierTable(getThreadGlobalData()->identifierTable);
}
