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
 */

#include "config.h"
#include "objc_class.h"

#include "objc_instance.h"
#include "WebScriptObject.h"

namespace KJS {
namespace Bindings {
    
ObjcClass::ObjcClass(ClassStructPtr aClass)
{
    _isa = aClass;
    _methods = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &MethodDictionaryValueCallBacks);
    _fields = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &FieldDictionaryValueCallBacks);
}

ObjcClass::~ObjcClass() {
    CFRelease(_fields);
    CFRelease(_methods);
}

static CFMutableDictionaryRef classesByIsA = 0;

static void _createClassesByIsAIfNecessary()
{
    if (!classesByIsA)
        classesByIsA = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
}

ObjcClass* ObjcClass::classForIsA(ClassStructPtr isa)
{
    _createClassesByIsAIfNecessary();

    ObjcClass* aClass = (ObjcClass*)CFDictionaryGetValue(classesByIsA, isa);
    if (!aClass) {
        aClass = new ObjcClass(isa);
        CFDictionaryAddValue(classesByIsA, isa, aClass);
    }

    return aClass;
}

const char* ObjcClass::name() const
{
    return object_getClassName(_isa);
}

MethodList ObjcClass::methodsNamed(const char* JSName, Instance*) const
{
    MethodList methodList;
    char fixedSizeBuffer[1024];
    char* buffer = fixedSizeBuffer;

    if (!convertJSMethodNameToObjc(JSName, buffer, sizeof(fixedSizeBuffer))) {
        int length = strlen(JSName) + 1;
        buffer = new char[length];
        if (!buffer || !convertJSMethodNameToObjc(JSName, buffer, length))
            return methodList;
    }

    CFStringRef methodName = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingASCII);
    Method* method = (Method*)CFDictionaryGetValue(_methods, methodName);
    if (method) {
        CFRelease(methodName);
        methodList.addMethod(method);
        return methodList;
    }

    ClassStructPtr thisClass = _isa;
    while (thisClass && methodList.length() < 1) {
#if defined(OBJC_API_VERSION) && OBJC_API_VERSION >= 2
        unsigned numMethodsInClass = 0;
        MethodStructPtr* objcMethodList = class_copyMethodList(thisClass, &numMethodsInClass);
#else
        void* iterator = 0;
        struct objc_method_list* objcMethodList;
        while ((objcMethodList = class_nextMethodList(thisClass, &iterator))) {
            unsigned numMethodsInClass = objcMethodList->method_count;
#endif
            for (unsigned i = 0; i < numMethodsInClass; i++) {
#if defined(OBJC_API_VERSION) && OBJC_API_VERSION >= 2
                MethodStructPtr objcMethod = objcMethodList[i];
                SEL objcMethodSelector = method_getName(objcMethod);
                const char* objcMethodSelectorName = sel_getName(objcMethodSelector);
#else
                struct objc_method* objcMethod = &objcMethodList->method_list[i];
                SEL objcMethodSelector = objcMethod->method_name;
                const char* objcMethodSelectorName = (const char*)objcMethodSelector;
#endif
                NSString* mappedName = nil;

                // See if the class wants to exclude the selector from visibility in JavaScript.
                if ([thisClass respondsToSelector:@selector(isSelectorExcludedFromWebScript:)])
                    if ([thisClass isSelectorExcludedFromWebScript:objcMethodSelector])
                        continue;

                // See if the class want to provide a different name for the selector in JavaScript.
                // Note that we do not do any checks to guarantee uniqueness. That's the responsiblity
                // of the class.
                if ([thisClass respondsToSelector:@selector(webScriptNameForSelector:)])
                    mappedName = [thisClass webScriptNameForSelector:objcMethodSelector];

                if ((mappedName && [mappedName isEqual:(NSString*)methodName]) || strcmp(objcMethodSelectorName, buffer) == 0) {
                    Method* aMethod = new ObjcMethod(thisClass, objcMethodSelectorName); // deleted when the dictionary is destroyed
                    CFDictionaryAddValue(_methods, methodName, aMethod);
                    methodList.addMethod(aMethod);
                    break;
                }
            }
#if defined(OBJC_API_VERSION) && OBJC_API_VERSION >= 2
            thisClass = class_getSuperclass(thisClass);
            free(objcMethodList);
#else
        }
        thisClass = thisClass->super_class;
#endif
    }

    CFRelease(methodName);
    if (buffer != fixedSizeBuffer)
        delete [] buffer;

    return methodList;
}

