/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#import "JSExportMacros.h"
#import <JavaScriptCore/JavaScriptCore.h>

#import "CurrentThisInsideBlockGetterTest.h"
#import "DateTests.h"
#import "JSCast.h"
#import "JSContextPrivate.h"
#import "JSExportTests.h"
#import "JSScript.h"
#import "JSValuePrivate.h"
#import "JSVirtualMachineInternal.h"
#import "JSVirtualMachinePrivate.h"
#import "JSWrapperMapTests.h"
#import "Regress141275.h"
#import "Regress141809.h"
#import <wtf/SafeStrerror.h>
#import <wtf/spi/darwin/DataVaultSPI.h>


#if PLATFORM(COCOA)
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#if __has_include(<libproc.h>)
#define HAS_LIBPROC 1
#import <libproc.h>
#else
#define HAS_LIBPROC 0
#endif
#import <pthread.h>
#import <vector>
#import <wtf/MemoryFootprint.h>
#import <wtf/DataLog.h>
#import <wtf/RetainPtr.h>

extern "C" void JSSynchronousGarbageCollectForDebugging(JSContextRef);
extern "C" void JSSynchronousEdenCollectForDebugging(JSContextRef);

extern "C" bool _Block_has_signature(id);
extern "C" const char * _Block_signature(id);

extern int failed;
extern "C" void testObjectiveCAPI(const char*);
extern "C" void checkResult(NSString *, bool);

#if JSC_OBJC_API_ENABLED

@interface UnexportedObject : NSObject
@end

@implementation UnexportedObject
@end

@protocol ParentObject <JSExport>
@end

@interface ParentObject : NSObject<ParentObject>
+ (NSString *)parentTest;
@end

@implementation ParentObject
+ (NSString *)parentTest
{
    return [self description];
}
@end

@protocol TestObject <JSExport>
- (id)init;
@property int variable;
@property (readonly) int six;
@property CGPoint point;
+ (NSString *)classTest;
+ (NSString *)parentTest;
- (NSString *)getString;
JSExportAs(testArgumentTypes,
- (NSString *)testArgumentTypesWithInt:(int)i double:(double)d boolean:(BOOL)b string:(NSString *)s number:(NSNumber *)n array:(NSArray *)a dictionary:(NSDictionary *)o
);
- (void)callback:(JSValue *)function;
- (void)bogusCallback:(void(^)(int))function;
@end

@interface TestObject : ParentObject <TestObject>
@property int six;
+ (id)testObject;
@end

@implementation TestObject
@synthesize variable;
@synthesize six;
@synthesize point;
+ (id)testObject
{
    return [[TestObject alloc] init];
}
+ (NSString *)classTest
{
    return @"classTest - okay";
}
- (NSString *)getString
{
    return @"42";
}
- (NSString *)testArgumentTypesWithInt:(int)i double:(double)d boolean:(BOOL)b string:(NSString *)s number:(NSNumber *)n array:(NSArray *)a dictionary:(NSDictionary *)o
{
    return [NSString stringWithFormat:@"%d,%g,%d,%@,%d,%@,%@", i, d, b==YES?true:false,s,[n intValue],a[1],o[@"x"]];
}
- (void)callback:(JSValue *)function
{
    [function callWithArguments:@[@(42)]];
}
- (void)bogusCallback:(void(^)(int))function
{
    function(42);
}
@end

bool testXYZTested = false;

@protocol TextXYZ <JSExport>
- (id)initWithString:(NSString*)string;
@property int x;
@property (readonly) int y;
@property (assign) JSValue *onclick;
@property (assign) JSValue *weakOnclick;
- (void)test:(NSString *)message;
@end

@interface TextXYZ : NSObject <TextXYZ>
@property int x;
@property int y;
@property int z;
- (void)click;
@end

@implementation TextXYZ {
    JSManagedValue *m_weakOnclickHandler;
    JSManagedValue *m_onclickHandler;
}
@synthesize x;
@synthesize y;
@synthesize z;
- (id)initWithString:(NSString*)string
{
    self = [super init];
    if (!self)
        return nil;

    NSLog(@"%@", string);

    return self;
}
- (void)test:(NSString *)message
{
    testXYZTested = [message isEqual:@"test"] && x == 13 & y == 4 && z == 5;
}
- (void)setWeakOnclick:(JSValue *)value
{
    m_weakOnclickHandler = [JSManagedValue managedValueWithValue:value];
}

- (void)setOnclick:(JSValue *)value
{
    m_onclickHandler = [JSManagedValue managedValueWithValue:value];
    [value.context.virtualMachine addManagedReference:m_onclickHandler withOwner:self];
}
- (JSValue *)weakOnclick
{
    return [m_weakOnclickHandler value];
}
- (JSValue *)onclick
{
    return [m_onclickHandler value];
}
- (void)click
{
    if (!m_onclickHandler)
        return;

    JSValue *function = [m_onclickHandler value];
    [function callWithArguments:@[]];
}
@end

@class TinyDOMNode;

@protocol TinyDOMNode <JSExport>
- (void)appendChild:(TinyDOMNode *)child;
- (NSUInteger)numberOfChildren;
- (TinyDOMNode *)childAtIndex:(NSUInteger)index;
- (void)removeChildAtIndex:(NSUInteger)index;
@end

@interface TinyDOMNode : NSObject<TinyDOMNode>
@end

@implementation TinyDOMNode {
    NSMutableArray *m_children;
    RetainPtr<JSVirtualMachine> m_sharedVirtualMachine;
}

- (id)initWithVirtualMachine:(JSVirtualMachine *)virtualMachine
{
    self = [super init];
    if (!self)
        return nil;

    m_children = [[NSMutableArray alloc] initWithCapacity:0];
    m_sharedVirtualMachine = virtualMachine;

    return self;
}

- (void)appendChild:(TinyDOMNode *)child
{
    [m_sharedVirtualMachine addManagedReference:child withOwner:self];
    [m_children addObject:child];
}

- (NSUInteger)numberOfChildren
{
    return [m_children count];
}

- (TinyDOMNode *)childAtIndex:(NSUInteger)index
{
    if (index >= [m_children count])
        return nil;
    return [m_children objectAtIndex:index];
}

- (void)removeChildAtIndex:(NSUInteger)index
{
    if (index >= [m_children count])
        return;
    [m_sharedVirtualMachine removeManagedReference:[m_children objectAtIndex:index] withOwner:self];
    [m_children removeObjectAtIndex:index];
}

@end

@interface JSCollection : NSObject
- (void)setValue:(JSValue *)value forKey:(NSString *)key;
- (JSValue *)valueForKey:(NSString *)key;
@end

@implementation JSCollection {
    NSMutableDictionary *_dict;
}
- (id)init
{
    self = [super init];
    if (!self)
        return nil;

    _dict = [[NSMutableDictionary alloc] init];

    return self;
}

- (void)setValue:(JSValue *)value forKey:(NSString *)key
{
    JSManagedValue *oldManagedValue = [_dict objectForKey:key];
    if (oldManagedValue) {
        JSValue* oldValue = [oldManagedValue value];
        if (oldValue)
            [oldValue.context.virtualMachine removeManagedReference:oldManagedValue withOwner:self];
    }
    JSManagedValue *managedValue = [JSManagedValue managedValueWithValue:value];
    [value.context.virtualMachine addManagedReference:managedValue withOwner:self];
    [_dict setObject:managedValue forKey:key];
}

- (JSValue *)valueForKey:(NSString *)key
{
    JSManagedValue *managedValue = [_dict objectForKey:key];
    if (!managedValue)
        return nil;
    return [managedValue value];
}
@end

@protocol InitA <JSExport>
- (id)initWithA:(int)a;
- (int)initialize;
@end

@protocol InitB <JSExport>
- (id)initWithA:(int)a b:(int)b;
@end

@protocol InitC <JSExport>
- (id)_init;
@end

@interface ClassA : NSObject<InitA>
@end

@interface ClassB : ClassA<InitB>
@end

@interface ClassC : ClassB<InitA, InitB>
@end

@interface ClassCPrime : ClassB<InitA, InitC>
@end

@interface ClassD : NSObject<InitA>
- (id)initWithA:(int)a;
@end

@interface ClassE : ClassD
- (id)initWithA:(int)a;
@end

@implementation ClassA {
    int _a;
}
- (id)initWithA:(int)a
{
    self = [super init];
    if (!self)
        return nil;

    _a = a;

    return self;
}
- (int)initialize
{
    return 42;
}
@end

@implementation ClassB {
    int _b;
}
- (id)initWithA:(int)a b:(int)b
{
    self = [super initWithA:a];
    if (!self)
        return nil;

    _b = b;

    return self;
}
@end

@implementation ClassC {
    int _c;
}
- (id)initWithA:(int)a
{
    return [self initWithA:a b:0];
}
- (id)initWithA:(int)a b:(int)b
{
    self = [super initWithA:a b:b];
    if (!self)
        return nil;

    _c = a + b;

    return self;
}
@end

@implementation ClassCPrime
- (id)initWithA:(int)a
{
    self = [super initWithA:a b:0];
    if (!self)
        return nil;
    return self;
}
- (id)_init
{
    return [self initWithA:42];
}
@end

@implementation ClassD

- (id)initWithA:(int)a
{
    self = nil;
    return [[ClassE alloc] initWithA:a];
}
- (int)initialize
{
    return 0;
}
@end

@implementation ClassE {
    int _a;
}

- (id)initWithA:(int)a
{
    self = [super init];
    if (!self)
        return nil;

    _a = a;

    return self;
}
@end

static bool evilAllocationObjectWasDealloced = false;

@interface EvilAllocationObject : NSObject
- (JSValue *)doEvilThingsWithContext:(JSContext *)context;
@end

@implementation EvilAllocationObject {
    JSContext *m_context;
}
- (id)initWithContext:(JSContext *)context
{
    self = [super init];
    if (!self)
        return nil;

    m_context = context;

    return self;
}
- (void)dealloc
{
    [self doEvilThingsWithContext:m_context];
    evilAllocationObjectWasDealloced = true;
#if !__has_feature(objc_arc)
    [super dealloc];
#endif
}

- (JSValue *)doEvilThingsWithContext:(JSContext *)context
{
    JSValue *result = [context evaluateScript:@" \
        (function() { \
            var a = []; \
            var sum = 0; \
            for (var i = 0; i < 10000; ++i) { \
                sum += i; \
                a[i] = sum; \
            } \
            return sum; \
        })()"];

    JSSynchronousGarbageCollectForDebugging([context JSGlobalContextRef]);
    return result;
}
@end

extern "C" void checkResult(NSString *description, bool passed)
{
    NSLog(@"TEST: \"%@\": %@", description, passed ? @"PASSED" : @"FAILED");
    if (!passed)
        failed = 1;
}

static bool blockSignatureContainsClass()
{
    static bool containsClass = ^{
        id block = ^(NSString *string){ return string; };
        return _Block_has_signature(block) && strstr(_Block_signature(block), "NSString");
    }();
    return containsClass;
}

static void* threadMain(void* contextPtr)
{
    JSContext *context = (__bridge JSContext*)contextPtr;

    // Do something to enter the VM.
    TestObject *testObject = [TestObject testObject];
    context[@"testObject"] = testObject;
    return nullptr;
}

static void* multiVMThreadMain(void* okPtr)
{
    bool& ok = *static_cast<bool*>(okPtr);
    JSVirtualMachine *vm = [[JSVirtualMachine alloc] init];
    JSContext* context = [[JSContext alloc] initWithVirtualMachine:vm];
    [context evaluateScript:
        @"var array = [{}];\n"
         "for (var i = 0; i < 20; ++i) {\n"
         "    var newArray = new Array(array.length * 2);\n"
         "    for (var j = 0; j < newArray.length; ++j)\n"
         "        newArray[j] = {parent: array[j / 2]};\n"
         "    array = newArray;\n"
         "}\n"];
    if (context.exception) {
        NSLog(@"Uncaught exception.\n");
        ok = false;
    }
    if (![context.globalObject valueForProperty:@"array"].toObject) {
        NSLog(@"Did not find \"array\" variable.\n");
        ok = false;
    }
    JSSynchronousGarbageCollectForDebugging([context JSGlobalContextRef]);
    return nullptr;
}

static void runJITThreadLimitTests()
{
#if ENABLE(DFG_JIT)
    auto testDFG = [] {
        unsigned defaultNumberOfThreads = JSC::Options::numberOfDFGCompilerThreads();
        unsigned targetNumberOfThreads = 1;
        unsigned initialNumberOfThreads = [JSVirtualMachine setNumberOfDFGCompilerThreads:targetNumberOfThreads];
        checkResult(@"Initial number of DFG threads should be the value provided through Options", initialNumberOfThreads == defaultNumberOfThreads);
        unsigned updatedNumberOfThreads = [JSVirtualMachine setNumberOfDFGCompilerThreads:initialNumberOfThreads];
        checkResult(@"Number of DFG threads should have been updated", updatedNumberOfThreads == targetNumberOfThreads);
    };

    auto testFTL = [] {
        unsigned defaultNumberOfThreads = JSC::Options::numberOfFTLCompilerThreads();
        unsigned targetNumberOfThreads = 3;
        unsigned initialNumberOfThreads = [JSVirtualMachine setNumberOfFTLCompilerThreads:targetNumberOfThreads];
        checkResult(@"Initial number of FTL threads should be the value provided through Options", initialNumberOfThreads == defaultNumberOfThreads);
        unsigned updatedNumberOfThreads = [JSVirtualMachine setNumberOfFTLCompilerThreads:initialNumberOfThreads];
        checkResult(@"Number of FTL threads should have been updated", updatedNumberOfThreads == targetNumberOfThreads);
    };

    testDFG();
    testFTL();
#endif // ENABLE(DFG_JIT)
}

