/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "JSWrapperMapTests.h"

#import "APICast.h"
#import "HeapCellInlines.h"
#import "JSGlobalObjectInlines.h"
#import "JSValue.h"

#if JSC_OBJC_API_ENABLED

extern "C" void checkResult(NSString *description, bool passed);

@protocol TestClassJSExport <JSExport>
- (instancetype)init;
@end

@interface TestClass : NSObject <TestClassJSExport>
@end

@implementation TestClass
@end


@interface JSWrapperMapTests : NSObject
+ (void)testStructureIdentity;
@end


@implementation JSWrapperMapTests
+ (void)testStructureIdentity
{
    JSContext* context = [[JSContext alloc] init];
    JSGlobalContextRef contextRef = JSGlobalContextRetain(context.JSGlobalContextRef);
    JSC::JSGlobalObject* globalObject = toJS(contextRef);
    JSC::VM& vm = globalObject->vm();

    context[@"TestClass"] = [TestClass class];
    JSValue* aWrapper = [context evaluateScript:@"new TestClass()"];
    JSValue* bWrapper = [context evaluateScript:@"new TestClass()"];
    JSC::JSValue aValue = toJS(globalObject, aWrapper.JSValueRef);
    JSC::JSValue bValue = toJS(globalObject, bWrapper.JSValueRef);
    JSC::Structure* aStructure = aValue.structureOrNull(vm);
    JSC::Structure* bStructure = bValue.structureOrNull(vm);
    checkResult(@"structure should not be null", !!aStructure);
    checkResult(@"both wrappers should share the same structure", aStructure == bStructure);
}
@end

void runJSWrapperMapTests()
{
    @autoreleasepool {
        [JSWrapperMapTests testStructureIdentity];
    }
}

#endif // JSC_OBJC_API_ENABLED
