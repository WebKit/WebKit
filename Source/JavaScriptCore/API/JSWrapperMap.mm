/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#import "JavaScriptCore.h"

#if JS_OBJC_API_ENABLED

#import "JSContextInternal.h"
#import "JSWrapperMap.h"
#import "ObjCCallbackFunction.h"
#import "ObjcRuntimeExtras.h"
#import <wtf/TCSpinLock.h>
#import "wtf/Vector.h"

static void wrapperFinalize(JSObjectRef object)
{
    [(id)JSObjectGetPrivate(object) release];
}

// All wrapper objects and constructor objects derive from this type, so we can detect & unwrap Objective-C instances/Classes.
static JSClassRef wrapperClass()
{
    static SpinLock initLock = SPINLOCK_INITIALIZER;
    SpinLockHolder lockHolder(&initLock);

    static JSClassRef classRef = 0;

    if (!classRef) {
        JSClassDefinition definition;
        definition = kJSClassDefinitionEmpty;
        definition.className = "objc_class";
        definition.finalize = wrapperFinalize;
        classRef = JSClassCreate(&definition);
    }

    return classRef;
}

// Default conversion of selectors to property names.
// All semicolons are removed, lowercase letters following a semicolon are capitalized.
static NSString* selectorToPropertyName(const char* start)
{
    // Use 'index' to check for colons, if there are non, this is easy!
    const char* firstColon = index(start, ':');
    if (!firstColon)
        return [NSString stringWithUTF8String:start];

    // 'header' is the length of string up to the first colon.
    size_t header = firstColon - start;
    // The new string needs to be long enough to hold 'header', plus the remainder of the string, excluding
    // at least one ':', but including a '\0'. (This is conservative if there are more than one ':').
    char* buffer = (char*)malloc(header + strlen(firstColon + 1) + 1);
    // Copy 'header' characters, set output to point to the end of this & input to point past the first ':'.
    memcpy(buffer, start, header);
    char* output = buffer + header;
    const char* input = start + header + 1;

    // On entry to the loop, we have already skipped over a ':' from the input.
    while (true) {
        char c;
        // Skip over any additional ':'s. We'll leave c holding the next character after the
        // last ':', and input pointing past c.
        while ((c = *(input++)) == ':');
        // Copy the character, converting to upper case if necessary.
        // If the character we copy is '\0', then we're done!
        if (!(*(output++) = toupper(c)))
            goto done;
        // Loop over characters other than ':'.
        while ((c = *(input++)) != ':') {
            // Copy the character.
            // If the character we copy is '\0', then we're done!
            if (!(*(output++) = c))
                goto done;
        }
        // If we get here, we've consumed a ':' - wash, rinse, repeat.
    }
done:
    NSString* result = [NSString stringWithUTF8String:buffer];
    free(buffer);
    return result;
}

// Make an object that is in all ways are completely vanilla JavaScript object,
// other than that it has a native brand set that will be displayed by the default
// Object.prototype.toString comversion.
static JSValue* createObjectWithCustomBrand(JSContext* context, NSString* brand, JSClassRef parentClass = 0, void* privateData = 0)
{
    JSClassDefinition definition;
    definition = kJSClassDefinitionEmpty;
    definition.className = [brand UTF8String];
    definition.parentClass = parentClass;
    JSClassRef classRef = JSClassCreate(&definition);
    JSObjectRef result = JSObjectMake(contextInternalContext(context), classRef, privateData);
    JSClassRelease(classRef);
    return [[JSValue alloc] initWithValue:result inContext:context];
}

