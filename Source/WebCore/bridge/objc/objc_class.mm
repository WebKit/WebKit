/*
 * Copyright (C) 2004, 2012 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "objc_class.h"

#import "WebScriptObject.h"
#import "WebScriptObjectProtocol.h"
#import "objc_instance.h"

namespace JSC {
namespace Bindings {

ObjcClass::ObjcClass(ClassStructPtr aClass)
    : _isa(aClass)
{
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

    auto aClass = reinterpret_cast<ObjcClass*>(const_cast<void*>(CFDictionaryGetValue(classesByIsA, (__bridge CFTypeRef)isa)));
    if (!aClass) {
        aClass = new ObjcClass(isa);
        CFDictionaryAddValue(classesByIsA, (__bridge CFTypeRef)isa, aClass);
    }

    return aClass;
}

/*
    By default, a JavaScript method name is produced by concatenating the
    components of an ObjectiveC method name, replacing ':' with '_', and
    escaping '_' and '$' with a leading '$', such that '_' becomes "$_" and
    '$' becomes "$$". For example:

    ObjectiveC name         Default JavaScript name
        moveTo::                moveTo__
        moveTo_                 moveTo$_
        moveTo$_                moveTo$$$_

    This function performs the inverse of that operation.

    @result Fills 'buffer' with the ObjectiveC method name that corresponds to 'JSName'.
*/
typedef Vector<char, 256> JSNameConversionBuffer;
static inline void convertJSMethodNameToObjc(const CString& jsName, JSNameConversionBuffer& buffer)
{
    buffer.reserveInitialCapacity(jsName.length() + 1);

    const char* source = jsName.data();
    while (true) {
        if (*source == '$') {
            ++source;
            buffer.uncheckedAppend(*source);
        } else if (*source == '_')
            buffer.uncheckedAppend(':');
        else
            buffer.uncheckedAppend(*source);

        if (!*source)
            return;

        ++source;
    }
}

Method* ObjcClass::methodNamed(PropertyName propertyName, Instance*) const
{
    String name(propertyName.publicName());
    if (name.isNull())
        return nullptr;

    if (Method* method = m_methodCache.get(name.impl()))
        return method;

    CString jsName = name.ascii();
    JSNameConversionBuffer buffer;
    convertJSMethodNameToObjc(jsName, buffer);
    RetainPtr<CFStringRef> methodName = adoptCF(CFStringCreateWithCString(NULL, buffer.data(), kCFStringEncodingASCII));

    Method* methodPtr = 0;
    ClassStructPtr thisClass = _isa;
    
    while (thisClass && !methodPtr) {
        unsigned numMethodsInClass = 0;
        MethodStructPtr* objcMethodList = class_copyMethodList(thisClass, &numMethodsInClass);
        for (unsigned i = 0; i < numMethodsInClass; i++) {
            MethodStructPtr objcMethod = objcMethodList[i];
            SEL objcMethodSelector = method_getName(objcMethod);
            const char* objcMethodSelectorName = sel_getName(objcMethodSelector);
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

            if ((mappedName && [mappedName isEqual:(__bridge NSString*)methodName.get()]) || !strcmp(objcMethodSelectorName, buffer.data())) {
                auto method = std::make_unique<ObjcMethod>(thisClass, objcMethodSelector);
                methodPtr = method.get();
                m_methodCache.add(name.impl(), WTFMove(method));
                break;
            }
        }
        thisClass = class_getSuperclass(thisClass);
        free(objcMethodList);
    }

    return methodPtr;
}

Field* ObjcClass::fieldNamed(PropertyName propertyName, Instance* instance) const
{
    String name(propertyName.publicName());
    if (name.isNull())
        return nullptr;

    Field* field = m_fieldCache.get(name.impl());
    if (field)
        return field;

    ClassStructPtr thisClass = _isa;

    CString jsName = name.ascii();
    RetainPtr<CFStringRef> fieldName = adoptCF(CFStringCreateWithCString(NULL, jsName.data(), kCFStringEncodingASCII));
    id targetObject = (static_cast<ObjcInstance*>(instance))->getObject();
#if PLATFORM(IOS_FAMILY)
    IGNORE_WARNINGS_BEGIN("undeclared-selector")
    id attributes = [targetObject respondsToSelector:@selector(attributeKeys)] ? [targetObject performSelector:@selector(attributeKeys)] : nil;
    IGNORE_WARNINGS_END
#else
    id attributes = [targetObject attributeKeys];
#endif
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

            if ((mappedName && [mappedName isEqual:(__bridge NSString *)fieldName.get()]) || [keyName isEqual:(__bridge NSString *)fieldName.get()]) {
                auto newField = std::make_unique<ObjcField>((__bridge CFStringRef)keyName);
                field = newField.get();
                m_fieldCache.add(name.impl(), WTFMove(newField));
                break;
            }
        }
    } else {
        // Class doesn't override attributeKeys, so fall back on class runtime
        // introspection.

        while (thisClass) {
            unsigned numFieldsInClass = 0;
            IvarStructPtr* ivarsInClass = class_copyIvarList(thisClass, &numFieldsInClass);

            for (unsigned i = 0; i < numFieldsInClass; i++) {
                IvarStructPtr objcIVar = ivarsInClass[i];
                const char* objcIvarName = ivar_getName(objcIVar);
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

                if ((mappedName && [mappedName isEqual:(__bridge NSString *)fieldName.get()]) || !strcmp(objcIvarName, jsName.data())) {
                    auto newField = std::make_unique<ObjcField>(objcIVar);
                    field = newField.get();
                    m_fieldCache.add(name.impl(), WTFMove(newField));
                    break;
                }
            }

            thisClass = class_getSuperclass(thisClass);
            free(ivarsInClass);
        }
    }

    return field;
}

JSValue ObjcClass::fallbackObject(ExecState* exec, Instance* instance, PropertyName propertyName)
{
    ObjcInstance* objcInstance = static_cast<ObjcInstance*>(instance);
    id targetObject = objcInstance->getObject();
    
    if (![targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)])
        return jsUndefined();

    if (!propertyName.publicName())
        return jsUndefined();

    return ObjcFallbackObjectImp::create(exec, exec->lexicalGlobalObject(), objcInstance, propertyName.publicName());
}

}
}