Field* ObjcClass::fieldNamed(const char* name, Instance* instance) const
{
    ClassStructPtr thisClass = _isa;

    CFStringRef fieldName = CFStringCreateWithCString(NULL, name, kCFStringEncodingASCII);
    Field* aField = (Field*)CFDictionaryGetValue(_fields, fieldName);
    if (aField) {
        CFRelease(fieldName);
        return aField;
    }

    id targetObject = (static_cast<ObjcInstance*>(instance))->getObject();
    id attributes = [targetObject attributeKeys];
    if (attributes) {
        // Class overrides attributeKeys, use that array of key names.
        unsigned count = [attributes count];
        for (unsigned i = 0; i < count; i++) {
            NSString* keyName = [attributes objectAtIndex:i];
            const char* UTF8KeyName = [keyName UTF8String]; // ObjC actually only supports ASCII names.

            // See if the class wants to exclude the selector from visibility in JavaScript.
            if ([thisClass respondsToSelector:@selector(isKeyExcludedFromWebScript:)])
                if ([thisClass isKeyExcludedFromWebScript:UTF8KeyName])
                    continue;

            // See if the class want to provide a different name for the selector in JavaScript.
            // Note that we do not do any checks to guarantee uniqueness. That's the responsiblity
            // of the class.
            NSString* mappedName = nil;
            if ([thisClass respondsToSelector:@selector(webScriptNameForKey:)])
                mappedName = [thisClass webScriptNameForKey:UTF8KeyName];

            if ((mappedName && [mappedName isEqual:(NSString*)fieldName]) || [keyName isEqual:(NSString*)fieldName]) {
                aField = new ObjcField((CFStringRef)keyName); // deleted when the dictionary is destroyed
                CFDictionaryAddValue(_fields, fieldName, aField);
                break;
            }
        }
    } else {
        // Class doesn't override attributeKeys, so fall back on class runtime
        // introspection.

        while (thisClass) {
#if defined(OBJC_API_VERSION) && OBJC_API_VERSION >= 2
            unsigned numFieldsInClass = 0;
            IvarStructPtr* ivarsInClass = class_copyIvarList(thisClass, &numFieldsInClass);
#else
            struct objc_ivar_list* fieldsInClass = thisClass->ivars;
            if (fieldsInClass) {
                unsigned numFieldsInClass = fieldsInClass->ivar_count;
#endif
                for (unsigned i = 0; i < numFieldsInClass; i++) {
#if defined(OBJC_API_VERSION) && OBJC_API_VERSION >= 2
                    IvarStructPtr objcIVar = ivarsInClass[i];
                    const char* objcIvarName = ivar_getName(objcIVar);
#else
                    IvarStructPtr objcIVar = &fieldsInClass->ivar_list[i];
                    const char* objcIvarName = objcIVar->ivar_name;
#endif
                    NSString* mappedName = 0;

                    // See if the class wants to exclude the selector from visibility in JavaScript.
                    if ([thisClass respondsToSelector:@selector(isKeyExcludedFromWebScript:)])
                        if ([thisClass isKeyExcludedFromWebScript:objcIvarName])
                            continue;

                    // See if the class want to provide a different name for the selector in JavaScript.
                    // Note that we do not do any checks to guarantee uniqueness. That's the responsiblity
                    // of the class.
                    if ([thisClass respondsToSelector:@selector(webScriptNameForKey:)])
                        mappedName = [thisClass webScriptNameForKey:objcIvarName];

                    if ((mappedName && [mappedName isEqual:(NSString*)fieldName]) || strcmp(objcIvarName, name) == 0) {
                        aField = new ObjcField(objcIVar); // deleted when the dictionary is destroyed
                        CFDictionaryAddValue(_fields, fieldName, aField);
                        break;
                    }
                }
#if defined(OBJC_API_VERSION) && OBJC_API_VERSION >= 2
            thisClass = class_getSuperclass(thisClass);
            free(ivarsInClass);
#else
            }
            thisClass = thisClass->super_class;
#endif
        }

        CFRelease(fieldName);
    }

    return aField;
}

JSValue* ObjcClass::fallbackObject(ExecState*, Instance* instance, const Identifier &propertyName)
{
    ObjcInstance* objcInstance = static_cast<ObjcInstance*>(instance);
    id targetObject = objcInstance->getObject();
    
    if (![targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)])
        return jsUndefined();
    return new ObjcFallbackObjectImp(objcInstance, propertyName);
}

}
}