// Look for @optional properties in the prototype containing a selector to property
// name mapping, separated by a __JS_EXPORT_AS__ delimiter.
static NSMutableDictionary* createRenameMap(Protocol* protocol, BOOL isInstanceMethod)
{
    NSMutableDictionary* renameMap = [[NSMutableDictionary alloc] init];

    forEachMethodInProtocol(protocol, NO, isInstanceMethod, ^(SEL sel, const char*){
        NSString* rename = @(sel_getName(sel));
        NSRange range = [rename rangeOfString:@"__JS_EXPORT_AS__"];
        if (range.location == NSNotFound)
            return;
        NSString* selector = [rename substringToIndex:range.location];
        NSUInteger begin = range.location + range.length;
        NSUInteger length = [rename length] - begin - 1;
        NSString* name = [rename substringWithRange:(NSRange){ begin, length }];
        renameMap[selector] = name;
    });

    return renameMap;
}

inline void putNonEnumerable(JSValue* base, NSString* propertyName, JSValue* value)
{
    [base defineProperty:propertyName descriptor:@{
        JSPropertyDescriptorValueKey: value,
        JSPropertyDescriptorWritableKey: @YES,
        JSPropertyDescriptorEnumerableKey: @NO,
        JSPropertyDescriptorConfigurableKey: @YES
    }];
}

// This method will iterate over the set of required methods in the protocol, and:
//  * Determine a property name (either via a renameMap or default conversion).
//  * If an accessorMap is provided, and conatins a this name, store the method in the map.
//  * Otherwise, if the object doesn't already conatin a property with name, create it.
static void copyMethodsToObject(JSContext* context, Class objcClass, Protocol* protocol, BOOL isInstanceMethod, JSValue* object, NSMutableDictionary* accessorMethods = nil)
{
    NSMutableDictionary* renameMap = createRenameMap(protocol, isInstanceMethod);

    forEachMethodInProtocol(protocol, YES, isInstanceMethod, ^(SEL sel, const char* types){
        const char* nameCStr = sel_getName(sel);
        NSString* name = @(nameCStr);
        if (accessorMethods && accessorMethods[name]) {
            JSObjectRef method = objCCallbackFunctionForMethod(context, objcClass, protocol, isInstanceMethod, sel, types);
            if (!method)
                return;
            accessorMethods[name] = [JSValue valueWithValue:method inContext:context];
        } else {
            name = renameMap[name];
            if (!name)
                name = selectorToPropertyName(nameCStr);
            if ([object hasProperty:name])
                return;
            JSObjectRef method = objCCallbackFunctionForMethod(context, objcClass, protocol, isInstanceMethod, sel, types);
            if (method)
                putNonEnumerable(object, name, [JSValue valueWithValue:method inContext:context]);
        }
    });

    [renameMap release];
}

static bool parsePropertyAttributes(objc_property_t property, char*& getterName, char*& setterName)
{
    bool readonly = false;
    unsigned attributeCount;
    objc_property_attribute_t* attributes = property_copyAttributeList(property, &attributeCount);
    if (attributeCount) {
        for (unsigned i = 0; i < attributeCount; ++i) {
            switch (*(attributes[i].name)) {
            case 'G':
                getterName = strdup(attributes[i].value);
                break;
            case 'S':
                setterName = strdup(attributes[i].value);
                break;
            case 'R':
                readonly = true;
                break;
            default:
                break;
            }
        }
        free(attributes);
    }
    return readonly;
}

static char* makeSetterName(const char* name)
{
    size_t nameLength = strlen(name);
    char* setterName = (char*)malloc(nameLength + 5); // "set" Name ":\0"
    setterName[0] = 's';
    setterName[1] = 'e';
    setterName[2] = 't';
    setterName[3] = toupper(*name);
    memcpy(setterName + 4, name + 1, nameLength - 1);
    setterName[nameLength + 3] = ':';
    setterName[nameLength + 4] = '\0';
    return setterName;
}