static void testObjectiveCAPIMain()
{
    @autoreleasepool {
        JSValue *value = [JSValue valueWithJSValueRef:nil inContext:nil];
        checkResult(@"nil JSValue creation", !value);
    }

    @autoreleasepool {
        JSVirtualMachine* vm = [[JSVirtualMachine alloc] init];
        JSContext* context = [[JSContext alloc] initWithVirtualMachine:vm];
        [context evaluateScript:@"bad"];
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *result = [context evaluateScript:@"2 + 2"];
        checkResult(@"2 + 2", result.isNumber && [result toInt32] == 4);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        NSString *result = [NSString stringWithFormat:@"Two plus two is %@", [context evaluateScript:@"2 + 2"]];
        checkResult(@"stringWithFormat", [result isEqual:@"Two plus two is 4"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"message"] = @"Hello";
        JSValue *result = [context evaluateScript:@"message + ', World!'"];
        checkResult(@"Hello, World!", result.isString && [result isEqualToObject:@"Hello, World!"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        checkResult(@"Promise is exposed", ![context[@"Promise"] isUndefined]);
        JSValue *result = [context evaluateScript:@"typeof Promise"];
        checkResult(@"typeof Promise is 'function'", result.isString && [result isEqualToObject:@"function"]);
    }

    @autoreleasepool {
        JSVirtualMachine* vm = [[JSVirtualMachine alloc] init];
        JSContext* context = [[JSContext alloc] initWithVirtualMachine:vm];
        [context evaluateScript:@"result = 0; Promise.resolve(42).then(function (value) { result = value; });"];
        checkResult(@"Microtask is drained", [context[@"result"]  isEqualToObject:@42]);
    }

    @autoreleasepool {
        JSVirtualMachine* vm = [[JSVirtualMachine alloc] init];
        JSContext* context = [[JSContext alloc] initWithVirtualMachine:vm];
        TestObject* testObject = [TestObject testObject];
        context[@"testObject"] = testObject;
        [context evaluateScript:@"result = 0; callbackResult = 0; Promise.resolve(42).then(function (value) { result = value; }); callbackResult = testObject.getString();"];
        checkResult(@"Microtask is drained with same VM", [context[@"result"]  isEqualToObject:@42] && [context[@"callbackResult"] isEqualToObject:@"42"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *result = [context evaluateScript:@"({ x:42 })"];
        checkResult(@"({ x:42 })", result.isObject && [result[@"x"] isEqualToObject:@42]);
        id obj = [result toObject];
        checkResult(@"Check dictionary literal", [obj isKindOfClass:[NSDictionary class]]);
        id num = (NSDictionary *)obj[@"x"];
        checkResult(@"Check numeric literal", [num isKindOfClass:[NSNumber class]]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *result = [context evaluateScript:@"[ ]"];
        checkResult(@"[ ]", result.isArray);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *result = [context evaluateScript:@"new Date"];
        checkResult(@"new Date", result.isDate);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *symbol = [context evaluateScript:@"Symbol('dope');"];
        JSValue *notSymbol = [context evaluateScript:@"'dope'"];
        checkResult(@"Should be a symbol value", symbol.isSymbol);
        checkResult(@"Should not be a symbol value", !notSymbol.isSymbol);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *symbol = [JSValue valueWithNewSymbolFromDescription:@"dope" inContext:context];
        checkResult(@"Should be a created from Obj-C", symbol.isSymbol);
    }

// Older platforms ifdef the type of some selectors so these tests don't work.
// FIXME: Remove this when we stop building for macOS 10.14/iOS 12.
#if (defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || (defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000)

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *arrayIterator = [context evaluateScript:@"Array.prototype[Symbol.iterator]"];
        JSValue *iteratorSymbol = context[@"Symbol"][@"iterator"];
        JSValue *array = [JSValue valueWithNewArrayInContext:context];
        checkResult(@"Looking up by subscript with symbol should work", [array[iteratorSymbol] isEqual:arrayIterator]);
        checkResult(@"Looking up by method with symbol should work", [[array valueForProperty:iteratorSymbol] isEqual:arrayIterator]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *iteratorSymbol = context[@"Symbol"][@"iterator"];
        JSValue *object = [JSValue valueWithNewObjectInContext:context];
        JSValue *theAnswer = [JSValue valueWithUInt32:42 inContext:context];
        object[iteratorSymbol] = theAnswer;
        checkResult(@"Setting by subscript with symbol should work", [object[iteratorSymbol] isEqual:theAnswer]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *iteratorSymbol = context[@"Symbol"][@"iterator"];
        JSValue *object = [JSValue valueWithNewObjectInContext:context];
        JSValue *theAnswer = [JSValue valueWithUInt32:42 inContext:context];
        [object setValue:theAnswer forProperty:iteratorSymbol];
        checkResult(@"Setting by method with symbol should work", [object[iteratorSymbol] isEqual:theAnswer]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *iteratorSymbol = context[@"Symbol"][@"iterator"];
        JSValue *object = [JSValue valueWithNewObjectInContext:context];
        JSValue *theAnswer = [JSValue valueWithUInt32:42 inContext:context];
        object[iteratorSymbol] = theAnswer;
        checkResult(@"has property with symbol should work", [object hasProperty:iteratorSymbol]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *iteratorSymbol = context[@"Symbol"][@"iterator"];
        JSValue *object = [JSValue valueWithNewObjectInContext:context];
        JSValue *theAnswer = [JSValue valueWithUInt32:42 inContext:context];
        checkResult(@"delete property with symbol should work without property", [object deleteProperty:iteratorSymbol]);
        object[iteratorSymbol] = theAnswer;
        checkResult(@"delete property with symbol should work with property", [object deleteProperty:iteratorSymbol]);
        checkResult(@"delete should be false with non-configurable property", ![context[@"Array"] deleteProperty:@"prototype"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *object = [JSValue valueWithNewObjectInContext:context];
        NSObject *objCObject = [[NSObject alloc] init];
        JSValue *result = object[objCObject];
        checkResult(@"getting a non-convertable object should return undefined", [result isUndefined]);
        object[objCObject] = @(1);
        result = object[objCObject];
        checkResult(@"getting a non-convertable object should return the stored value", [result toUInt32] == 1);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *object = [JSValue valueWithNewObjectInContext:context];
        JSValue *iteratorSymbol = context[@"Symbol"][@"iterator"];
        object[@"value"] = @(1);
        context[@"object"] = object;

        object[iteratorSymbol] = ^{
            JSValue *result = [JSValue valueWithNewObjectInContext:context];
            result[@"object"] = [JSContext currentThis];
            result[@"next"] = ^{
                JSValue *result = [JSValue valueWithNewObjectInContext:context];
                JSValue *value = [JSContext currentThis][@"object"][@"value"];
                [[JSContext currentThis][@"object"] deleteProperty:@"value"];
                result[@"value"] = value;
                result[@"done"] = [JSValue valueWithBool:[value isUndefined] inContext:context];
                return result;
            };
            return result;
        };


        JSValue *count = [context evaluateScript:@"let count = 0; for (let v of object) { if (v !== 1) throw new Error(); count++; } count;"];
        checkResult(@"iterator should not throw", ![context exception]);
        checkResult(@"iteration count should be 1", [count toUInt32] == 1);
    }

#endif

    @autoreleasepool {
        JSCollection* myPrivateProperties = [[JSCollection alloc] init];

        @autoreleasepool {
            JSContext* context = [[JSContext alloc] init];
            TestObject* rootObject = [TestObject testObject];
            context[@"root"] = rootObject;
            [context.virtualMachine addManagedReference:myPrivateProperties withOwner:rootObject];
            [myPrivateProperties setValue:[JSValue valueWithBool:true inContext:context] forKey:@"is_ham"];
            [myPrivateProperties setValue:[JSValue valueWithObject:@"hello!" inContext:context] forKey:@"message"];
            [myPrivateProperties setValue:[JSValue valueWithInt32:42 inContext:context] forKey:@"my_number"];
            [myPrivateProperties setValue:[JSValue valueWithNullInContext:context] forKey:@"definitely_null"];
            [myPrivateProperties setValue:[JSValue valueWithUndefinedInContext:context] forKey:@"not_sure_if_undefined"];

            JSSynchronousGarbageCollectForDebugging([context JSGlobalContextRef]);

            JSValue *isHam = [myPrivateProperties valueForKey:@"is_ham"];
            JSValue *message = [myPrivateProperties valueForKey:@"message"];
            JSValue *myNumber = [myPrivateProperties valueForKey:@"my_number"];
            JSValue *definitelyNull = [myPrivateProperties valueForKey:@"definitely_null"];
            JSValue *notSureIfUndefined = [myPrivateProperties valueForKey:@"not_sure_if_undefined"];
            checkResult(@"is_ham is true", isHam.isBoolean && [isHam toBool]);
            checkResult(@"message is hello!", message.isString && [@"hello!" isEqualToString:[message toString]]);
            checkResult(@"my_number is 42", myNumber.isNumber && [myNumber toInt32] == 42);
            checkResult(@"definitely_null is null", definitelyNull.isNull);
            checkResult(@"not_sure_if_undefined is undefined", notSureIfUndefined.isUndefined);
        }

        checkResult(@"is_ham is nil", ![myPrivateProperties valueForKey:@"is_ham"]);
        checkResult(@"message is nil", ![myPrivateProperties valueForKey:@"message"]);
        checkResult(@"my_number is 42", ![myPrivateProperties valueForKey:@"my_number"]);
        checkResult(@"definitely_null is null", ![myPrivateProperties valueForKey:@"definitely_null"]);
        checkResult(@"not_sure_if_undefined is undefined", ![myPrivateProperties valueForKey:@"not_sure_if_undefined"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *message = [JSValue valueWithObject:@"hello" inContext:context];
        TestObject *rootObject = [TestObject testObject];
        JSCollection *collection = [[JSCollection alloc] init];
        context[@"root"] = rootObject;
        @autoreleasepool {
            JSValue *jsCollection = [JSValue valueWithObject:collection inContext:context];
            JSManagedValue *weakCollection = [JSManagedValue managedValueWithValue:jsCollection andOwner:rootObject];
            [context.virtualMachine addManagedReference:weakCollection withOwner:message];
            JSSynchronousGarbageCollectForDebugging([context JSGlobalContextRef]);
        }
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        __block int result;
        context[@"blockCallback"] = ^(int value){
            result = value;
        };
        [context evaluateScript:@"blockCallback(42)"];
        checkResult(@"blockCallback", result == 42);
    }

    if (blockSignatureContainsClass()) {
        @autoreleasepool {
            JSContext *context = [[JSContext alloc] init];
            __block bool result = false;
            context[@"blockCallback"] = ^(NSString *value){
                result = [@"42" isEqualToString:value] == YES;
            };
            [context evaluateScript:@"blockCallback(42)"];
            checkResult(@"blockCallback(NSString *)", result);
        }
    } else
        NSLog(@"Skipping 'blockCallback(NSString *)' test case");

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        checkResult(@"!context.exception", !context.exception);
        [context evaluateScript:@"!@#$%^&*() THIS IS NOT VALID JAVASCRIPT SYNTAX !@#$%^&*()"];
        checkResult(@"context.exception", context.exception);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        __block bool caught = false;
        context.exceptionHandler = ^(JSContext *context, JSValue *exception) {
            (void)context;
            (void)exception;
            caught = true;
        };
        [context evaluateScript:@"!@#$%^&*() THIS IS NOT VALID JAVASCRIPT SYNTAX !@#$%^&*()"];
        checkResult(@"JSContext.exceptionHandler", caught);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        __block int expectedExceptionLineNumber = 1;
        __block bool sawExpectedExceptionLineNumber = false;
        context.exceptionHandler = ^(JSContext *, JSValue *exception) {
            sawExpectedExceptionLineNumber = [exception[@"line"] toInt32] == expectedExceptionLineNumber;
        };
        [context evaluateScript:@"!@#$%^&*() THIS IS NOT VALID JAVASCRIPT SYNTAX !@#$%^&*()"];
        checkResult(@"evaluteScript exception on line 1", sawExpectedExceptionLineNumber);

        expectedExceptionLineNumber = 2;
        sawExpectedExceptionLineNumber = false;
        [context evaluateScript:@"// Line 1\n!@#$%^&*() THIS IS NOT VALID JAVASCRIPT SYNTAX !@#$%^&*()"];
        checkResult(@"evaluteScript exception on line 2", sawExpectedExceptionLineNumber);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        __block bool emptyExceptionSourceURL = false;
        context.exceptionHandler = ^(JSContext *, JSValue *exception) {
            emptyExceptionSourceURL = exception[@"sourceURL"].isUndefined;
        };
        [context evaluateScript:@"!@#$%^&*() THIS IS NOT VALID JAVASCRIPT SYNTAX !@#$%^&*()"];
        checkResult(@"evaluteScript: exception has no sourceURL", emptyExceptionSourceURL);

        __block NSString *exceptionSourceURL = nil;
        context.exceptionHandler = ^(JSContext *, JSValue *exception) {
            exceptionSourceURL = [exception[@"sourceURL"] toString];
        };
        NSURL *url = [NSURL fileURLWithPath:@"/foo/bar.js" isDirectory:NO];
        [context evaluateScript:@"!@#$%^&*() THIS IS NOT VALID JAVASCRIPT SYNTAX !@#$%^&*()" withSourceURL:url];
        checkResult(@"evaluateScript:withSourceURL: exception has expected sourceURL", [exceptionSourceURL isEqualToString:[url absoluteString]]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"callback"] = ^{
            JSContext *context = [JSContext currentContext];
            context.exception = [JSValue valueWithNewErrorFromMessage:@"Something went wrong." inContext:context];
        };
        JSValue *result = [context evaluateScript:@"var result; try { callback(); } catch (e) { result = 'Caught exception'; }"];
        checkResult(@"Explicit throw in callback - was caught by JavaScript", [result isEqualToObject:@"Caught exception"]);
        checkResult(@"Explicit throw in callback - not thrown to Objective-C", !context.exception);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"callback"] = ^{
            JSContext *context = [JSContext currentContext];
            [context evaluateScript:@"!@#$%^&*() THIS IS NOT VALID JAVASCRIPT SYNTAX !@#$%^&*()"];
        };
        JSValue *result = [context evaluateScript:@"var result; try { callback(); } catch (e) { result = 'Caught exception'; }"];
        checkResult(@"Implicit throw in callback - was caught by JavaScript", [result isEqualToObject:@"Caught exception"]);
        checkResult(@"Implicit throw in callback - not thrown to Objective-C", !context.exception);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        [context evaluateScript:
            @"function sum(array) { \
                var result = 0; \
                for (var i in array) \
                    result += array[i]; \
                return result; \
            }"];
        JSValue *array = [JSValue valueWithObject:@[@13, @2, @7] inContext:context];
        JSValue *sumFunction = context[@"sum"];
        JSValue *result = [sumFunction callWithArguments:@[ array ]];
        checkResult(@"sum([13, 2, 7])", [result toInt32] == 22);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *mulAddFunction = [context evaluateScript:
            @"(function(array, object) { \
                var result = []; \
                for (var i in array) \
                    result.push(array[i] * object.x + object.y); \
                return result; \
            })"];
        JSValue *result = [mulAddFunction callWithArguments:@[ @[ @2, @4, @8 ], @{ @"x":@0.5, @"y":@42 } ]];
        checkResult(@"mulAddFunction", result.isObject && [[result toString] isEqual:@"43,44,46"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];        
        JSValue *array = [JSValue valueWithNewArrayInContext:context];
        checkResult(@"arrayLengthEmpty", [[array[@"length"] toNumber] unsignedIntegerValue] == 0);
        JSValue *value1 = [JSValue valueWithInt32:42 inContext:context];
        JSValue *value2 = [JSValue valueWithInt32:24 inContext:context];
        NSUInteger lowIndex = 5;
        NSUInteger maxLength = UINT_MAX;

        [array setValue:value1 atIndex:lowIndex];
        checkResult(@"array.length after put to low index", [[array[@"length"] toNumber] unsignedIntegerValue] == (lowIndex + 1));

        [array setValue:value1 atIndex:(maxLength - 1)];
        checkResult(@"array.length after put to maxLength - 1", [[array[@"length"] toNumber] unsignedIntegerValue] == maxLength);

        [array setValue:value2 atIndex:maxLength];
        checkResult(@"array.length after put to maxLength", [[array[@"length"] toNumber] unsignedIntegerValue] == maxLength);

        [array setValue:value2 atIndex:(maxLength + 1)];
        checkResult(@"array.length after put to maxLength + 1", [[array[@"length"] toNumber] unsignedIntegerValue] == maxLength);

        if (sizeof(NSUInteger) == 8)
            checkResult(@"valueAtIndex:0 is undefined", [array valueAtIndex:0].isUndefined);
        else
            checkResult(@"valueAtIndex:0", [[array valueAtIndex:0] toInt32] == 24);
        checkResult(@"valueAtIndex:lowIndex", [[array valueAtIndex:lowIndex] toInt32] == 42);
        checkResult(@"valueAtIndex:maxLength - 1", [[array valueAtIndex:(maxLength - 1)] toInt32] == 42);
        checkResult(@"valueAtIndex:maxLength", [[array valueAtIndex:maxLength] toInt32] == 24);
        checkResult(@"valueAtIndex:maxLength + 1", [[array valueAtIndex:(maxLength + 1)] toInt32] == 24);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *object = [JSValue valueWithNewObjectInContext:context];

        object[@"point"] = @{ @"x":@1, @"y":@2 };
        object[@"point"][@"x"] = @3;
        CGPoint point = [object[@"point"] toPoint];
        checkResult(@"toPoint", point.x == 3 && point.y == 2);

        object[@{ @"toString":^{ return @"foo"; } }] = @"bar";
        checkResult(@"toString in object literal used as subscript", [[object[@"foo"] toString] isEqual:@"bar"]);

        object[[@"foobar" substringToIndex:3]] = @"bar";
        checkResult(@"substring used as subscript", [[object[@"foo"] toString] isEqual:@"bar"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TextXYZ *testXYZ = [[TextXYZ alloc] init];
        context[@"testXYZ"] = testXYZ;
        testXYZ.x = 3;
        testXYZ.y = 4;
        testXYZ.z = 5;
        [context evaluateScript:@"testXYZ.x = 13; testXYZ.y = 14;"];
        [context evaluateScript:@"testXYZ.test('test')"];
        checkResult(@"TextXYZ - testXYZTested", testXYZTested);
        JSValue *result = [context evaluateScript:@"testXYZ.x + ',' + testXYZ.y + ',' + testXYZ.z"];
        checkResult(@"TextXYZ - result", [result isEqualToObject:@"13,4,undefined"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        [context[@"Object"][@"prototype"] defineProperty:@"getterProperty" descriptor:@{
            JSPropertyDescriptorGetKey:^{
                return [JSContext currentThis][@"x"];
            }
        }];
        JSValue *object = [JSValue valueWithObject:@{ @"x":@101 } inContext:context];
        int result = [object [@"getterProperty"] toInt32];
        checkResult(@"getterProperty", result == 101);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"concatenate"] = ^{
            NSArray *arguments = [JSContext currentArguments];
            if (![arguments count])
                return @"";
            NSString *message = [arguments[0] description];
            for (NSUInteger index = 1; index < [arguments count]; ++index)
                message = [NSString stringWithFormat:@"%@ %@", message, arguments[index]];
            return message;
        };
        JSValue *result = [context evaluateScript:@"concatenate('Hello,', 'World!')"];
        checkResult(@"concatenate", [result isEqualToObject:@"Hello, World!"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"foo"] = @YES;
        checkResult(@"@YES is boolean", [context[@"foo"] isBoolean]);
        JSValue *result = [context evaluateScript:@"typeof foo"];
        checkResult(@"@YES is boolean", [result isEqualToObject:@"boolean"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *result = [context evaluateScript:@"String(console)"];
        checkResult(@"String(console)", [result isEqualToObject:@"[object console]"]);
        result = [context evaluateScript:@"typeof console.log"];
        checkResult(@"typeof console.log", [result isEqualToObject:@"function"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject* testObject = [TestObject testObject];
        context[@"testObject"] = testObject;
        JSValue *result = [context evaluateScript:@"String(testObject)"];
        checkResult(@"String(testObject)", [result isEqualToObject:@"[object TestObject]"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject* testObject = [TestObject testObject];
        context[@"testObject"] = testObject;
        JSValue *result = [context evaluateScript:@"String(testObject.__proto__)"];
        checkResult(@"String(testObject.__proto__)", [result isEqualToObject:@"[object TestObjectPrototype]"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"TestObject"] = [TestObject class];
        JSValue *result = [context evaluateScript:@"String(TestObject)"];
        checkResult(@"String(TestObject)", [result isEqualToObject:@"function TestObject() {\n    [native code]\n}"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue* value = [JSValue valueWithObject:[TestObject class] inContext:context];
        checkResult(@"[value toObject] == [TestObject class]", [value toObject] == [TestObject class]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"TestObject"] = [TestObject class];
        JSValue *result = [context evaluateScript:@"TestObject.parentTest()"];
        checkResult(@"TestObject.parentTest()", [result isEqualToObject:@"TestObject"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject* testObject = [TestObject testObject];
        context[@"testObjectA"] = testObject;
        context[@"testObjectB"] = testObject;
        JSValue *result = [context evaluateScript:@"testObjectA == testObjectB"];
        checkResult(@"testObjectA == testObjectB", result.isBoolean && [result toBool]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject* testObject = [TestObject testObject];
        context[@"testObject"] = testObject;
        testObject.point = (CGPoint){3,4};
        JSValue *result = [context evaluateScript:@"var result = JSON.stringify(testObject.point); testObject.point = {x:12,y:14}; result"];
        checkResult(@"testObject.point - result", [result isEqualToObject:@"{\"x\":3,\"y\":4}"]);
        checkResult(@"testObject.point - {x:12,y:14}", testObject.point.x == 12 && testObject.point.y == 14);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject* testObject = [TestObject testObject];
        testObject.six = 6;
        context[@"testObject"] = testObject;
        context[@"mul"] = ^(int x, int y){ return x * y; };
        JSValue *result = [context evaluateScript:@"mul(testObject.six, 7)"];
        checkResult(@"mul(testObject.six, 7)", result.isNumber && [result toInt32] == 42);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject* testObject = [TestObject testObject];
        context[@"testObject"] = testObject;
        context[@"testObject"][@"variable"] = @4;
        [context evaluateScript:@"++testObject.variable"];
        checkResult(@"++testObject.variable", testObject.variable == 5);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"point"] = @{ @"x":@6, @"y":@7 };
        JSValue *result = [context evaluateScript:@"point.x + ',' + point.y"];
        checkResult(@"point.x + ',' + point.y", [result isEqualToObject:@"6,7"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"point"] = @{ @"x":@6, @"y":@7 };
        JSValue *result = [context evaluateScript:@"point.x + ',' + point.y"];
        checkResult(@"point.x + ',' + point.y", [result isEqualToObject:@"6,7"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject* testObject = [TestObject testObject];
        context[@"testObject"] = testObject;
        JSValue *result = [context evaluateScript:@"testObject.getString()"];
        checkResult(@"testObject.getString()", result.isString && [result toInt32] == 42);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject* testObject = [TestObject testObject];
        context[@"testObject"] = testObject;
        JSValue *result = [context evaluateScript:@"testObject.testArgumentTypes(101,0.5,true,'foo',666,[false,'bar',false],{x:'baz'})"];
        checkResult(@"testObject.testArgumentTypes", [result isEqualToObject:@"101,0.5,1,foo,666,bar,baz"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject* testObject = [TestObject testObject];
        context[@"testObject"] = testObject;
        JSValue *result = [context evaluateScript:@"testObject.getString.call(testObject)"];
        checkResult(@"testObject.getString.call(testObject)", result.isString && [result toInt32] == 42);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject* testObject = [TestObject testObject];
        context[@"testObject"] = testObject;
        checkResult(@"testObject.getString.call({}) pre", !context.exception);
        [context evaluateScript:@"testObject.getString.call({})"];
        checkResult(@"testObject.getString.call({}) post", context.exception);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject* testObject = [TestObject testObject];
        context[@"testObject"] = testObject;
        JSValue *result = [context evaluateScript:@"var result = 0; testObject.callback(function(x){ result = x; }); result"];
        checkResult(@"testObject.callback", result.isNumber && [result toInt32] == 42);
        result = [context evaluateScript:@"testObject.bogusCallback"];
        checkResult(@"testObject.bogusCallback == undefined", result.isUndefined);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject *testObject = [TestObject testObject];
        context[@"testObject"] = testObject;
        JSValue *result = [context evaluateScript:@"Function.prototype.toString.call(testObject.callback)"];
        checkResult(@"Function.prototype.toString", !context.exception && !result.isUndefined);
    }

    @autoreleasepool {
        JSContext *context1 = [[JSContext alloc] init];
        JSContext *context2 = [[JSContext alloc] initWithVirtualMachine:context1.virtualMachine];
        JSValue *value = [JSValue valueWithDouble:42 inContext:context2];
        context1[@"passValueBetweenContexts"] = value;
        JSValue *result = [context1 evaluateScript:@"passValueBetweenContexts"];
        checkResult(@"[value isEqualToObject:result]", [value isEqualToObject:result]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"handleTheDictionary"] = ^(NSDictionary *dict) {
            NSDictionary *expectedDict = @{
                @"foo": @(1),
                @"bar": @{ @"baz": @(2) }
            };
            checkResult(@"recursively convert nested dictionaries", [dict isEqualToDictionary:expectedDict]);
        };
        [context evaluateScript:@"var myDict = { \
            'foo': 1, \
            'bar': {'baz': 2} \
        }; \
        handleTheDictionary(myDict);"];

        context[@"handleTheArray"] = ^(NSArray *array) {
            NSArray *expectedArray = @[@"foo", @"bar", @[@"baz"]];
            checkResult(@"recursively convert nested arrays", [array isEqualToArray:expectedArray]);
        };
        [context evaluateScript:@"var myArray = ['foo', 'bar', ['baz']]; handleTheArray(myArray);"];
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject *testObject = [TestObject testObject];
        @autoreleasepool {
            context[@"testObject"] = testObject;
            [context evaluateScript:@"var constructor = Object.getPrototypeOf(testObject).constructor; constructor.prototype = undefined;"];
            [context evaluateScript:@"testObject = undefined"];
        }
        
        JSSynchronousGarbageCollectForDebugging([context JSGlobalContextRef]);

        @autoreleasepool {
            context[@"testObject"] = testObject;
        }
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TextXYZ *testXYZ = [[TextXYZ alloc] init];

        @autoreleasepool {
            context[@"testXYZ"] = testXYZ;

            [context evaluateScript:@" \
                didClick = false; \
                testXYZ.onclick = function() { \
                    didClick = true; \
                }; \
                 \
                testXYZ.weakOnclick = function() { \
                    return 'foo'; \
                }; \
            "];
        }

        @autoreleasepool {
            [testXYZ click];
            JSValue *result = [context evaluateScript:@"didClick"];
            checkResult(@"Event handler onclick", [result toBool]);
        }

        JSSynchronousGarbageCollectForDebugging([context JSGlobalContextRef]);

        @autoreleasepool {
            JSValue *result = [context evaluateScript:@"testXYZ.onclick"];
            checkResult(@"onclick still around after GC", !(result.isNull || result.isUndefined));
        }


        @autoreleasepool {
            JSValue *result = [context evaluateScript:@"testXYZ.weakOnclick"];
            checkResult(@"weakOnclick not around after GC", result.isNull || result.isUndefined);
        }

        @autoreleasepool {
            [context evaluateScript:@" \
                didClick = false; \
                testXYZ = null; \
            "];
        }

        JSSynchronousGarbageCollectForDebugging([context JSGlobalContextRef]);

        @autoreleasepool {
            [testXYZ click];
            JSValue *result = [context evaluateScript:@"didClick"];
            checkResult(@"Event handler onclick doesn't fire", ![result toBool]);
        }
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TinyDOMNode *root = [[TinyDOMNode alloc] initWithVirtualMachine:context.virtualMachine];
        TinyDOMNode *lastNode = root;
        for (NSUInteger i = 0; i < 3; i++) {
            TinyDOMNode *newNode = [[TinyDOMNode alloc] initWithVirtualMachine:context.virtualMachine];
            [lastNode appendChild:newNode];
            lastNode = newNode;
        }

        @autoreleasepool {
            context[@"root"] = root;
            context[@"getLastNodeInChain"] = ^(TinyDOMNode *head){
                TinyDOMNode *lastNode = nil;
                while (head) {
                    lastNode = head;
                    head = [lastNode childAtIndex:0];
                }
                return lastNode;
            };
            [context evaluateScript:@"getLastNodeInChain(root).myCustomProperty = 42;"];
        }

        JSSynchronousGarbageCollectForDebugging([context JSGlobalContextRef]);

        JSValue *myCustomProperty = [context evaluateScript:@"getLastNodeInChain(root).myCustomProperty"];
        checkResult(@"My custom property == 42", myCustomProperty.isNumber && [myCustomProperty toInt32] == 42);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TinyDOMNode *root = [[TinyDOMNode alloc] initWithVirtualMachine:context.virtualMachine];
        TinyDOMNode *lastNode = root;
        for (NSUInteger i = 0; i < 3; i++) {
            TinyDOMNode *newNode = [[TinyDOMNode alloc] initWithVirtualMachine:context.virtualMachine];
            [lastNode appendChild:newNode];
            lastNode = newNode;
        }

        @autoreleasepool {
            context[@"root"] = root;
            context[@"getLastNodeInChain"] = ^(TinyDOMNode *head){
                TinyDOMNode *lastNode = nil;
                while (head) {
                    lastNode = head;
                    head = [lastNode childAtIndex:0];
                }
                return lastNode;
            };
            [context evaluateScript:@"getLastNodeInChain(root).myCustomProperty = 42;"];

            [root appendChild:[root childAtIndex:0]];
            [root removeChildAtIndex:0];
        }

        JSSynchronousGarbageCollectForDebugging([context JSGlobalContextRef]);

        JSValue *myCustomProperty = [context evaluateScript:@"getLastNodeInChain(root).myCustomProperty"];
        checkResult(@"duplicate calls to addManagedReference don't cause things to die", myCustomProperty.isNumber && [myCustomProperty toInt32] == 42);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *o = [JSValue valueWithNewObjectInContext:context];
        o[@"foo"] = @"foo";
        JSSynchronousGarbageCollectForDebugging([context JSGlobalContextRef]);

        checkResult(@"JSValue correctly protected its internal value", [[o[@"foo"] toString] isEqualToString:@"foo"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject *testObject = [TestObject testObject];
        context[@"testObject"] = testObject;
        [context evaluateScript:@"testObject.__lookupGetter__('variable').call({})"];
        checkResult(@"Make sure we throw an exception when calling getter on incorrect |this|", context.exception);
    }

    @autoreleasepool {
        static constexpr unsigned count = 100;
        NSMutableArray *array = [NSMutableArray arrayWithCapacity:count];
        JSContext *context = [[JSContext alloc] init];
        @autoreleasepool {
            for (unsigned i = 0; i < count; ++i) {
                JSValue *object = [JSValue valueWithNewObjectInContext:context];
                JSManagedValue *managedObject = [JSManagedValue managedValueWithValue:object];
                [array addObject:managedObject];
            }
        }
        JSSynchronousGarbageCollectForDebugging([context JSGlobalContextRef]);
        for (unsigned i = 0; i < count; ++i)
            [context.virtualMachine addManagedReference:array[i] withOwner:array];
    }

    @autoreleasepool {
        TestObject *testObject = [TestObject testObject];
        JSManagedValue *managedTestObject;
        @autoreleasepool {
            JSContext *context = [[JSContext alloc] init];
            context[@"testObject"] = testObject;
            managedTestObject = [JSManagedValue managedValueWithValue:context[@"testObject"]];
            [context.virtualMachine addManagedReference:managedTestObject withOwner:testObject];
        }
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        TestObject *testObject = [TestObject testObject];
        context[@"testObject"] = testObject;
        JSManagedValue *managedValue = nil;
        @autoreleasepool {
            JSValue *object = [JSValue valueWithNewObjectInContext:context];
            managedValue = [JSManagedValue managedValueWithValue:object andOwner:testObject];
            [context.virtualMachine addManagedReference:managedValue withOwner:testObject];
        }
        JSSynchronousGarbageCollectForDebugging([context JSGlobalContextRef]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"MyClass"] = ^{
            JSValue *newThis = [JSValue valueWithNewObjectInContext:[JSContext currentContext]];
            JSGlobalContextRef contextRef = [[JSContext currentContext] JSGlobalContextRef];
            JSObjectRef newThisRef = JSValueToObject(contextRef, [newThis JSValueRef], NULL);
            JSObjectSetPrototype(contextRef, newThisRef, [[JSContext currentContext][@"MyClass"][@"prototype"] JSValueRef]);
            return newThis;
        };

        context[@"MyOtherClass"] = ^{
            JSValue *newThis = [JSValue valueWithNewObjectInContext:[JSContext currentContext]];
            JSGlobalContextRef contextRef = [[JSContext currentContext] JSGlobalContextRef];
            JSObjectRef newThisRef = JSValueToObject(contextRef, [newThis JSValueRef], NULL);
            JSObjectSetPrototype(contextRef, newThisRef, [[JSContext currentContext][@"MyOtherClass"][@"prototype"] JSValueRef]);
            return newThis;
        };

        context.exceptionHandler = ^(JSContext *context, JSValue *exception) {
            NSLog(@"EXCEPTION: %@", [exception toString]);
            context.exception = nil;
        };

        JSValue *constructor1 = context[@"MyClass"];
        JSValue *constructor2 = context[@"MyOtherClass"];

        JSValue *value1 = [context evaluateScript:@"new MyClass()"];
        checkResult(@"value1 instanceof MyClass", [value1 isInstanceOf:constructor1]);
        checkResult(@"!(value1 instanceof MyOtherClass)", ![value1 isInstanceOf:constructor2]);
        checkResult(@"MyClass.prototype.constructor === MyClass", [[context evaluateScript:@"MyClass.prototype.constructor === MyClass"] toBool]);
        checkResult(@"MyClass instanceof Function", [[context evaluateScript:@"MyClass instanceof Function"] toBool]);

        JSValue *value2 = [context evaluateScript:@"new MyOtherClass()"];
        checkResult(@"value2 instanceof MyOtherClass", [value2 isInstanceOf:constructor2]);
        checkResult(@"!(value2 instanceof MyClass)", ![value2 isInstanceOf:constructor1]);
        checkResult(@"MyOtherClass.prototype.constructor === MyOtherClass", [[context evaluateScript:@"MyOtherClass.prototype.constructor === MyOtherClass"] toBool]);
        checkResult(@"MyOtherClass instanceof Function", [[context evaluateScript:@"MyOtherClass instanceof Function"] toBool]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"MyClass"] = ^{
            NSLog(@"I'm intentionally not returning anything.");
        };
        JSValue *result = [context evaluateScript:@"new MyClass()"];
        checkResult(@"result === undefined", result.isUndefined);
        checkResult(@"exception.message is correct'", context.exception 
            && [@"Objective-C blocks called as constructors must return an object." isEqualToString:[context.exception[@"message"] toString]]);
    }

    @autoreleasepool {
        checkResult(@"[JSContext currentThis] == nil outside of callback", ![JSContext currentThis]);
        checkResult(@"[JSContext currentArguments] == nil outside of callback", ![JSContext currentArguments]);
        if ([JSContext currentCallee])
            checkResult(@"[JSContext currentCallee] == nil outside of callback", ![JSContext currentCallee]);
    }

    if ([JSContext currentCallee]) {
        @autoreleasepool {
            JSContext *context = [[JSContext alloc] init];
            context[@"testFunction"] = ^{
                checkResult(@"testFunction.foo === 42", [[JSContext currentCallee][@"foo"] toInt32] == 42);
            };
            context[@"testFunction"][@"foo"] = @42;
            [context[@"testFunction"] callWithArguments:nil];

            context[@"TestConstructor"] = ^{
                JSValue *newThis = [JSValue valueWithNewObjectInContext:[JSContext currentContext]];
                JSGlobalContextRef contextRef = [[JSContext currentContext] JSGlobalContextRef];
                JSObjectRef newThisRef = JSValueToObject(contextRef, [newThis JSValueRef], NULL);
                JSObjectSetPrototype(contextRef, newThisRef, [[JSContext currentCallee][@"prototype"] JSValueRef]);
                return newThis;
            };
            checkResult(@"(new TestConstructor) instanceof TestConstructor", [context evaluateScript:@"(new TestConstructor) instanceof TestConstructor"]);
        }
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"TestObject"] = [TestObject class];
        JSValue *testObject = [context evaluateScript:@"(new TestObject())"];
        checkResult(@"testObject instanceof TestObject", [testObject isInstanceOf:context[@"TestObject"]]);

        context[@"TextXYZ"] = [TextXYZ class];
        JSValue *textObject = [context evaluateScript:@"(new TextXYZ(\"Called TextXYZ constructor!\"))"];
        checkResult(@"textObject instanceof TextXYZ", [textObject isInstanceOf:context[@"TextXYZ"]]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"ClassA"] = [ClassA class];
        context[@"ClassB"] = [ClassB class];
        context[@"ClassC"] = [ClassC class]; // Should print error message about too many inits found.
        context[@"ClassCPrime"] = [ClassCPrime class]; // Ditto.

        JSValue *a = [context evaluateScript:@"(new ClassA(42))"];
        checkResult(@"a instanceof ClassA", [a isInstanceOf:context[@"ClassA"]]);
        checkResult(@"a.initialize() is callable", [[a invokeMethod:@"initialize" withArguments:@[]] toInt32] == 42);

        JSValue *b = [context evaluateScript:@"(new ClassB(42, 53))"];
        checkResult(@"b instanceof ClassB", [b isInstanceOf:context[@"ClassB"]]);

        JSValue *canConstructClassC = [context evaluateScript:@"(function() { \
            try { \
                (new ClassC(1, 2)); \
                return true; \
            } catch(e) { \
                return false; \
            } \
        })()"];
        checkResult(@"shouldn't be able to construct ClassC", ![canConstructClassC toBool]);

        JSValue *canConstructClassCPrime = [context evaluateScript:@"(function() { \
            try { \
                (new ClassCPrime(1)); \
                return true; \
            } catch(e) { \
                return false; \
            } \
        })()"];
        checkResult(@"shouldn't be able to construct ClassCPrime", ![canConstructClassCPrime toBool]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"ClassA"] = [ClassA class];
        context.exceptionHandler = ^(JSContext *context, JSValue *exception) {
            NSLog(@"%@", [exception toString]);
            context.exception = exception;
        };

        checkResult(@"ObjC Constructor without 'new' pre", !context.exception);
        [context evaluateScript:@"ClassA(42)"];
        checkResult(@"ObjC Constructor without 'new' post", context.exception);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"ClassD"] = [ClassD class];
        context[@"ClassE"] = [ClassE class];
       
        JSValue *d = [context evaluateScript:@"(new ClassD())"];
        checkResult(@"Returning instance of ClassE from ClassD's init has correct class", [d isInstanceOf:context[@"ClassE"]]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        while (!evilAllocationObjectWasDealloced) {
            @autoreleasepool {
                EvilAllocationObject *evilObject = [[EvilAllocationObject alloc] initWithContext:context];
                context[@"evilObject"] = evilObject;
                context[@"evilObject"] = nil;
            }
        }
        checkResult(@"EvilAllocationObject was successfully dealloced without crashing", evilAllocationObjectWasDealloced);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        checkResult(@"default context.name is nil", context.name == nil);
        NSString *name1 = @"Name1";
        NSString *name2 = @"Name2";
        context.name = name1;
        NSString *fetchedName1 = context.name;
        context.name = name2;
        NSString *fetchedName2 = context.name;
        context.name = nil;
        NSString *fetchedName3 = context.name;
        checkResult(@"fetched context.name was expected", [fetchedName1 isEqualToString:name1]);
        checkResult(@"fetched context.name was expected", [fetchedName2 isEqualToString:name2]);
        checkResult(@"fetched context.name was expected", ![fetchedName1 isEqualToString:fetchedName2]);
        checkResult(@"fetched context.name was expected", fetchedName3 == nil);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        context[@"UnexportedObject"] = [UnexportedObject class];
        context[@"makeObject"] = ^{
            return [[UnexportedObject alloc] init];
        };
        JSValue *result = [context evaluateScript:@"(makeObject() instanceof UnexportedObject)"];
        checkResult(@"makeObject() instanceof UnexportedObject", result.isBoolean && [result toBool]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        [[JSValue valueWithInt32:42 inContext:context] toDictionary];
        [[JSValue valueWithInt32:42 inContext:context] toArray];
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];

        // Create the root, make it reachable from JS, and force an EdenCollection
        // so that we scan the external object graph.
        TestObject *root = [TestObject testObject];
        @autoreleasepool {
            context[@"root"] = root;
        }
        JSSynchronousEdenCollectForDebugging([context JSGlobalContextRef]);

        // Create a new Obj-C object only reachable via the external object graph
        // through the object we already scanned during the EdenCollection.
        TestObject *child = [TestObject testObject];
        [context.virtualMachine addManagedReference:child withOwner:root];

        // Create a new managed JSValue that will only be kept alive if we properly rescan
        // the external object graph.
        JSManagedValue *managedJSObject = nil;
        @autoreleasepool {
            JSValue *jsObject = [JSValue valueWithObject:@"hello" inContext:context];
            managedJSObject = [JSManagedValue managedValueWithValue:jsObject];
            [context.virtualMachine addManagedReference:managedJSObject withOwner:child];
        }

        // Force another EdenCollection. It should rescan the new part of the external object graph.
        JSSynchronousEdenCollectForDebugging([context JSGlobalContextRef]);
        
        // Check that the managed JSValue is still alive.
        checkResult(@"EdenCollection doesn't reclaim new managed values", [managedJSObject value] != nil);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        
        pthread_t threadID;
        pthread_create(&threadID, NULL, &threadMain, (__bridge void*)context);
        pthread_join(threadID, nullptr);
        JSSynchronousGarbageCollectForDebugging([context JSGlobalContextRef]);

        checkResult(@"Did not crash after entering the VM from another thread", true);
    }
    
    @autoreleasepool {
        std::vector<pthread_t> threads;
        bool ok = true;
        for (unsigned i = 0; i < 5; ++i) {
            pthread_t threadID;
            pthread_create(&threadID, nullptr, multiVMThreadMain, &ok);
            threads.push_back(threadID);
        }

        for (pthread_t thread : threads)
            pthread_join(thread, nullptr);

        checkResult(@"Ran code in five concurrent VMs that GC'd", ok);
    }

    currentThisInsideBlockGetterTest();
    runDateTests();
    runJSExportTests();
    runJSWrapperMapTests();
    runRegress141275();
    runRegress141809();
}

@protocol NumberProtocol <JSExport>

@property (nonatomic) NSInteger number;

@end

@interface NumberObject : NSObject <NumberProtocol>

@property (nonatomic) NSInteger number;

@end

@implementation NumberObject

@end

// Check that negative NSIntegers retain the correct value when passed into JS code.
static void checkNegativeNSIntegers()
{
    NumberObject *container = [[NumberObject alloc] init];
    container.number = -1;
    JSContext *context = [[JSContext alloc] init];
    context[@"container"] = container;
    NSString *jsID = @"var getContainerNumber = function() { return container.number }";
    [context evaluateScript:jsID];
    JSValue *jsFunction = context[@"getContainerNumber"];
    JSValue *result = [jsFunction callWithArguments:@[]];
    
    checkResult(@"Negative number maintained its original value", [[result toString] isEqualToString:@"-1"]);
}

enum class Resolution {
    ResolveEager,
    RejectEager,
    ResolveLate,
    RejectLate,
};

static void promiseWithExecutor(Resolution resolution)
{
    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];

        __block JSValue *resolveCallback;
        __block JSValue *rejectCallback;
        JSValue *promise = [JSValue valueWithNewPromiseInContext:context fromExecutor:^(JSValue *resolve, JSValue *reject) {
            if (resolution == Resolution::ResolveEager)
                [resolve callWithArguments:@[@YES]];
            if (resolution == Resolution::RejectEager)
                [reject callWithArguments:@[@YES]];
            resolveCallback = resolve;
            rejectCallback = reject;
        }];

        __block bool valueWasResolvedTrue = false;
        __block bool valueWasRejectedTrue = false;
        [promise invokeMethod:@"then" withArguments:@[
            ^(JSValue *value) { valueWasResolvedTrue = [value isBoolean] && [value toBool]; },
            ^(JSValue *value) { valueWasRejectedTrue = [value isBoolean] && [value toBool]; },
        ]];

        switch (resolution) {
        case Resolution::ResolveEager:
            checkResult(@"ResolveEager should have set resolve early.", valueWasResolvedTrue && !valueWasRejectedTrue);
            break;
        case Resolution::RejectEager:
            checkResult(@"RejectEager should have set reject early.", !valueWasResolvedTrue && valueWasRejectedTrue);
            break;
        default:
            checkResult(@"Resolve/RejectLate should have not have set anything early.", !valueWasResolvedTrue && !valueWasRejectedTrue);
            break;
        }

        valueWasResolvedTrue = false;
        valueWasRejectedTrue = false;

        // Run script to make sure reactions don't happen again
        [context evaluateScript:@"{ };"];

        if (resolution == Resolution::ResolveLate)
            [resolveCallback callWithArguments:@[@YES]];
        if (resolution == Resolution::RejectLate)
            [rejectCallback callWithArguments:@[@YES]];

        switch (resolution) {
        case Resolution::ResolveLate:
            checkResult(@"ResolveLate should have set resolve late.", valueWasResolvedTrue && !valueWasRejectedTrue);
            break;
        case Resolution::RejectLate:
            checkResult(@"RejectLate should have set reject late.", !valueWasResolvedTrue && valueWasRejectedTrue);
            break;
        default:
            checkResult(@"Resolve/RejectEarly should have not have set anything late.", !valueWasResolvedTrue && !valueWasRejectedTrue);
            break;
        }
    }
}

static void promiseRejectOnJSException()
{
    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];

        JSValue *promise = [JSValue valueWithNewPromiseInContext:context fromExecutor:^(JSValue *, JSValue *) {
            context.exception = [JSValue valueWithNewErrorFromMessage:@"dope" inContext:context];
        }];
        checkResult(@"Exception set in callback should not propagate", !context.exception);

        __block bool reasonWasObject = false;
        [promise invokeMethod:@"catch" withArguments:@[^(JSValue *reason) { reasonWasObject = [reason isObject]; }]];

        checkResult(@"Setting an exception in executor causes the promise to be rejected", reasonWasObject);

        promise = [JSValue valueWithNewPromiseInContext:context fromExecutor:^(JSValue *, JSValue *) {
            [context evaluateScript:@"throw new Error('dope');"];
        }];
        checkResult(@"Exception thrown in callback should not propagate", !context.exception);

        reasonWasObject = false;
        [promise invokeMethod:@"catch" withArguments:@[^(JSValue *reason) { reasonWasObject = [reason isObject]; }]];

        checkResult(@"Running code that throws an exception in the executor causes the promise to be rejected", reasonWasObject);
    }
}

static void promiseCreateResolved()
{
    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];

        JSValue *promise = [JSValue valueWithNewPromiseResolvedWithResult:[NSNull null] inContext:context];
        __block bool calledWithNull = false;
        [promise invokeMethod:@"then" withArguments:@[
            ^(JSValue *result) { calledWithNull = [result isNull]; }
        ]];

        checkResult(@"ResolvedPromise should actually resolve the promise", calledWithNull);
    }
}

static void promiseCreateRejected()
{
    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];

        JSValue *promise = [JSValue valueWithNewPromiseRejectedWithReason:[NSNull null] inContext:context];
        __block bool calledWithNull = false;
        [promise invokeMethod:@"then" withArguments:@[
            [NSNull null],
            ^(JSValue *result) { calledWithNull = [result isNull]; }
        ]];

        checkResult(@"RejectedPromise should actually reject the promise", calledWithNull);
    }
}

static void parallelPromiseResolveTest()
{
    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];

        __block RefPtr<Thread> thread;

        Atomic<bool> shouldResolveSoon { false };
        Atomic<bool> startedThread { false };
        auto* shouldResolveSoonPtr = &shouldResolveSoon;
        auto* startedThreadPtr = &startedThread;

        JSValue *promise = [JSValue valueWithNewPromiseInContext:context fromExecutor:^(JSValue *resolve, JSValue *) {
            thread = Thread::create("async thread", ^() {
                startedThreadPtr->store(true);
                while (!shouldResolveSoonPtr->load()) { }
                [resolve callWithArguments:@[[NSNull null]]];
            });

        }];

        shouldResolveSoon.store(true);
        while (!startedThread.load())
            [context evaluateScript:@"for (let i = 0; i < 10000; i++) { }"];

        thread->waitForCompletion();

        __block bool calledWithNull = false;
        [promise invokeMethod:@"then" withArguments:@[
            ^(JSValue *result) { calledWithNull = [result isNull]; }
        ]];

        checkResult(@"Promise should be resolved", calledWithNull);
    }
}

typedef JSValue *(^ResolveBlock)(JSContext *, JSValue *, JSScript *);
typedef void (^FetchBlock)(JSContext *, JSValue *, JSValue *, JSValue *);

@interface JSContextFetchDelegate : JSContext <JSModuleLoaderDelegate>

+ (instancetype)contextWithBlockForFetch:(FetchBlock)block;

@property unsigned willEvaluateModuleCallCount;
@property unsigned didEvaluateModuleCallCount;
@property BOOL sawBarJS;
@property BOOL sawFooJS;

@end

@implementation JSContextFetchDelegate {
    FetchBlock m_fetchBlock;
}

+ (instancetype)contextWithBlockForFetch:(FetchBlock)block
{
    auto *result = [[JSContextFetchDelegate alloc] init];
    result.willEvaluateModuleCallCount = 0;
    result.didEvaluateModuleCallCount = 0;
    result.sawBarJS = NO;
    result.sawFooJS = NO;
    result->m_fetchBlock = block;
    return result;
}

- (void)context:(JSContext *)context fetchModuleForIdentifier:(JSValue *)identifier withResolveHandler:(JSValue *)resolve andRejectHandler:(JSValue *)reject
{
    m_fetchBlock(context, identifier, resolve, reject);
}

- (void)willEvaluateModule:(NSURL *)url
{
    self.willEvaluateModuleCallCount += 1;
    self.sawBarJS |= [url isEqual:[NSURL URLWithString:@"file:///directory/bar.js"]];
}

- (void)didEvaluateModule:(NSURL *)url
{
    self.didEvaluateModuleCallCount += 1;
    self.sawFooJS |= [url isEqual:[NSURL URLWithString:@"file:///foo.js"]];
}

@end

static void checkModuleCodeRan(JSContext *context, JSValue *promise, JSValue *expected)
{
    __block BOOL promiseWasResolved = false;
    [promise invokeMethod:@"then" withArguments:@[^(JSValue *exportValue) {
        promiseWasResolved = true;
        checkResult(@"module exported value 'exp' is null", [exportValue[@"exp"] isEqualToObject:expected]);
        checkResult(@"ran is %@", [context[@"ran"] isEqualToObject:expected]);
    }, ^(JSValue *error) {
        NSLog(@"%@", [error toString]);
        checkResult(@"module graph was resolved as expected", NO);
    }]];
    checkResult(@"Promise was resolved", promiseWasResolved);
}

static void checkModuleWasRejected(JSContext *context, JSValue *promise)
{
    [promise invokeMethod:@"then" withArguments:@[^() {
        checkResult(@"module was rejected as expected", NO);
    }, ^(JSValue *error) {
        NSLog(@"%@", [error toString]);
        checkResult(@"module graph was rejected with error", ![error isEqualWithTypeCoercionToObject:[JSValue valueWithNullInContext:context]]);
    }]];
}

static void testFetch()
{
    @autoreleasepool {
        auto *context = [JSContextFetchDelegate contextWithBlockForFetch:^(JSContext *context, JSValue *identifier, JSValue *resolve, JSValue *reject) {
            if ([identifier isEqualToObject:@"file:///directory/bar.js"]) {
                [resolve callWithArguments:@[[JSScript scriptOfType:kJSScriptTypeModule 
                    withSource:@"import \"../foo.js\"; export let exp = null;"
                    andSourceURL:[NSURL fileURLWithPath:@"/directory/bar.js"] 
                    andBytecodeCache:nil
                    inVirtualMachine:[context virtualMachine]
                    error:nil]]];
            } else if ([identifier isEqualToObject:@"file:///foo.js"]) {
                [resolve callWithArguments:@[[JSScript scriptOfType:kJSScriptTypeModule 
                    withSource:@"globalThis.ran = null;"
                    andSourceURL:[NSURL fileURLWithPath:@"/foo.js"] 
                    andBytecodeCache:nil
                    inVirtualMachine:[context virtualMachine]
                    error:nil]]];
            } else
                [reject callWithArguments:@[[JSValue valueWithNewErrorFromMessage:@"Weird path" inContext:context]]];
        }];
        context.moduleLoaderDelegate = context;
        JSValue *promise = [context evaluateScript:@"import('./bar.js');" withSourceURL:[NSURL fileURLWithPath:@"/directory" isDirectory:YES]];
        JSValue *null = [JSValue valueWithNullInContext:context];
        checkModuleCodeRan(context, promise, null);
        checkResult(@"Context should call willEvaluateModule: twice", context.willEvaluateModuleCallCount == 2);
        checkResult(@"Context should call didEvaluateModule: twice", context.didEvaluateModuleCallCount == 2);
        checkResult(@"Context should see bar.js url", !!context.sawBarJS);
        checkResult(@"Context should see foo.js url", !!context.sawFooJS);
    }
}

static void testFetchWithTwoCycle()
{
    @autoreleasepool {
        auto *context = [JSContextFetchDelegate contextWithBlockForFetch:^(JSContext *context, JSValue *identifier, JSValue *resolve, JSValue *reject) {
            if ([identifier isEqualToObject:@"file:///directory/bar.js"]) {
                [resolve callWithArguments:@[[JSScript scriptOfType:kJSScriptTypeModule 
                    withSource:@"import { n } from \"../foo.js\"; export let exp = n;"
                    andSourceURL:[NSURL fileURLWithPath:@"/directory/bar.js"] 
                    andBytecodeCache:nil
                    inVirtualMachine:[context virtualMachine]
                    error:nil]]];
            } else if ([identifier isEqualToObject:@"file:///foo.js"]) {
                [resolve callWithArguments:@[[JSScript scriptOfType:kJSScriptTypeModule 
                    withSource:@"import \"./directory/bar.js\"; globalThis.ran = null; export let n = null;"
                    andSourceURL:[NSURL fileURLWithPath:@"/foo.js"]
                    andBytecodeCache:nil
                    inVirtualMachine:[context virtualMachine]
                    error:nil]]];
            } else
                [reject callWithArguments:@[[JSValue valueWithNewErrorFromMessage:@"Weird path" inContext:context]]];
        }];
        context.moduleLoaderDelegate = context;
        JSValue *promise = [context evaluateScript:@"import('./bar.js');" withSourceURL:[NSURL fileURLWithPath:@"/directory" isDirectory:YES]];
        JSValue *null = [JSValue valueWithNullInContext:context];
        checkModuleCodeRan(context, promise, null);
        checkResult(@"Context should call willEvaluateModule: twice", context.willEvaluateModuleCallCount == 2);
        checkResult(@"Context should call didEvaluateModule: twice", context.didEvaluateModuleCallCount == 2);
    }
}


static void testFetchWithThreeCycle()
{
    @autoreleasepool {
        auto *context = [JSContextFetchDelegate contextWithBlockForFetch:^(JSContext *context, JSValue *identifier, JSValue *resolve, JSValue *reject) {
            if ([identifier isEqualToObject:@"file:///directory/bar.js"]) {
                [resolve callWithArguments:@[[JSScript scriptOfType:kJSScriptTypeModule 
                    withSource:@"import { n } from \"../foo.js\"; export let foo = n;"
                    andSourceURL:[NSURL fileURLWithPath:@"/directory/bar.js"] 
                    andBytecodeCache:nil
                    inVirtualMachine:[context virtualMachine]
                    error:nil]]];
            } else if ([identifier isEqualToObject:@"file:///foo.js"]) {
                [resolve callWithArguments:@[[JSScript scriptOfType:kJSScriptTypeModule 
                    withSource:@"import \"./otherDirectory/baz.js\"; export let n = null;"
                    andSourceURL:[NSURL fileURLWithPath:@"/foo.js"] 
                    andBytecodeCache:nil
                    inVirtualMachine:[context virtualMachine]
                    error:nil]]];
            } else if ([identifier isEqualToObject:@"file:///otherDirectory/baz.js"]) {
                [resolve callWithArguments:@[[JSScript scriptOfType:kJSScriptTypeModule 
                    withSource:@"import { foo } from \"../directory/bar.js\"; globalThis.ran = null; export let exp = foo;"
                    andSourceURL:[NSURL fileURLWithPath:@"/otherDirectory/baz.js"] 
                    andBytecodeCache:nil
                    inVirtualMachine:[context virtualMachine]
                    error:nil]]];
            } else
                [reject callWithArguments:@[[JSValue valueWithNewErrorFromMessage:@"Weird path" inContext:context]]];
        }];
        context.moduleLoaderDelegate = context;
        JSValue *promise = [context evaluateScript:@"import('../otherDirectory/baz.js');" withSourceURL:[NSURL fileURLWithPath:@"/directory" isDirectory:YES]];
        JSValue *null = [JSValue valueWithNullInContext:context];
        checkModuleCodeRan(context, promise, null);
        checkResult(@"Context should call willEvaluateModule: three times", context.willEvaluateModuleCallCount == 3);
        checkResult(@"Context should call didEvaluateModule: three times", context.didEvaluateModuleCallCount == 3);
        checkResult(@"Context should see bar.js url", !!context.sawBarJS);
        checkResult(@"Context should see foo.js url", !!context.sawFooJS);
    }
}

static void testLoaderResolvesAbsoluteScriptURL()
{
    @autoreleasepool {
        auto *context = [JSContextFetchDelegate contextWithBlockForFetch:^(JSContext *context, JSValue *identifier, JSValue *resolve, JSValue *reject) {
            if ([identifier isEqualToObject:@"file:///directory/bar.js"]) {
                [resolve callWithArguments:@[[JSScript scriptOfType:kJSScriptTypeModule 
                    withSource:@"export let exp = null; globalThis.ran = null;"
                    andSourceURL:[NSURL fileURLWithPath:@"/directory/bar.js"] 
                    andBytecodeCache:nil
                    inVirtualMachine:[context virtualMachine]
                    error:nil]]];
            } else
                [reject callWithArguments:@[[JSValue valueWithNewErrorFromMessage:@"Weird path" inContext:context]]];
        }];
        context.moduleLoaderDelegate = context;
        JSValue *promise = [context evaluateScript:@"import('/directory/bar.js');"];
        JSValue *null = [JSValue valueWithNullInContext:context];
        checkModuleCodeRan(context, promise, null);
        checkResult(@"Context should call willEvaluateModule: once", context.willEvaluateModuleCallCount == 1);
        checkResult(@"Context should call didEvaluateModule: once", context.didEvaluateModuleCallCount == 1);
        checkResult(@"Context should see bar.js url", !!context.sawBarJS);
        checkResult(@"Context should not see foo.js url", !context.sawFooJS);
    }
}

static void testLoaderRejectsNilScriptURL()
{
    @autoreleasepool {
        auto *context = [JSContextFetchDelegate contextWithBlockForFetch:^(JSContext *, JSValue *, JSValue *, JSValue *) {
            checkResult(@"Code is not run", NO);
        }];
        context.moduleLoaderDelegate = context;
        JSValue *promise = [context evaluateScript:@"import('../otherDirectory/baz.js');"];
        checkModuleWasRejected(context, promise);
        checkResult(@"Context should call willEvaluateModule: zero times", context.willEvaluateModuleCallCount == 0);
        checkResult(@"Context should call didEvaluateModule: zero times", context.didEvaluateModuleCallCount == 0);
        checkResult(@"Context should not see bar.js url", !context.sawBarJS);
        checkResult(@"Context should not see foo.js url", !context.sawFooJS);
    }
}

static void testLoaderRejectsFailedFetch()
{
    @autoreleasepool {
        auto *context = [JSContextFetchDelegate contextWithBlockForFetch:^(JSContext *context, JSValue *, JSValue *, JSValue *reject) {
            [reject callWithArguments:@[[JSValue valueWithNewErrorFromMessage:@"Nope" inContext:context]]];
        }];
        context.moduleLoaderDelegate = context;
        JSValue *promise = [context evaluateScript:@"import('/otherDirectory/baz.js');"];
        checkModuleWasRejected(context, promise);
    }
}

static void testImportModuleTwice()
{
    @autoreleasepool {
        auto *context = [JSContextFetchDelegate contextWithBlockForFetch:^(JSContext * context, JSValue *, JSValue *resolve, JSValue *) { 
            [resolve callWithArguments:@[[JSScript scriptOfType:kJSScriptTypeModule 
                withSource:@"ran++; export let exp = 1;"
                andSourceURL:[NSURL fileURLWithPath:@"/baz.js"]
                andBytecodeCache:nil
                inVirtualMachine:[context virtualMachine]
                error:nil]]];
        }];
        context.moduleLoaderDelegate = context;
        context[@"ran"] = @(0);
        JSValue *promise = [context evaluateScript:@"import('/baz.js');"];
        JSValue *promise2 = [context evaluateScript:@"import('/baz.js');"];
        JSValue *one = [JSValue valueWithInt32:1 inContext:context];
        checkModuleCodeRan(context, promise, one);
        checkModuleCodeRan(context, promise2, one);
    }
}

static NSURL *tempFile(NSString *string)
{
    NSURL* tempDirectory = [NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES];
    return [tempDirectory URLByAppendingPathComponent:string];
}

static NSURL* cacheFileInDataVault(NSString* name)
{
#if USE(APPLE_INTERNAL_SDK)
    static NSURL* dataVaultURL;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        char userDir[PATH_MAX];
        RELEASE_ASSERT(confstr(_CS_DARWIN_USER_DIR, userDir, sizeof(userDir)));

        NSString *userDirPath = [[NSFileManager defaultManager] stringWithFileSystemRepresentation:userDir length:strlen(userDir)];
        dataVaultURL = [NSURL fileURLWithPath:userDirPath isDirectory:YES];
        dataVaultURL = [dataVaultURL URLByAppendingPathComponent:@"JavaScriptCore" isDirectory:YES];
        rootless_mkdir_datavault(dataVaultURL.path.UTF8String, 0700, "JavaScriptCore");
    });

    return [dataVaultURL URLByAppendingPathComponent:name isDirectory:NO];
#else
    return tempFile(name);
#endif
}

static void testModuleBytecodeCache()
{
    @autoreleasepool {
        NSString *fooSource = @"import './otherDirectory/baz.js'; export let n = null;";
        NSString *barSource = @"import { n } from '../foo.js'; export let foo = () => n;";
        NSString *bazSource = @"import { foo } from '../directory/bar.js'; globalThis.ran = null; export let exp = foo();";

        NSURL *fooPath = tempFile(@"foo.js");
        NSURL *barPath = tempFile(@"bar.js");
        NSURL *bazPath = tempFile(@"baz.js");

        NSURL *fooCachePath = cacheFileInDataVault(@"foo.js.cache");
        NSURL *barCachePath = cacheFileInDataVault(@"bar.js.cache");
        NSURL *bazCachePath = cacheFileInDataVault(@"baz.js.cache");

        NSURL *fooFakePath = [NSURL fileURLWithPath:@"/foo.js"];
        NSURL *barFakePath = [NSURL fileURLWithPath:@"/directory/bar.js"];
        NSURL *bazFakePath = [NSURL fileURLWithPath:@"/otherDirectory/baz.js"];

        NSFileManager* fileManager = [NSFileManager defaultManager];

        // Clear out any potential old data left over from previous failed runs.
        // This resets the slate clean for this run of the test.
        [fileManager removeItemAtURL:fooPath error:nil];
        [fileManager removeItemAtURL:barPath error:nil];
        [fileManager removeItemAtURL:bazPath error:nil];
        [fileManager removeItemAtURL:fooCachePath error:nil];
        [fileManager removeItemAtURL:barCachePath error:nil];
        [fileManager removeItemAtURL:bazCachePath error:nil];

        [fooSource writeToURL:fooPath atomically:NO encoding:NSASCIIStringEncoding error:nil];
        [barSource writeToURL:barPath atomically:NO encoding:NSASCIIStringEncoding error:nil];
        [bazSource writeToURL:bazPath atomically:NO encoding:NSASCIIStringEncoding error:nil];

        auto block = ^(JSContext *context, JSValue *identifier, JSValue *resolve, JSValue *reject) {
            JSC::Options::forceDiskCache() = true;
            JSScript *script = nil;
            if ([identifier isEqualToObject:[fooFakePath absoluteString]])
                script = [JSScript scriptOfType:kJSScriptTypeModule memoryMappedFromASCIIFile:fooPath withSourceURL:fooFakePath andBytecodeCache:fooCachePath inVirtualMachine:context.virtualMachine error:nil];
            else if ([identifier isEqualToObject:[barFakePath absoluteString]])
                script = [JSScript scriptOfType:kJSScriptTypeModule memoryMappedFromASCIIFile:barPath withSourceURL:barFakePath andBytecodeCache:barCachePath inVirtualMachine:context.virtualMachine error:nil];
            else if ([identifier isEqualToObject:[bazFakePath absoluteString]])
                script = [JSScript scriptOfType:kJSScriptTypeModule memoryMappedFromASCIIFile:bazPath withSourceURL:bazFakePath andBytecodeCache:bazCachePath inVirtualMachine:context.virtualMachine error:nil];

            if (script) {
                NSError *error = nil;
                if (![script cacheBytecodeWithError:&error])
                    CRASH();
                [resolve callWithArguments:@[script]];
            } else
                [reject callWithArguments:@[[JSValue valueWithNewErrorFromMessage:@"Weird path" inContext:context]]];
        };

        @autoreleasepool {
            auto *context = [JSContextFetchDelegate contextWithBlockForFetch:block];
            context.moduleLoaderDelegate = context;
            JSValue *promise = [context evaluateScript:@"import('../otherDirectory/baz.js');" withSourceURL:[NSURL fileURLWithPath:@"/directory" isDirectory:YES]];
            JSValue *null = [JSValue valueWithNullInContext:context];
            checkModuleCodeRan(context, promise, null);
            JSC::Options::forceDiskCache() = false;
        }

        BOOL removedAll = true;
        removedAll &= [fileManager removeItemAtURL:fooPath error:nil];
        removedAll &= [fileManager removeItemAtURL:barPath error:nil];
        removedAll &= [fileManager removeItemAtURL:bazPath error:nil];
        removedAll &= [fileManager removeItemAtURL:fooCachePath error:nil];
        removedAll &= [fileManager removeItemAtURL:barCachePath error:nil];
        removedAll &= [fileManager removeItemAtURL:bazCachePath error:nil];
        checkResult(@"Removed all temp files created", removedAll);
    }
}

static void testProgramBytecodeCache()
{
    @autoreleasepool {
        NSString *fooSource = @"function foo() { return 42; }; function bar() { return 40; }; foo() + bar();";
        NSURL *fooCachePath = cacheFileInDataVault(@"foo.js.cache");
        JSContext *context = [[JSContext alloc] init];
        JSScript *script = [JSScript scriptOfType:kJSScriptTypeProgram withSource:fooSource andSourceURL:[NSURL URLWithString:@"my-path"] andBytecodeCache:fooCachePath inVirtualMachine:context.virtualMachine error:nil];
        RELEASE_ASSERT(script);
        if (![script cacheBytecodeWithError:nil])
            CRASH();

        JSC::Options::forceDiskCache() = true;
        JSValue *result = [context evaluateJSScript:script];
        RELEASE_ASSERT(result);
        RELEASE_ASSERT([result isNumber]);
        checkResult(@"result of cached program is 40+42", [[result toNumber] intValue] == 40 + 42);
        JSC::Options::forceDiskCache() = false;

        NSFileManager* fileManager = [NSFileManager defaultManager];
        BOOL removedAll = [fileManager removeItemAtURL:fooCachePath error:nil];
        checkResult(@"Removed all temp files created", removedAll);
    }
}

static void testBytecodeCacheWithSyntaxError(JSScriptType type)
{
    @autoreleasepool {
        NSString *fooSource = @"this is a syntax error";
        NSURL *fooCachePath = cacheFileInDataVault(@"foo.js.cache");
        JSContext *context = [[JSContext alloc] init];
        JSScript *script = [JSScript scriptOfType:type withSource:fooSource andSourceURL:[NSURL URLWithString:@"my-path"] andBytecodeCache:fooCachePath inVirtualMachine:context.virtualMachine error:nil];
        RELEASE_ASSERT(script);
        NSError *error = nil;
        if ([script cacheBytecodeWithError:&error])
            CRASH();
        RELEASE_ASSERT(error);
        checkResult(@"Got error when trying to cache bytecode for a script with a syntax error.", [[error description] containsString:@"Unable to generate bytecode for this JSScript because"]);
    }
}

static void testBytecodeCacheWithSameCacheFileAndDifferentScript(bool forceDiskCache)
{

    NSURL *cachePath = cacheFileInDataVault(@"cachePath.cache");
    NSURL *sourceURL = [NSURL URLWithString:@"my-path"];

    @autoreleasepool {
        JSVirtualMachine *vm = [[JSVirtualMachine alloc] init];
        NSString *source = @"function foo() { return 42; }; function bar() { return 40; }; foo() + bar();";
        JSContext *context = [[JSContext alloc] initWithVirtualMachine:vm];
        JSScript *script = [JSScript scriptOfType:kJSScriptTypeProgram withSource:source andSourceURL:sourceURL andBytecodeCache:cachePath inVirtualMachine:vm error:nil];
        RELEASE_ASSERT(script);
        if (![script cacheBytecodeWithError:nil])
            CRASH();

        JSC::Options::forceDiskCache() = forceDiskCache;
        JSValue *result = [context evaluateJSScript:script];
        RELEASE_ASSERT(result);
        RELEASE_ASSERT([result isNumber]);
        checkResult(@"Expected 82 as result", [[result toNumber] intValue] == 82);
    }

    @autoreleasepool {
        JSVirtualMachine *vm = [[JSVirtualMachine alloc] init];
        NSString *source = @"function foo() { return 10; }; function bar() { return 20; }; foo() + bar();";
        JSContext *context = [[JSContext alloc] initWithVirtualMachine:vm];
        JSScript *script = [JSScript scriptOfType:kJSScriptTypeProgram withSource:source andSourceURL:sourceURL andBytecodeCache:cachePath inVirtualMachine:vm error:nil];
        RELEASE_ASSERT(script);
        if (![script cacheBytecodeWithError:nil])
            CRASH();

        JSC::Options::forceDiskCache() = forceDiskCache;
        JSValue *result = [context evaluateJSScript:script];
        RELEASE_ASSERT(result);
        RELEASE_ASSERT([result isNumber]);
        checkResult(@"Expected 30 as result", [[result toNumber] intValue] == 30);
    }

    JSC::Options::forceDiskCache() = false;

    NSFileManager* fileManager = [NSFileManager defaultManager];
    BOOL removedAll = [fileManager removeItemAtURL:cachePath error:nil];
    checkResult(@"Removed all temp files created", removedAll);

}

static void testProgramJSScriptException()
{
    @autoreleasepool {
        NSString *source = @"throw 42;";
        JSContext *context = [[JSContext alloc] init];
        JSScript *script = [JSScript scriptOfType:kJSScriptTypeProgram withSource:source andSourceURL:[NSURL URLWithString:@"my-path"] andBytecodeCache:nil inVirtualMachine:context.virtualMachine error:nil];
        RELEASE_ASSERT(script);
        __block bool handledException = false;
        context.exceptionHandler = ^(JSContext *, JSValue *exception) {
            handledException = true;
            RELEASE_ASSERT([exception isNumber]);
            checkResult(@"Program JSScript with exception should have the exception value be 42.", [[exception toNumber] intValue] == 42);
        };

        JSValue *result = [context evaluateJSScript:script];
        RELEASE_ASSERT(result);
        checkResult(@"Program JSScript with exception should return undefined.", [result isUndefined]);
        checkResult(@"Program JSScript with exception should call exception handler.", handledException);
    }
}

static void testCacheFileFailsWhenItsAlreadyCached()
{
    NSURL* cachePath = cacheFileInDataVault(@"foo.program.cache");
    NSURL* sourceURL = [NSURL URLWithString:@"my-path"];
    NSString *source = @"function foo() { return 42; } foo();";

    @autoreleasepool {
        JSVirtualMachine *vm = [[JSVirtualMachine alloc] init];

        JSScript *script = [JSScript scriptOfType:kJSScriptTypeProgram withSource:source andSourceURL:sourceURL andBytecodeCache:cachePath inVirtualMachine:vm error:nil];
        RELEASE_ASSERT(script);
        checkResult(@"Should be able to cache the first file", [script cacheBytecodeWithError:nil]);
    }

    @autoreleasepool {
        JSVirtualMachine *vm = [[JSVirtualMachine alloc] init];

        JSScript *script = [JSScript scriptOfType:kJSScriptTypeProgram withSource:source andSourceURL:sourceURL andBytecodeCache:cachePath inVirtualMachine:vm error:nil];
        RELEASE_ASSERT(script);
        NSError* error = nil;
        checkResult(@"Should not be able to cache the second time because the cache is already present", ![script cacheBytecodeWithError:&error]);
        checkResult(@"Correct error message should be set", [[error description] containsString:@"Cache for JSScript is already non-empty. Can not override it."]);

        JSContext* context = [[JSContext alloc] initWithVirtualMachine:vm];
        JSC::Options::forceDiskCache() = true;
        JSValue *result = [context evaluateJSScript:script];
        RELEASE_ASSERT(result);
        checkResult(@"Result should be 42", [result isNumber] && [result toInt32] == 42);
        JSC::Options::forceDiskCache() = false;
    }

    NSFileManager* fileManager = [NSFileManager defaultManager];
    BOOL removedAll = [fileManager removeItemAtURL:cachePath error:nil];
    checkResult(@"Successfully removed cache file", removedAll);
}

static void testCanCacheManyFilesWithTheSameVM()
{
    NSMutableArray *cachePaths = [[NSMutableArray alloc] init];
    NSMutableArray *scripts = [[NSMutableArray alloc] init];

    for (unsigned i = 0; i < 10000; ++i)
        [cachePaths addObject:cacheFileInDataVault([NSString stringWithFormat:@"cache-%d.cache", i])];

    JSVirtualMachine *vm = [[JSVirtualMachine alloc] init];
    bool cachedAll = true;
    for (NSURL *path : cachePaths) {
        @autoreleasepool {
            NSURL *sourceURL = [NSURL URLWithString:@"id"];
            NSString *source = @"function foo() { return 42; } foo();";
            JSScript *script = [JSScript scriptOfType:kJSScriptTypeProgram withSource:source andSourceURL:sourceURL andBytecodeCache:path inVirtualMachine:vm error:nil];
            RELEASE_ASSERT(script);

            [scripts addObject:script];
            cachedAll &= [script cacheBytecodeWithError:nil];
        }
    }
    checkResult(@"Cached all 10000 scripts", cachedAll);

    JSContext *context = [[JSContext alloc] init];
    bool all42 = true;
    for (JSScript *script : scripts) {
        @autoreleasepool {
            JSValue *result = [context evaluateJSScript:script];
            RELEASE_ASSERT(result);
            all42 &= [result isNumber] && [result toInt32] == 42;
        }
    }
    checkResult(@"All scripts returned 42", all42);

    NSFileManager* fileManager = [NSFileManager defaultManager];
    bool removedAll = true;
    for (NSURL *path : cachePaths)
        removedAll &= [fileManager removeItemAtURL:path error:nil];

    checkResult(@"Removed all cache files", removedAll);
}

static void testIsUsingBytecodeCacheAccessor()
{
    NSURL* cachePath = cacheFileInDataVault(@"foo.program.cache");
    NSURL* sourceURL = [NSURL URLWithString:@"my-path"];
    NSString *source = @"function foo() { return 1337; } foo();";

    @autoreleasepool {
        JSVirtualMachine *vm = [[JSVirtualMachine alloc] init];
        JSContext* context = [[JSContext alloc] initWithVirtualMachine:vm];
        JSScript *script = [JSScript scriptOfType:kJSScriptTypeProgram withSource:source andSourceURL:sourceURL andBytecodeCache:cachePath inVirtualMachine:vm error:nil];
        RELEASE_ASSERT(script);
        checkResult(@"Should not yet be using the bytecode cache", ![script isUsingBytecodeCache]);
        checkResult(@"Should be able to cache the script", [script cacheBytecodeWithError:nil]);
        checkResult(@"Should now using the bytecode cache", [script isUsingBytecodeCache]);
        JSC::Options::forceDiskCache() = true;
        JSValue *result = [context evaluateJSScript:script];
        JSC::Options::forceDiskCache() = false;
        checkResult(@"Result should be 1337", [result isNumber] && [result toInt32] == 1337);
    }

    @autoreleasepool {
        JSVirtualMachine *vm = [[JSVirtualMachine alloc] init];
        JSContext* context = [[JSContext alloc] initWithVirtualMachine:vm];
        JSScript *script = [JSScript scriptOfType:kJSScriptTypeProgram withSource:source andSourceURL:sourceURL andBytecodeCache:cachePath inVirtualMachine:vm error:nil];
        RELEASE_ASSERT(script);
        checkResult(@"Should be using the bytecode cache", [script isUsingBytecodeCache]);
        JSValue *result = [context evaluateJSScript:script];
        checkResult(@"Result should be 1337", [result isNumber] && [result toInt32] == 1337);
    }

    NSFileManager* fileManager = [NSFileManager defaultManager];
    BOOL removedAll = [fileManager removeItemAtURL:cachePath error:nil];
    checkResult(@"Successfully removed cache file", removedAll);
}

static void testBytecodeCacheValidation()
{
    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];

        auto testInvalidCacheURL = [&](NSURL* cacheURL, NSString* expectedErrorMessage)
        {
            NSError* error;
            JSScript *script = [JSScript scriptOfType:kJSScriptTypeProgram withSource:@"" andSourceURL:[NSURL URLWithString:@"my-path"] andBytecodeCache:cacheURL inVirtualMachine:context.virtualMachine error:&error];
            RELEASE_ASSERT(!script);
            RELEASE_ASSERT(error);
            NSString* testDesciption = [NSString stringWithFormat:@"Cache path validation for `%@` fails with message `%@`", cacheURL.absoluteString, expectedErrorMessage];
            checkResult(testDesciption, [error.description containsString:expectedErrorMessage]);
        };

        testInvalidCacheURL([NSURL URLWithString:@""], @"Cache path `` is not a local file");
        testInvalidCacheURL([NSURL URLWithString:@"file:///"], @"Cache path `/` already exists and is not a file");
        testInvalidCacheURL([NSURL URLWithString:@"file:///a/b/c/d/e"], @"Cache directory `/a/b/c/d` is not a directory or does not exist");
#if USE(APPLE_INTERNAL_SDK)
        testInvalidCacheURL([NSURL URLWithString:@"file:///private/tmp/file.cache"], @"Cache directory `/private/tmp` is not a data vault");
#endif
    }

#if USE(APPLE_INTERNAL_SDK)
    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];

        auto testValidCacheURL = [&](NSURL* cacheURL)
        {
            NSError* error;
            JSScript *script = [JSScript scriptOfType:kJSScriptTypeProgram withSource:@"" andSourceURL:[NSURL URLWithString:@"my-path"] andBytecodeCache:cacheURL inVirtualMachine:context.virtualMachine error:&error];
            NSString* testDesciption = [NSString stringWithFormat:@"Cache path validation for `%@` passed", cacheURL.absoluteString];
            checkResult(testDesciption, script && !error);
        };

        testValidCacheURL(cacheFileInDataVault(@"file.cache"));
    }
#endif
}

@interface JSContextFileLoaderDelegate : JSContext <JSModuleLoaderDelegate>

+ (instancetype)newContext;
- (JSScript *)fetchModuleScript:(NSString *)relativePath;

@end

@implementation JSContextFileLoaderDelegate {
    NSMutableDictionary<NSString *, JSScript *> *m_keyToScript;
}

+ (instancetype)newContext
{
    auto *result = [[JSContextFileLoaderDelegate alloc] init];
    result.moduleLoaderDelegate = result;
    result->m_keyToScript = [[NSMutableDictionary<NSString *, JSScript *> alloc] init];
    return result;
}

static NSURL *resolvePathToScripts()
{
    NSString *arg0 = NSProcessInfo.processInfo.arguments[0];
    NSURL *base;
    if ([arg0 hasPrefix:@"/"])
        base = [NSURL fileURLWithPath:arg0 isDirectory:NO];
    else {
        const size_t maxLength = 10000;
        char cwd[maxLength];
        if (!getcwd(cwd, maxLength)) {
            NSLog(@"getcwd errored with code: %s", safeStrerror(errno).data());
            exit(1);
        }
        NSURL *cwdURL = [NSURL fileURLWithPath:[NSString stringWithFormat:@"%s", cwd]];
        base = [NSURL fileURLWithPath:arg0 isDirectory:NO relativeToURL:cwdURL];
    }
    return [NSURL fileURLWithPath:@"./testapiScripts/" isDirectory:YES relativeToURL:base];
}

- (JSScript *)fetchModuleScript:(NSString *)relativePath
{
    auto *filePath = [NSURL URLWithString:relativePath relativeToURL:resolvePathToScripts()];
    if (auto *script = [self findScriptForKey:[filePath absoluteString]])
        return script;
    NSError *error;
    auto *result = [JSScript scriptOfType:kJSScriptTypeModule memoryMappedFromASCIIFile:filePath withSourceURL:filePath andBytecodeCache:nil inVirtualMachine:[self virtualMachine] error:&error];
    if (!result) {
        NSLog(@"%@\n", error);
        CRASH();
    }
    [m_keyToScript setObject:result forKey:[filePath absoluteString]];
    return result;
}

- (JSScript *)findScriptForKey:(NSString *)key
{
    return [m_keyToScript objectForKey:key];
}

- (void)context:(JSContext *)context fetchModuleForIdentifier:(JSValue *)identifier withResolveHandler:(JSValue *)resolve andRejectHandler:(JSValue *)reject
{
    NSURL *filePath = [NSURL URLWithString:[identifier toString]];
    // FIXME: We should fix this: https://bugs.webkit.org/show_bug.cgi?id=199714
    if (auto *script = [self findScriptForKey:[identifier toString]]) {
        [resolve callWithArguments:@[script]];
        return;
    }

    auto* script = [JSScript scriptOfType:kJSScriptTypeModule
        memoryMappedFromASCIIFile:filePath
        withSourceURL:filePath
        andBytecodeCache:nil 
        inVirtualMachine:context.virtualMachine
        error:nil];
    if (script) {
        [m_keyToScript setObject:script forKey:[identifier toString]];
        [resolve callWithArguments:@[script]];
    } else
        [reject callWithArguments:@[[JSValue valueWithNewErrorFromMessage:@"Unable to create Script" inContext:context]]];
}

@end

static void testLoadBasicFileLegacySPI()
{
    @autoreleasepool {
        auto *context = [JSContextFileLoaderDelegate newContext];
        context.moduleLoaderDelegate = context;
        JSValue *promise = [context evaluateScript:@"import('./basic.js');" withSourceURL:resolvePathToScripts()];
        JSValue *null = [JSValue valueWithNullInContext:context];
        checkModuleCodeRan(context, promise, null);
    }
}


@interface JSContextMemoryMappedLoaderDelegate : JSContext <JSModuleLoaderDelegate>

+ (instancetype)newContext;

@end

@implementation JSContextMemoryMappedLoaderDelegate {
}

+ (instancetype)newContext
{
    auto *result = [[JSContextMemoryMappedLoaderDelegate alloc] init];
    return result;
}

- (void)context:(JSContext *)context fetchModuleForIdentifier:(JSValue *)identifier withResolveHandler:(JSValue *)resolve andRejectHandler:(JSValue *)reject
{
    NSURL *filePath = [NSURL URLWithString:[identifier toString]];
    auto *script = [JSScript scriptOfType:kJSScriptTypeModule memoryMappedFromASCIIFile:filePath withSourceURL:filePath andBytecodeCache:nil inVirtualMachine:context.virtualMachine error:nil];
    if (script)
        [resolve callWithArguments:@[script]];
    else
        [reject callWithArguments:@[[JSValue valueWithNewErrorFromMessage:@"Unable to create Script" inContext:context]]];
}

@end

static void testLoadBasicFile()
{
#if HAS_LIBPROC
    size_t count = proc_pidinfo(getpid(), PROC_PIDLISTFDS, 0, 0, 0);
#endif
    @autoreleasepool {
        auto *context = [JSContextMemoryMappedLoaderDelegate newContext];
        context.moduleLoaderDelegate = context;
        JSValue *promise = [context evaluateScript:@"import('./basic.js');" withSourceURL:resolvePathToScripts()];
        JSValue *null = [JSValue valueWithNullInContext:context];
#if HAS_LIBPROC
        size_t afterCount = proc_pidinfo(getpid(), PROC_PIDLISTFDS, 0, 0, 0);
        checkResult(@"JSScript should not hold a file descriptor", count == afterCount);
#endif
        checkModuleCodeRan(context, promise, null);
    }
#if HAS_LIBPROC
    size_t after = proc_pidinfo(getpid(), PROC_PIDLISTFDS, 0, 0, 0);
    checkResult(@"File descriptor count sholudn't change after context is dealloced", count == after);
#endif
}

@interface JSContextAugmentedLoaderDelegate : JSContext <JSModuleLoaderDelegate>

+ (instancetype)newContext;

@end

@implementation JSContextAugmentedLoaderDelegate {
}

+ (instancetype)newContext
{
    auto *result = [[JSContextAugmentedLoaderDelegate alloc] init];
    return result;
}

- (void)context:(JSContext *)context fetchModuleForIdentifier:(JSValue *)identifier withResolveHandler:(JSValue *)resolve andRejectHandler:(JSValue *)reject
{
    UNUSED_PARAM(reject);

    NSURL *filePath = [NSURL URLWithString:[identifier toString]];
    NSString *pathString = [filePath absoluteString];
    if ([pathString containsString:@"basic.js"] || [pathString containsString:@"foo.js"]) {
        auto *script = [JSScript scriptOfType:kJSScriptTypeModule memoryMappedFromASCIIFile:filePath withSourceURL:filePath andBytecodeCache:nil inVirtualMachine:context.virtualMachine error:nil];
        RELEASE_ASSERT(script);
        [resolve callWithArguments:@[script]];
        return;
    }

    if ([pathString containsString:@"bar.js"]) {
        auto *script = [JSScript scriptOfType:kJSScriptTypeModule withSource:@"" andSourceURL:[NSURL fileURLWithPath:@"/not/path/to/bar.js"] andBytecodeCache:nil inVirtualMachine:context.virtualMachine error:nil];
        RELEASE_ASSERT(script);
        [resolve callWithArguments:@[script]];
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

@end

static void testJSScriptURL()
{
    @autoreleasepool {
        auto *context = [JSContextAugmentedLoaderDelegate newContext];
        context.moduleLoaderDelegate = context;
        NSURL *basic = [NSURL URLWithString:@"./basic.js" relativeToURL:resolvePathToScripts()];
        JSScript *script1 = [JSScript scriptOfType:kJSScriptTypeModule memoryMappedFromASCIIFile:basic withSourceURL:basic andBytecodeCache:nil inVirtualMachine:context.virtualMachine error:nil];

        JSValue *result1 = [context evaluateJSScript:script1];
        JSValue *null = [JSValue valueWithNullInContext:context];
        checkModuleCodeRan(context, result1, null);

        NSURL *foo = [NSURL URLWithString:@"./foo.js" relativeToURL:resolvePathToScripts()];
        JSScript *script2 = [JSScript scriptOfType:kJSScriptTypeModule memoryMappedFromASCIIFile:foo withSourceURL:foo andBytecodeCache:nil inVirtualMachine:context.virtualMachine error:nil];
        RELEASE_ASSERT(script2);
        JSValue *result2 = [context evaluateJSScript:script2];

        __block bool wasRejected = false;
        [result2 invokeMethod:@"catch" withArguments:@[^(JSValue *reason) {
            wasRejected = [reason isObject];
            RELEASE_ASSERT([[reason toString] containsString:@"The same JSScript was provided for two different identifiers"]);
        }]];

        checkResult(@"Module JSScript imported with different identifiers is rejected", wasRejected);
    }
}

static void testDependenciesArray()
{
    @autoreleasepool {
        auto *context = [JSContextFileLoaderDelegate newContext];

        JSScript *entryScript = [context fetchModuleScript:@"./dependencyListTests/dependenciesEntry.js"];

        JSValue *promise = [context evaluateJSScript:entryScript];
        [promise invokeMethod:@"then" withArguments:@[^(JSValue *) {
            checkResult(@"module ran successfully", true);
        }, ^(JSValue *) {
            checkResult(@"module ran successfully", false);
        }]];

        checkResult(@"looking up the entry script should find the same script again.", [context findScriptForKey:[entryScript.sourceURL absoluteString]] == entryScript);

        auto *deps = [context dependencyIdentifiersForModuleJSScript:entryScript];

        checkResult(@"deps should be an array", [deps isArray]);
        checkResult(@"deps should have two entries", [deps[@"length"] isEqualToObject:@(2)]);

        checkResult(@"first dependency should be foo.js", [[[[context fetchModuleScript:@"./dependencyListTests/foo.js"] sourceURL] absoluteString] isEqual:[deps[@(0)] toString]]);
        checkResult(@"second dependency should be bar.js", [[[[context fetchModuleScript:@"./dependencyListTests/bar.js"] sourceURL] absoluteString] isEqual:[deps[@(1)] toString]]);
    }
}

static void testDependenciesEvaluationError()
{
    @autoreleasepool {
        auto *context = [JSContextFileLoaderDelegate newContext];

        JSScript *entryScript = [context fetchModuleScript:@"./dependencyListTests/referenceError.js"];

        JSValue *promise = [context evaluateJSScript:entryScript];
        [promise invokeMethod:@"then" withArguments:@[^(JSValue *) {
            checkResult(@"module failed successfully", false);
        }, ^(JSValue *) {
            checkResult(@"module failed successfully", true);
        }]];

        auto *deps = [context dependencyIdentifiersForModuleJSScript:entryScript];
        checkResult(@"deps should be an Array", [deps isArray]);
        checkResult(@"first dependency should be foo.js", [[[[context fetchModuleScript:@"./dependencyListTests/foo.js"] sourceURL] absoluteString] isEqual:[deps[@(0)] toString]]);
    }
}

static void testDependenciesSyntaxError()
{
    @autoreleasepool {
        auto *context = [JSContextFileLoaderDelegate newContext];

        JSScript *entryScript = [context fetchModuleScript:@"./dependencyListTests/syntaxError.js"];

        JSValue *promise = [context evaluateJSScript:entryScript];
        [promise invokeMethod:@"then" withArguments:@[^(JSValue *) {
            checkResult(@"module failed successfully", false);
        }, ^(JSValue *) {
            checkResult(@"module failed successfully", true);
        }]];

        auto *deps = [context dependencyIdentifiersForModuleJSScript:entryScript];
        checkResult(@"deps should be undefined", [deps isUndefined]);
        checkResult(@"there should be a pending exception on the context", context.exception);
    }
}

static void testDependenciesBadImportId()
{
    @autoreleasepool {
        auto *context = [JSContextFileLoaderDelegate newContext];

        JSScript *entryScript = [context fetchModuleScript:@"./dependencyListTests/badModuleImportId.js"];

        JSValue *promise = [context evaluateJSScript:entryScript];
        [promise invokeMethod:@"then" withArguments:@[^(JSValue *) {
            checkResult(@"module failed successfully", false);
        }, ^(JSValue *) {
            checkResult(@"module failed successfully", true);
        }]];

        auto *deps = [context dependencyIdentifiersForModuleJSScript:entryScript];
        checkResult(@"deps should be undefined", [deps isUndefined]);
        checkResult(@"there should be a pending exception on the context", context.exception);
    }
}

static void testDependenciesMissingImport()
{
    @autoreleasepool {
        auto *context = [JSContextFileLoaderDelegate newContext];

        JSScript *entryScript = [context fetchModuleScript:@"./dependencyListTests/missingImport.js"];

        JSValue *promise = [context evaluateJSScript:entryScript];
        [promise invokeMethod:@"then" withArguments:@[^(JSValue *) {
            checkResult(@"module failed successfully", false);
        }, ^(JSValue *) {
            checkResult(@"module failed successfully", true);
        }]];

        auto *deps = [context dependencyIdentifiersForModuleJSScript:entryScript];
        checkResult(@"deps should be undefined", [deps isUndefined]);
        checkResult(@"there should be a pending exception on the context", context.exception);
    }
}

static void testMicrotaskWithFunction()
{
    @autoreleasepool {
#if PLATFORM(COCOA)
        bool useLegacyDrain = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DoesNotDrainTheMicrotaskQueueWhenCallingObjC);
        if (useLegacyDrain)
            return;
#endif

        JSContext *context = [[JSContext alloc] init];

        JSValue *globalObject = context.globalObject;

        auto block = ^() {
            return 1+1;
        };

        [globalObject setValue:block forProperty:@"setTimeout"];
        JSValue *arr = [context evaluateScript:@"var arr = []; (async () => { await 1; arr.push(3); })(); arr.push(1); setTimeout(); arr.push(2); arr;"];
        checkResult(@"arr[0] should be 1", [arr[@0] toInt32] == 1);
        checkResult(@"arr[1] should be 2", [arr[@1] toInt32] == 2);
        checkResult(@"arr[2] should be 3", [arr[@2] toInt32] == 3);
    }
}

@protocol ToString <JSExport>
- (NSString *)toString;
@end

@interface ToStringClass : NSObject<ToString>
@end

@implementation ToStringClass
- (NSString *)toString
{
    return @"foo";
}
@end

@interface ToStringSubclass : ToStringClass<ToString>
@end

@implementation ToStringSubclass
- (NSString *)toString
{
    return @"baz";
}
@end

@interface ToStringSubclassNoProtocol : ToStringClass
@end

@implementation ToStringSubclassNoProtocol
- (NSString *)toString
{
    return @"baz";
}
@end

static void testToString()
{
    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];

        JSValue *toStringClass = [JSValue valueWithObject:[[ToStringClass alloc] init] inContext:context];
        checkResult(@"exporting a property with the same name as a builtin on Object.prototype should still be exported", [[toStringClass invokeMethod:@"toString" withArguments:@[]] isEqualToObject:@"foo"]);
        checkResult(@"converting an object with an exported custom toObject property should use that method", [[toStringClass toString] isEqualToString:@"foo"]);

        toStringClass = [JSValue valueWithObject:[[ToStringSubclass alloc] init] inContext:context];
        checkResult(@"Calling a method on a derived class should call the derived implementation", [[toStringClass invokeMethod:@"toString" withArguments:@[]] isEqualToObject:@"baz"]);
        checkResult(@"Converting an object with an exported custom toObject property should use that method", [[toStringClass toString] isEqualToString:@"baz"]);
        context[@"toStringValue"] = toStringClass;
        JSValue *hasOwnProperty = [context evaluateScript:@"toStringValue.__proto__.hasOwnProperty('toString')"];
        checkResult(@"A subclass that exports a method exported by a super class shouldn't have a duplicate prototype method", [hasOwnProperty toBool]);

        toStringClass = [JSValue valueWithObject:[[ToStringSubclassNoProtocol alloc] init] inContext:context];
        checkResult(@"Calling a method on a derived class should call the derived implementation even when not exported on the derived class", [[toStringClass invokeMethod:@"toString" withArguments:@[]] isEqualToObject:@"baz"]);
    }
}

#define RUN(test) do { \
        if (!shouldRun(#test)) \
            break; \
        NSLog(@"%s...\n", #test); \
        test; \
        NSLog(@"%s: done.\n", #test); \
    } while (false)

void testObjectiveCAPI(const char* filter)
{
    NSLog(@"Testing Objective-C API");

    auto shouldRun = [&] (const char* test) -> bool {
        if (filter)
            return strcasestr(test, filter);
        return true;
    };

    RUN(checkNegativeNSIntegers());
    RUN(runJITThreadLimitTests());
    RUN(testToString());

    RUN(testLoaderResolvesAbsoluteScriptURL());
    RUN(testFetch());
    RUN(testFetchWithTwoCycle());
    RUN(testFetchWithThreeCycle());
    RUN(testImportModuleTwice());
    RUN(testModuleBytecodeCache());
    RUN(testProgramBytecodeCache());
    RUN(testBytecodeCacheWithSyntaxError(kJSScriptTypeProgram));
    RUN(testBytecodeCacheWithSyntaxError(kJSScriptTypeModule));
    RUN(testBytecodeCacheWithSameCacheFileAndDifferentScript(false));
    RUN(testBytecodeCacheWithSameCacheFileAndDifferentScript(true));
    RUN(testProgramJSScriptException());
    RUN(testCacheFileFailsWhenItsAlreadyCached());
    RUN(testCanCacheManyFilesWithTheSameVM());
    RUN(testIsUsingBytecodeCacheAccessor());
    RUN(testBytecodeCacheValidation());

    RUN(testLoaderRejectsNilScriptURL());
    RUN(testLoaderRejectsFailedFetch());

    RUN(testJSScriptURL());

    // File loading
    RUN(testLoadBasicFileLegacySPI());
    RUN(testLoadBasicFile());

    RUN(testDependenciesArray());
    RUN(testDependenciesSyntaxError());
    RUN(testDependenciesEvaluationError());
    RUN(testDependenciesBadImportId());
    RUN(testDependenciesMissingImport());

    RUN(promiseWithExecutor(Resolution::ResolveEager));
    RUN(promiseWithExecutor(Resolution::RejectEager));
    RUN(promiseWithExecutor(Resolution::ResolveLate));
    RUN(promiseWithExecutor(Resolution::RejectLate));
    RUN(promiseRejectOnJSException());
    RUN(promiseCreateResolved());
    RUN(promiseCreateRejected());
    RUN(parallelPromiseResolveTest());

    RUN(testMicrotaskWithFunction());

    if (!filter)
        testObjectiveCAPIMain();
}

#else

void testObjectiveCAPI(const char*)
{
}

#endif // JSC_OBJC_API_ENABLED
