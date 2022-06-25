/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#import "JavaScriptCore.h"

#if JSC_OBJC_API_ENABLED
#import "APICast.h"
#import "APIUtils.h"
#import "JSAPIWrapperObject.h"
#import "JSCInlines.h"
#import "JSCallbackObject.h"
#import "JSContextInternal.h"
#import "JSWrapperMap.h"
#import "ObjCCallbackFunction.h"
#import "ObjcRuntimeExtras.h"
#import "ObjectConstructor.h"
#import "WeakGCMap.h"
#import "WeakGCMapInlines.h"
#import <wtf/Vector.h>

#if PLATFORM(COCOA)
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#import <mach-o/dyld.h>

#if PLATFORM(APPLETV)
#else
static constexpr int32_t firstJavaScriptCoreVersionWithInitConstructorSupport = 0x21A0400; // 538.4.0
#endif

@class JSObjCClassInfo;

@interface JSWrapperMap () 

- (JSObjCClassInfo*)classInfoForClass:(Class)cls;

@end

static constexpr unsigned InitialBufferSize { 256 };

// Default conversion of selectors to property names.
// All semicolons are removed, lowercase letters following a semicolon are capitalized.
static NSString *selectorToPropertyName(const char* start)
{
    // Use 'index' to check for colons, if there are none, this is easy!
    const char* firstColon = strchr(start, ':');
    if (!firstColon)
        return [NSString stringWithUTF8String:start];

    // 'header' is the length of string up to the first colon.
    size_t header = firstColon - start;
    // The new string needs to be long enough to hold 'header', plus the remainder of the string, excluding
    // at least one ':', but including a '\0'. (This is conservative if there are more than one ':').
    Vector<char, InitialBufferSize> buffer(header + strlen(firstColon + 1) + 1);
    // Copy 'header' characters, set output to point to the end of this & input to point past the first ':'.
    memcpy(buffer.data(), start, header);
    char* output = buffer.data() + header;
    const char* input = start + header + 1;

    // On entry to the loop, we have already skipped over a ':' from the input.
    while (true) {
        char c;
        // Skip over any additional ':'s. We'll leave c holding the next character after the
        // last ':', and input pointing past c.
        while ((c = *(input++)) == ':');
        // Copy the character, converting to upper case if necessary.
        // If the character we copy is '\0', then we're done!
        if (!(*(output++) = toASCIIUpper(c)))
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
    return [NSString stringWithUTF8String:buffer.data()];
}

static bool constructorHasInstance(JSContextRef ctx, JSObjectRef constructorRef, JSValueRef possibleInstance, JSValueRef*)
{
    JSC::JSGlobalObject* globalObject = toJS(ctx);
    JSC::VM& vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);

    JSC::JSObject* constructor = toJS(constructorRef);
    JSC::JSValue instance = toJS(globalObject, possibleInstance);
    return JSC::JSObject::defaultHasInstance(globalObject, instance, constructor->get(globalObject, vm.propertyNames->prototype));
}

static JSC::JSObject* makeWrapper(JSContextRef ctx, JSClassRef jsClass, id wrappedObject)
{
    JSC::JSGlobalObject* globalObject = toJS(ctx);
    JSC::VM& vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);

    ASSERT(jsClass);
    JSC::JSCallbackObject<JSC::JSAPIWrapperObject>* object = JSC::JSCallbackObject<JSC::JSAPIWrapperObject>::create(globalObject, globalObject->objcWrapperObjectStructure(), jsClass, 0);
    object->setWrappedObject((__bridge void*)wrappedObject);
    if (JSC::JSObject* prototype = jsClass->prototype(globalObject))
        object->setPrototypeDirect(vm, prototype);

    return object;
}

// Make an object that is in all ways a completely vanilla JavaScript object,
// other than that it has a native brand set that will be displayed by the default
// Object.prototype.toString conversion.
static JSC::JSObject *objectWithCustomBrand(JSContext *context, NSString *brand, Class cls = 0)
{
    JSClassDefinition definition;
    definition = kJSClassDefinitionEmpty;
    definition.className = [brand UTF8String];
    JSClassRef classRef = JSClassCreate(&definition);
    JSC::JSObject* result = makeWrapper([context JSGlobalContextRef], classRef, cls);
    JSClassRelease(classRef);
    return result;
}

