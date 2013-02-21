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

#import "JavaScriptCore.h"

extern "C" bool _Block_has_signature(id);
extern "C" const char * _Block_signature(id);

extern int failed;
extern "C" void testObjectiveCAPI(void);

#if JSC_OBJC_API_ENABLED

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
    [function callWithArguments:[NSArray arrayWithObject:[NSNumber numberWithInt:42]]];
}
@end

bool testXYZTested = false;

@protocol TextXYZ <JSExport>
@property int x;
@property (readonly) int y;
- (void)test:(NSString *)message;
@end

@interface TextXYZ : NSObject <TextXYZ>
@property int x;
@property int y;
@property int z;
@end

@implementation TextXYZ
@synthesize x;
@synthesize y;
@synthesize z;
- (void)test:(NSString *)message
{
    testXYZTested = [message isEqual:@"test"] && x == 13 & y == 4 && z == 5;
}
@end

static void checkResult(NSString *description, bool passed)
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

void testObjectiveCAPI()
{
    NSLog(@"Testing Objective-C API");

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *result = [context evaluateScript:@"2 + 2"];
        checkResult(@"2 + 2", [result isNumber] && [result toInt32] == 4);
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
        checkResult(@"Hello, World!", [result isString] && [result isEqualToObject:@"Hello, World!"]);
    }

    @autoreleasepool {
        JSContext *context = [[JSContext alloc] init];
        JSValue *result = [context evaluateScript:@"({ x:42 })"];
        checkResult(@"({ x:42 })", [result isObject] && [result[@"x"] isEqualToObject:@42]);
        id obj = [result toObject];
        checkResult(@"Check dictionary literal", [obj isKindOfClass:[NSDictionary class]]);
        id num = (NSDictionary *)obj[@"x"];
        checkResult(@"Check numeric literal", [num isKindOfClass:[NSNumber class]]);
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
        checkResult(@"mulAddFunction", [result isObject] && [[result toString] isEqual:@"43,44,46"]);
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

        checkResult(@"valueAtIndex:0 is undefined", [[array valueAtIndex:0] isUndefined]);
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
        checkResult(@"String(TestObject)", [result isEqualToObject:@"[object TestObjectConstructor]"]);
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
        checkResult(@"testObjectA == testObjectB", [result isBoolean] && [result toBool]);
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
        checkResult(@"mul(testObject.six, 7)", [result isNumber] && [result toInt32] == 42);
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
        checkResult(@"testObject.getString()", [result isString] && [result toInt32] == 42);
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
        checkResult(@"testObject.getString.call(testObject)", [result isString] && [result toInt32] == 42);
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
        checkResult(@"testObject.callback", [result isNumber] && [result toInt32] == 42);
    }

    @autoreleasepool {
        JSContext *context1 = [[JSContext alloc] init];
        JSContext *context2 = [[JSContext alloc] initWithVirtualMachine:context1.virtualMachine];
        JSValue *value = [JSValue valueWithDouble:42 inContext:context2];
        context1[@"passValueBetweenContexts"] = value;
        JSValue *result = [context1 evaluateScript:@"passValueBetweenContexts"];
        checkResult(@"[value isEqualToObject:result]", [value isEqualToObject:result]);
    }
}

#else

void testObjectiveCAPI()
{
}

#endif