static void copyPrototypeProperties(JSContext* context, Class objcClass, Protocol* protocol, JSValue* prototypeValue)
{
    // First gather propreties into this list, then handle the methods (capturing the accessor methods).
    struct Property {
        const char* name;
        char* getterName;
        char* setterName;
    };
    __block WTF::Vector<Property> propertyList;

    // Map recording the methods used as getters/setters.
    NSMutableDictionary *accessorMethods = [NSMutableDictionary dictionary];

    // Useful value.
    JSValue* undefined = [JSValue valueWithUndefinedInContext:context];

    forEachPropertyInProtocol(protocol, ^(objc_property_t property){
        char* getterName = 0;
        char* setterName = 0;
        bool readonly = parsePropertyAttributes(property, getterName, setterName);
        const char* name = property_getName(property);

        // Add the names of the getter & setter methods to 
        if (!getterName)
            getterName = strdup(name);
        accessorMethods[@(getterName)] = undefined;
        if (!readonly) {
            if (!setterName)
                setterName = makeSetterName(name);
            accessorMethods[@(setterName)] = undefined;
        }

        // Add the properties to a list.
        propertyList.append((Property){ name, getterName, setterName });
    });

    // Copy methods to the prototype, capturing accessors in the accessorMethods map.
    copyMethodsToObject(context, objcClass, protocol, YES, prototypeValue, accessorMethods);

    // Iterate the propertyList & generate accessor properties.
    for (size_t i = 0; i < propertyList.size(); ++i) {
        Property& property = propertyList[i];

        JSValue* getter = accessorMethods[@(property.getterName)];
        free(property.getterName);
        ASSERT(![getter isUndefined]);

        JSValue* setter = undefined;
        if (property.setterName) {
            setter = accessorMethods[@(property.setterName)];
            free(property.setterName);
            ASSERT(![setter isUndefined]);
        }
        
        [prototypeValue defineProperty:@(property.name) descriptor:@{
            JSPropertyDescriptorGetKey: getter,
            JSPropertyDescriptorSetKey: setter,
            JSPropertyDescriptorEnumerableKey: @NO,
            JSPropertyDescriptorConfigurableKey: @YES
        }];
    }
}

@interface JSObjCClassInfo : NSObject {
    JSContext* m_context;
    Class m_class;
    bool m_block;
    JSClassRef m_classRef;
    JSValue* m_prototype;
    JSValue* m_constructor;
}

- (id)initWithContext:(JSContext*)context forClass:(Class)cls superClassInfo:(JSObjCClassInfo*)superClassInfo;
- (JSValue *)wrapperForObject:(id)object;
- (JSValue *)constructor;

@end

@implementation JSObjCClassInfo

- (id)initWithContext:(JSContext*)context forClass:(Class)cls superClassInfo:(JSObjCClassInfo*)superClassInfo
{
    [super init];

    const char* className = class_getName(cls);
    m_context = context;
    m_class = cls;
    m_block = [cls isSubclassOfClass:getNSBlockClass()];
    JSClassDefinition definition;
    definition = kJSClassDefinitionEmpty;
    definition.className = className;
    definition.parentClass = wrapperClass();
    m_classRef = JSClassCreate(&definition);

    ASSERT((cls == [NSObject class]) == !superClassInfo);
    if (!superClassInfo) {
        m_constructor = [context[@"Object"] retain];
        m_prototype = [m_constructor[@"prototype"] retain];
    } else {
        // Create the prototype/constructor pair.
        m_prototype = createObjectWithCustomBrand(context, [NSString stringWithFormat:@"%sPrototype", className]);
        m_constructor = createObjectWithCustomBrand(context, [NSString stringWithFormat:@"%sConstructor", className], wrapperClass(), [cls retain]);
        putNonEnumerable(m_prototype, @"constructor", m_constructor);
        putNonEnumerable(m_constructor, @"prototype", m_prototype);

        Protocol* exportProtocol = getJSExportProtocol();
        forEachProtocolImplementingProtocol(cls, exportProtocol, ^(Protocol* protocol){
            copyPrototypeProperties(context, cls, protocol, m_prototype);
            copyMethodsToObject(context, cls, protocol, NO, m_constructor);
        });

        // Set [Prototype].
        m_prototype[@"__proto__"] = superClassInfo->m_prototype;
    }

    return self;
}

- (void)dealloc
{
    JSClassRelease(m_classRef);
    [m_prototype release];
    [m_constructor release];
    [super dealloc];
}

