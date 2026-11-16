/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <JavaScriptCore/JavaScriptCore.h>
#import <QuartzCore/QuartzCore.h>

static unsigned ID_HANDLER_A = 2;
static unsigned ID_HANDLER_B = 3;
static unsigned DATA_SIZE = 4;

typedef struct {
    JSObjectRef scheduler;
    int v1;
    int v2;
} WorkerTask;

JSValueRef WorkerTask_run(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(ctx);
    
    JSValueRef empty = NULL;
    if (!exception)
        exception = &empty;
    else
        *exception = empty;
    JSValueRef packetValue = arguments[0];
    
    WorkerTask* task = JSObjectGetPrivate(thisObject);
    if (JSValueIsUndefined(ctx, packetValue) || JSValueIsNull(ctx, packetValue)) {
        JSStringRef suspendCurrentString = JSStringCreateWithUTF8CString("suspendCurrent");
        JSValueRef suspendCurrentValue = JSObjectGetProperty(ctx, task->scheduler, suspendCurrentString, exception);
        JSStringRelease(suspendCurrentString);
        if (*exception)
            return JSValueMakeUndefined(ctx);
        JSObjectRef suspendCurrentObject = JSValueToObject(ctx, suspendCurrentValue, exception);
        if (*exception)
            return JSValueMakeUndefined(ctx);
        return JSObjectCallAsFunction(ctx, suspendCurrentObject, task->scheduler, 0, NULL, exception);
    }
    
    JSObjectRef packetObject = JSValueToObject(ctx, packetValue, exception);
    if (*exception)
        return JSValueMakeUndefined(ctx);

    task->v1 = ID_HANDLER_A + ID_HANDLER_B - task->v1;
    
    JSStringRef idString = JSStringCreateWithUTF8CString("id");
    JSObjectSetProperty(ctx, packetObject, idString, JSValueMakeNumber(ctx, task->v1), kJSPropertyAttributeNone, exception);
    JSStringRelease(idString);
    if (*exception)
        return JSValueMakeUndefined(ctx);
    
    JSStringRef a1String = JSStringCreateWithUTF8CString("a1");
    JSObjectSetProperty(ctx, packetObject, a1String, JSValueMakeNumber(ctx, 0), kJSPropertyAttributeNone, exception);
    JSStringRelease(a1String);
    if (*exception)
        return JSValueMakeUndefined(ctx);

    JSStringRef a2String = JSStringCreateWithUTF8CString("a2");
    JSValueRef a2Value = JSObjectGetProperty(ctx, packetObject, a2String, exception);
    JSStringRelease(a2String);
    if (*exception)
        return JSValueMakeUndefined(ctx);
    JSObjectRef a2Object = JSValueToObject(ctx, a2Value, exception);
    if (*exception)
        return JSValueMakeUndefined(ctx);
    
    for (unsigned i = 0; i < DATA_SIZE; ++i) {
        if (++task->v2 > 26)
            task->v2 = 1;
        JSObjectSetPropertyAtIndex(ctx, a2Object, i, JSValueMakeNumber(ctx, task->v2), exception);
        if (*exception)
            return JSValueMakeUndefined(ctx);
    }
    
    JSStringRef queueString = JSStringCreateWithUTF8CString("queue");
    JSValueRef queueValue = JSObjectGetProperty(ctx, task->scheduler, queueString, exception);
    JSStringRelease(queueString);
    if (*exception)
        return JSValueMakeUndefined(ctx);
    JSObjectRef queueObject = JSValueToObject(ctx, queueValue, exception);
    if (*exception)
        return JSValueMakeUndefined(ctx);
    JSValueRef queueArguments[] = { packetValue };
    return JSObjectCallAsFunction(ctx, queueObject, task->scheduler, 1, queueArguments, exception);
}

JSClassRef workerTaskClass;

JSObjectRef WorkerTask_constructor(JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    WorkerTask* task = (WorkerTask*)malloc(sizeof(WorkerTask));
    task->scheduler = JSValueToObject(ctx, arguments[0], exception);
    task->v1 = JSValueToNumber(ctx, arguments[1], exception);
    task->v2 = JSValueToNumber(ctx, arguments[2], exception);
    return JSObjectMake(ctx, workerTaskClass, task);
}

JSValueRef WorkerTask_getProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    if (JSStringIsEqualToUTF8CString(propertyName, "run"))
        return JSObjectMakeFunctionWithCallback(ctx, propertyName, WorkerTask_run);
    return JSValueMakeUndefined(ctx);
}

int main()
{
    JSGlobalContextRef context = JSGlobalContextCreate(NULL);
    if (!context)
        exit(1);
    
    JSValueRef exception = NULL;
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = "WorkerTask";
    definition.callAsConstructor = WorkerTask_constructor;
    definition.getProperty = WorkerTask_getProperty;
    workerTaskClass = JSClassCreate(&definition);
    JSObjectRef workerTaskObject = JSObjectMake(context, workerTaskClass, &exception);
    if (exception)
        exit(1);
    
    JSStringRef workerTaskString = JSStringCreateWithUTF8CString("WorkerTask");
    
    JSObjectSetProperty(context, globalObject, workerTaskString, workerTaskObject, kJSPropertyAttributeNone, &exception);
    JSStringRelease(workerTaskString);
    if (exception)
        exit(1);
    
    JSClassRelease(workerTaskClass);

    NSString* source = [NSString stringWithContentsOfFile:@"richards.js" encoding:NSUTF8StringEncoding error:nil];
    JSStringRef script = JSStringCreateWithUTF8CString(source.UTF8String);
    JSEvaluateScript(context, script, NULL, NULL, 0, &exception);
    if (exception)
        exit(1);
    JSStringRelease(script);

    unsigned iterations = 50;
    CFTimeInterval start = CACurrentMediaTime();
    for (unsigned i = 1; i <= iterations; ++i) {
        JSStringRef runRichardsString = JSStringCreateWithUTF8CString("runRichards");
        JSValueRef runner = JSObjectGetProperty(context, globalObject, runRichardsString, &exception);
        if (exception)
            exit(1);
        JSObjectRef runnerObject = JSValueToObject(context, runner, &exception);
        if (exception)
            exit(1);
        JSObjectCallAsFunction(context, runnerObject, globalObject, 0, NULL, &exception);
        if (exception)
            exit(1);
        JSStringRelease(runRichardsString);
    }
    CFTimeInterval end = CACurrentMediaTime();
    printf("%d\n", (int)((end-start) * 1000));
    
    JSGlobalContextRelease(context);

    return EXIT_SUCCESS;
}