static JSC::JSObject *constructorWithCustomBrand(JSContext *context, NSString *brand, Class cls)
{
    JSClassDefinition definition;
    definition = kJSClassDefinitionEmpty;
    definition.className = [brand UTF8String];
    definition.hasInstance = constructorHasInstance;
    JSClassRef classRef = JSClassCreate(&definition);
    JSC::JSObject* result = makeWrapper([context JSGlobalContextRef], classRef, cls);
    JSClassRelease(classRef);
    return result;
}

// Look for @optional properties in the prototype containing a selector to property
// name mapping, separated by a __JS_EXPORT_AS__ delimiter.
static RetainPtr<NSMutableDictionary> createRenameMap(Protocol *protocol, BOOL isInstanceMethod)
{
    auto renameMap = adoptNS([[NSMutableDictionary alloc] init]);

    forEachMethodInProtocol(protocol, NO, isInstanceMethod, ^(SEL sel, const char*){
        NSString *rename = @(sel_getName(sel));
        NSRange range = [rename rangeOfString:@"__JS_EXPORT_AS__"];
        if (range.location == NSNotFound)
            return;
        NSString *selector = [rename substringToIndex:range.location];
        NSUInteger begin = range.location + range.length;
        NSUInteger length = [rename length] - begin - 1;
        NSString *name = [rename substringWithRange:(NSRange){ begin, length }];
        renameMap.get()[selector] = name;
    });

    return renameMap;
}

inline void putNonEnumerable(JSContext *context, JSValue *base, NSString *propertyName, JSValue *value)
{
    if (![base isObject])
        return;
    JSC::JSGlobalObject* globalObject = toJS([context JSGlobalContextRef]);
    JSC::VM& vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSC::JSObject* baseObject = JSC::asObject(toJS(globalObject, [base JSValueRef]));
    auto name = OpaqueJSString::tryCreate(propertyName);
    if (!name)
        return;

    JSC::PropertyDescriptor descriptor;
    descriptor.setValue(toJS(globalObject, [value JSValueRef]));
    descriptor.setEnumerable(false);
    descriptor.setConfigurable(true);
    descriptor.setWritable(true);
    bool shouldThrow = false;
    baseObject->methodTable()->defineOwnProperty(baseObject, globalObject, name->identifier(&vm), descriptor, shouldThrow);

    JSValueRef exception = 0;
    if (handleExceptionIfNeeded(scope, [context JSGlobalContextRef], &exception) == ExceptionStatus::DidThrow)
        [context valueFromNotifyException:exception];
}

static bool isInitFamilyMethod(NSString *name)
{
    NSUInteger i = 0;

    // Skip over initial underscores.
    for (; i < [name length]; ++i) {
        if ([name characterAtIndex:i] != '_')
            break;
    }

    // Match 'init'.
    NSUInteger initIndex = 0;
    NSString* init = @"init";
    for (; i < [name length] && initIndex < [init length]; ++i, ++initIndex) {
        if ([name characterAtIndex:i] != [init characterAtIndex:initIndex])
            return false;
    }

    // We didn't match all of 'init'.
    if (initIndex < [init length])
        return false;

    // If we're at the end or the next character is a capital letter then this is an init-family selector.
    return i == [name length] || [[NSCharacterSet uppercaseLetterCharacterSet] characterIsMember:[name characterAtIndex:i]]; 
}

static bool shouldSkipMethodWithName(NSString *name)
{
    // For clients that don't support init-based constructors just copy 
    // over the init method as we would have before.
    if (!supportsInitMethodConstructors())
        return false;

    // Skip over init family methods because we handle those specially 
    // for the purposes of hooking up the constructor correctly.
    return isInitFamilyMethod(name);
}