- (JSValue *)wrapperForObject:(id)object
{
    ASSERT([object isKindOfClass:m_class]);
    ASSERT(m_block == [object isKindOfClass:getNSBlockClass()]);
    if (m_block) {
        if (JSObjectRef method = objCCallbackFunctionForBlock(m_context, object))
            return [JSValue valueWithValue:method inContext:m_context];
    }

    JSValueRef prototypeValue = valueInternalValue(m_prototype);
    ASSERT(JSValueIsObject(contextInternalContext(m_context), prototypeValue));
    JSObjectRef prototype = JSValueToObject(contextInternalContext(m_context), prototypeValue, 0);

    JSObjectRef wrapper = JSObjectMake(contextInternalContext(m_context), m_classRef, [object retain]);
    JSObjectSetPrototype(contextInternalContext(m_context), wrapper, prototype);
    return [JSValue valueWithValue:wrapper inContext:m_context];
}

- (JSValue *)constructor
{
    return m_constructor;
}

@end

@implementation JSWrapperMap {
    JSContext *m_context;
    NSMutableDictionary* m_classMap;
}

- (id)initWithContext:(JSContext*)context
{
    [super init];
    m_context = context;
    m_classMap = [[NSMutableDictionary alloc] init];
    return self;
}

- (void)dealloc
{
    [m_classMap release];
    [super dealloc];
}

- (JSObjCClassInfo*)classInfoForClass:(Class)cls
{
    if (!cls)
        return nil;

    // Check if we've already created a JSObjCClassInfo for this Class.
    if (JSObjCClassInfo* classInfo = (JSObjCClassInfo*)m_classMap[cls])
        return classInfo;

    // Skip internal classes begining with '_' - just copy link to the parent class's info.
    if ('_' == *class_getName(cls))
        return m_classMap[cls] = [self classInfoForClass:class_getSuperclass(cls)];

    return m_classMap[cls] = [[[JSObjCClassInfo alloc] initWithContext:m_context forClass:cls superClassInfo:[self classInfoForClass:class_getSuperclass(cls)]] autorelease];
}

- (JSValue *)wrapperForObject:(id)object
{
    JSValue* wrapper = objc_getAssociatedObject(object, m_context);
    if (wrapper && wrapper.context)
        return wrapper;

    if (class_isMetaClass(object_getClass(object)))
        wrapper = [[self classInfoForClass:(Class)object] constructor];
    else {
        JSObjCClassInfo* classInfo = [self classInfoForClass:[object class]];
        wrapper = [classInfo wrapperForObject:object];
    }

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=105891
    // This general approach to wrapper caching is pretty effective, but there are a couple of problems:
    // (1) For immortal objects JSValues will effectively leaj and this results in error output being logged - we should avoid adding associated objects to immortal objects.
    // (2) A long lived object may rack up many JSValues. When the contexts are released these will unproctect the associated JavaScript objects,
    //     but still, would probably nicer if we made it so that only one associated object was required, broadcasting object dealloc.
    objc_setAssociatedObject(object, m_context, wrapper, OBJC_ASSOCIATION_RETAIN);
    return wrapper;
}

@end

id tryUnwrapObjcObject(JSGlobalContextRef context, JSValueRef value)
{
    if (!JSValueIsObject(context, value))
        return nil;
    JSValueRef exception = 0;
    JSObjectRef object = JSValueToObject(context, value, &exception);
    //ASSERT(!exception);
    if (JSValueIsObjectOfClass(context, object, wrapperClass()))
        return (id)JSObjectGetPrivate(object);
    if (id target = tryUnwrapBlock(context, object))
        return target;
    return nil;
}

Protocol* getJSExportProtocol()
{
    static Protocol* protocol = 0;
    if (!protocol)
        protocol = objc_getProtocol("JSExport");
    return protocol;
}

Class getNSBlockClass()
{
    static Class cls = 0;
    if (!cls)
        cls = objc_getClass("NSBlock");
    return cls;
}

#endif