// This method will iterate over the set of required methods in the protocol, and:
//  * Determine a property name (either via a renameMap or default conversion).
//  * If an accessorMap is provided, and contains this name, store the method in the map.
//  * Otherwise, if the object doesn't already contain a property with name, create it.
static void copyMethodsToObject(JSContext *context, Class objcClass, Protocol *protocol, BOOL isInstanceMethod, JSValue *object, NSMutableDictionary *accessorMethods = nil)
{
    auto renameMap = createRenameMap(protocol, isInstanceMethod);

    forEachMethodInProtocol(protocol, YES, isInstanceMethod, ^(SEL sel, const char* types){
        const char* nameCStr = sel_getName(sel);
        NSString *name = @(nameCStr);

        if (shouldSkipMethodWithName(name))
            return;

        if (accessorMethods && accessorMethods[name]) {
            JSObjectRef method = objCCallbackFunctionForMethod(context, objcClass, protocol, isInstanceMethod, sel, types);
            if (!method)
                return;
            accessorMethods[name] = [JSValue valueWithJSValueRef:method inContext:context];
        } else {
            name = renameMap.get()[name];
            if (!name)
                name = selectorToPropertyName(nameCStr);
            JSC::JSGlobalObject* globalObject = toJS([context JSGlobalContextRef]);
            JSValue *existingMethod = object[name];
            // ObjCCallbackFunction does a dynamic lookup for the
            // selector before calling the method. In order to save
            // memory we only put one callback object in any givin
            // prototype chain. However, to handle the client trying
            // to override normal builtins e.g. "toString" we check if
            // the existing value on the prototype chain is an ObjC
            // callback already.
            if ([existingMethod isObject] && JSC::jsDynamicCast<JSC::ObjCCallbackFunction*>(toJS(globalObject, [existingMethod JSValueRef])))
                return;
            JSObjectRef method = objCCallbackFunctionForMethod(context, objcClass, protocol, isInstanceMethod, sel, types);
            if (method)
                putNonEnumerable(context, object, name, [JSValue valueWithJSValueRef:method inContext:context]);
        }
    });
}

struct Property {
    const char* name;
    RetainPtr<NSString> getterName;
    RetainPtr<NSString> setterName;
};

static bool parsePropertyAttributes(objc_property_t objcProperty, Property& property)
{
    bool readonly = false;
    unsigned attributeCount;
    auto attributes = adoptSystem<objc_property_attribute_t[]>(property_copyAttributeList(objcProperty, &attributeCount));
    if (attributeCount) {
        for (unsigned i = 0; i < attributeCount; ++i) {
            switch (*(attributes[i].name)) {
            case 'G':
                property.getterName = @(attributes[i].value);
                break;
            case 'S':
                property.setterName = @(attributes[i].value);
                break;
            case 'R':
                readonly = true;
                break;
            default:
                break;
            }
        }
    }
    return readonly;
}

static RetainPtr<NSString> makeSetterName(const char* name)
{
    size_t nameLength = strlen(name);
    // "set" Name ":\0"  => nameLength + 5.
    Vector<char, 128> buffer(nameLength + 5);
    buffer[0] = 's';
    buffer[1] = 'e';
    buffer[2] = 't';
    buffer[3] = toASCIIUpper(*name);
    memcpy(buffer.data() + 4, name + 1, nameLength - 1);
    buffer[nameLength + 3] = ':';
    buffer[nameLength + 4] = '\0';
    return @(buffer.data());
}

static void copyPrototypeProperties(JSContext *context, Class objcClass, Protocol *protocol, JSValue *prototypeValue)
{
    // First gather propreties into this list, then handle the methods (capturing the accessor methods).
    __block Vector<Property> propertyList;

    // Map recording the methods used as getters/setters.
    NSMutableDictionary *accessorMethods = [NSMutableDictionary dictionary];

    // Useful value.
    JSValue *undefined = [JSValue valueWithUndefinedInContext:context];

    forEachPropertyInProtocol(protocol, ^(objc_property_t objcProperty) {
        const char* name = property_getName(objcProperty);
        Property property { name, nullptr, nullptr };
        bool readonly = parsePropertyAttributes(objcProperty, property);

        // Add the names of the getter & setter methods to
        if (!property.getterName)
            property.getterName = @(name);
        accessorMethods[property.getterName.get()] = undefined;
        if (!readonly) {
            if (!property.setterName)
                property.setterName = makeSetterName(name);
            accessorMethods[property.setterName.get()] = undefined;
        }

        // Add the properties to a list.
        propertyList.append(WTFMove(property));
    });

    // Copy methods to the prototype, capturing accessors in the accessorMethods map.
    copyMethodsToObject(context, objcClass, protocol, YES, prototypeValue, accessorMethods);

    // Iterate the propertyList & generate accessor properties.
    for (auto& property : propertyList) {
        JSValue* getter = accessorMethods[property.getterName.get()];
        ASSERT(![getter isUndefined]);

        JSValue* setter = undefined;
        if (property.setterName) {
            setter = accessorMethods[property.setterName.get()];
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
    Class m_class;
    bool m_block;
    NakedPtr<OpaqueJSClass> m_classRef;
    JSC::Weak<JSC::JSObject> m_prototype;
    JSC::Weak<JSC::JSObject> m_constructor;
    JSC::Weak<JSC::Structure> m_structure;
}

- (instancetype)initForClass:(Class)cls;
- (JSC::JSObject *)wrapperForObject:(id)object inContext:(JSContext *)context;
- (JSC::JSObject *)constructorInContext:(JSContext *)context;
- (JSC::JSObject *)prototypeInContext:(JSContext *)context;

@end

@implementation JSObjCClassInfo

- (instancetype)initForClass:(Class)cls
{
    self = [super init];
    if (!self)
        return nil;

    const char* className = class_getName(cls);
    m_class = cls;
    m_block = [cls isSubclassOfClass:getNSBlockClass()];
    JSClassDefinition definition;
    definition = kJSClassDefinitionEmpty;
    definition.className = className;
    m_classRef = JSClassCreate(&definition);

    return self;
}

- (void)dealloc
{
    JSClassRelease(m_classRef.get());
    [super dealloc];
}

static JSC::JSObject* allocateConstructorForCustomClass(JSContext *context, const char* className, Class cls)
{
    if (!supportsInitMethodConstructors())
        return constructorWithCustomBrand(context, [NSString stringWithFormat:@"%sConstructor", className], cls);

    // For each protocol that the class implements, gather all of the init family methods into a hash table.
    __block HashMap<String, CFTypeRef> initTable;
    Protocol *exportProtocol = getJSExportProtocol();
    for (Class currentClass = cls; currentClass; currentClass = class_getSuperclass(currentClass)) {
        forEachProtocolImplementingProtocol(currentClass, exportProtocol, ^(Protocol *protocol, bool&) {
            forEachMethodInProtocol(protocol, YES, YES, ^(SEL selector, const char*) {
                const char* name = sel_getName(selector);
                if (!isInitFamilyMethod(@(name)))
                    return;
                initTable.set(String::fromLatin1(name), (__bridge CFTypeRef)protocol);
            });
        });
    }

    for (Class currentClass = cls; currentClass; currentClass = class_getSuperclass(currentClass)) {
        __block unsigned numberOfInitsFound = 0;
        __block SEL initMethod = 0;
        __block Protocol *initProtocol = 0;
        __block const char* types = 0;
        forEachMethodInClass(currentClass, ^(Method method) {
            SEL selector = method_getName(method);
            const char* name = sel_getName(selector);
            auto iter = initTable.find(String::fromLatin1(name));

            if (iter == initTable.end())
                return;

            numberOfInitsFound++;
            initMethod = selector;
            initProtocol = (__bridge Protocol *)iter->value;
            types = method_getTypeEncoding(method);
        });

        if (!numberOfInitsFound)
            continue;

        if (numberOfInitsFound > 1) {
            NSLog(@"ERROR: Class %@ exported more than one init family method via JSExport. Class %@ will not have a callable JavaScript constructor function.", cls, cls);
            break;
        }

        JSObjectRef method = objCCallbackFunctionForInit(context, cls, initProtocol, initMethod, types);
        return toJS(method);
    }
    return constructorWithCustomBrand(context, [NSString stringWithFormat:@"%sConstructor", className], cls);
}

typedef std::pair<JSC::JSObject*, JSC::JSObject*> ConstructorPrototypePair;

- (ConstructorPrototypePair)allocateConstructorAndPrototypeInContext:(JSContext *)context
{
    JSObjCClassInfo* superClassInfo = [context.wrapperMap classInfoForClass:class_getSuperclass(m_class)];

    ASSERT(!m_constructor || !m_prototype);
    ASSERT((m_class == [NSObject class]) == !superClassInfo);

    JSC::JSObject* jsPrototype = m_prototype.get();
    JSC::JSObject* jsConstructor = m_constructor.get();

    if (!superClassInfo) {
        JSC::JSGlobalObject* globalObject = toJSGlobalObject([context JSGlobalContextRef]);
        if (!jsConstructor)
            jsConstructor = globalObject->objectConstructor();

        if (!jsPrototype)
            jsPrototype = globalObject->objectPrototype();
    } else {
        const char* className = class_getName(m_class);

        // Create or grab the prototype/constructor pair.
        if (!jsPrototype)
            jsPrototype = objectWithCustomBrand(context, [NSString stringWithFormat:@"%sPrototype", className]);

        if (!jsConstructor)
            jsConstructor = allocateConstructorForCustomClass(context, className, m_class);

        JSValue* prototype = [JSValue valueWithJSValueRef:toRef(jsPrototype) inContext:context];
        JSValue* constructor = [JSValue valueWithJSValueRef:toRef(jsConstructor) inContext:context];

        putNonEnumerable(context, prototype, @"constructor", constructor);
        putNonEnumerable(context, constructor, @"prototype", prototype);

        Protocol *exportProtocol = getJSExportProtocol();
        forEachProtocolImplementingProtocol(m_class, exportProtocol, ^(Protocol *protocol, bool&){
            copyPrototypeProperties(context, m_class, protocol, prototype);
            copyMethodsToObject(context, m_class, protocol, NO, constructor);
        });

        // Set [Prototype].
        JSC::JSObject* superClassPrototype = [superClassInfo prototypeInContext:context];
        JSObjectSetPrototype([context JSGlobalContextRef], toRef(jsPrototype), toRef(superClassPrototype));
    }

    m_prototype = jsPrototype;
    m_constructor = jsConstructor;
    return ConstructorPrototypePair(jsConstructor, jsPrototype);
}

- (JSC::JSObject*)wrapperForObject:(id)object inContext:(JSContext *)context
{
    ASSERT([object isKindOfClass:m_class]);
    ASSERT(m_block == [object isKindOfClass:getNSBlockClass()]);
    if (m_block) {
        if (JSObjectRef method = objCCallbackFunctionForBlock(context, object)) {
            JSValue *constructor = [JSValue valueWithJSValueRef:method inContext:context];
            JSValue *prototype = [JSValue valueWithNewObjectInContext:context];
            putNonEnumerable(context, constructor, @"prototype", prototype);
            putNonEnumerable(context, prototype, @"constructor", constructor);
            return toJS(method);
        }
    }

    JSC::Structure* structure = [self structureInContext:context];

    JSC::JSGlobalObject* globalObject = toJS([context JSGlobalContextRef]);
    JSC::VM& vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);

    auto wrapper = JSC::JSCallbackObject<JSC::JSAPIWrapperObject>::create(globalObject, structure, m_classRef, 0);
    wrapper->setWrappedObject((__bridge void*)object);
    return wrapper;
}

- (JSC::JSObject*)constructorInContext:(JSContext *)context
{
    JSC::JSObject* constructor = m_constructor.get();
    if (!constructor)
        constructor = [self allocateConstructorAndPrototypeInContext:context].first;
    ASSERT(!!constructor);
    return constructor;
}

- (JSC::JSObject*)prototypeInContext:(JSContext *)context
{
    JSC::JSObject* prototype = m_prototype.get();
    if (!prototype)
        prototype = [self allocateConstructorAndPrototypeInContext:context].second;
    ASSERT(!!prototype);
    return prototype;
}

- (JSC::Structure*)structureInContext:(JSContext *)context
{
    JSC::Structure* structure = m_structure.get();
    if (structure)
        return structure;

    JSC::JSGlobalObject* globalObject = toJSGlobalObject([context JSGlobalContextRef]);
    JSC::JSObject* prototype = [self prototypeInContext:context];
    m_structure = JSC::JSCallbackObject<JSC::JSAPIWrapperObject>::createStructure(globalObject->vm(), globalObject, prototype);

    return m_structure.get();
}

@end

@implementation JSWrapperMap {
    RetainPtr<NSMutableDictionary> m_classMap;
    std::unique_ptr<JSC::WeakGCMap<__unsafe_unretained id, JSC::JSObject>> m_cachedJSWrappers;
    RetainPtr<NSMapTable> m_cachedObjCWrappers;
}

- (instancetype)initWithGlobalContextRef:(JSGlobalContextRef)context
{
    self = [super init];
    if (!self)
        return nil;

    NSPointerFunctionsOptions keyOptions = NSPointerFunctionsOpaqueMemory | NSPointerFunctionsOpaquePersonality;
    NSPointerFunctionsOptions valueOptions = NSPointerFunctionsWeakMemory | NSPointerFunctionsObjectPersonality;
    m_cachedObjCWrappers = adoptNS([[NSMapTable alloc] initWithKeyOptions:keyOptions valueOptions:valueOptions capacity:0]);

    m_cachedJSWrappers = makeUnique<JSC::WeakGCMap<__unsafe_unretained id, JSC::JSObject>>(toJS(context)->vm());

    ASSERT(!toJSGlobalObject(context)->wrapperMap());
    toJSGlobalObject(context)->setWrapperMap(self);
    m_classMap = adoptNS([[NSMutableDictionary alloc] init]);
    return self;
}

- (JSObjCClassInfo*)classInfoForClass:(Class)cls
{
    if (!cls)
        return nil;

    // Check if we've already created a JSObjCClassInfo for this Class.
    if (JSObjCClassInfo* classInfo = (JSObjCClassInfo*)m_classMap.get()[(id)cls])
        return classInfo;

    // Skip internal classes beginning with '_' - just copy link to the parent class's info.
    if ('_' == *class_getName(cls)) {
        bool conformsToExportProtocol = false;
        forEachProtocolImplementingProtocol(cls, getJSExportProtocol(), [&conformsToExportProtocol](Protocol *, bool& stop) {
            conformsToExportProtocol = true;
            stop = true;
        });

        if (!conformsToExportProtocol)
            return m_classMap.get()[(id)cls] = [self classInfoForClass:class_getSuperclass(cls)];
    }

    return m_classMap.get()[(id)cls] = adoptNS([[JSObjCClassInfo alloc] initForClass:cls]).get();
}

- (JSValue *)jsWrapperForObject:(id)object inContext:(JSContext *)context
{
    ASSERT(toJSGlobalObject([context JSGlobalContextRef])->wrapperMap() == self);
    JSC::JSObject* jsWrapper = m_cachedJSWrappers->get(object);
    if (jsWrapper)
        return [JSValue valueWithJSValueRef:toRef(jsWrapper) inContext:context];

    if (class_isMetaClass(object_getClass(object)))
        jsWrapper = [[self classInfoForClass:(Class)object] constructorInContext:context];
    else {
        JSObjCClassInfo* classInfo = [self classInfoForClass:[object class]];
        jsWrapper = [classInfo wrapperForObject:object inContext:context];
    }

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=105891
    // This general approach to wrapper caching is pretty effective, but there are a couple of problems:
    // (1) For immortal objects JSValues will effectively leak and this results in error output being logged - we should avoid adding associated objects to immortal objects.
    // (2) A long lived object may rack up many JSValues. When the contexts are released these will unprotect the associated JavaScript objects,
    //     but still, would probably nicer if we made it so that only one associated object was required, broadcasting object dealloc.
    m_cachedJSWrappers->set(object, jsWrapper);
    return [JSValue valueWithJSValueRef:toRef(jsWrapper) inContext:context];
}

- (JSValue *)objcWrapperForJSValueRef:(JSValueRef)value inContext:context
{
    ASSERT(toJSGlobalObject([context JSGlobalContextRef])->wrapperMap() == self);
    auto wrapper = retainPtr((__bridge JSValue *)NSMapGet(m_cachedObjCWrappers.get(), value));
    if (!wrapper) {
        wrapper = adoptNS([[JSValue alloc] initWithValue:value inContext:context]);
        NSMapInsert(m_cachedObjCWrappers.get(), value, (__bridge void*)wrapper.get());
    }
    return wrapper.autorelease();
}

@end

id tryUnwrapObjcObject(JSGlobalContextRef context, JSValueRef value)
{
    if (!JSValueIsObject(context, value))
        return nil;
    JSValueRef exception = 0;
    JSObjectRef object = JSValueToObject(context, value, &exception);
    ASSERT(!exception);
    JSC::JSLockHolder locker(toJS(context));
    if (toJS(object)->inherits<JSC::JSCallbackObject<JSC::JSAPIWrapperObject>>())
        return (__bridge id)JSC::jsCast<JSC::JSAPIWrapperObject*>(toJS(object))->wrappedObject();
    if (id target = tryUnwrapConstructor(object))
        return target;
    return nil;
}

// This class ensures that the JSExport protocol is registered with the runtime.
NS_ROOT_CLASS @interface JSExport <JSExport>
@end
@implementation JSExport
@end

bool supportsInitMethodConstructors()
{
#if PLATFORM(APPLETV)
    // There are no old clients on Apple TV, so there's no need for backwards compatibility.
    return true;
#else
    static const bool supportsInitMethodConstructors = []() -> bool {
        // First check to see the version of JavaScriptCore we directly linked against.
        int32_t versionOfLinkTimeJavaScriptCore = NSVersionOfLinkTimeLibrary("JavaScriptCore");

        // Only do the link time version comparison if we linked directly with JavaScriptCore
        if (versionOfLinkTimeJavaScriptCore != -1)
            return versionOfLinkTimeJavaScriptCore >= firstJavaScriptCoreVersionWithInitConstructorSupport;

        return linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::SupportsInitConstructors);
    }();
    return supportsInitMethodConstructors;
#endif
}

Protocol *getJSExportProtocol()
{
    static Protocol *protocol = objc_getProtocol("JSExport");
    return protocol;
}

Class getNSBlockClass()
{
    static Class cls = objc_getClass("NSBlock");
    return cls;
}

#endif
