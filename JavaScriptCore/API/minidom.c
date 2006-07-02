// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "JavaScriptCore.h"
#include "JSNode.h"
#include <wtf/UnusedParam.h>

static char* createStringWithContentsOfFile(const char* fileName);
static JSValueRef print(JSContextRef context, JSObjectRef object, JSObjectRef thisObject, size_t argc, JSValueRef argv[]);

int main(int argc, char* argv[])
{
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);
    
    JSContextRef context = JSContextCreate(NULL, NULL);
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    
    JSCharBufferRef printBuf = JSCharBufferCreateUTF8("print");
    JSObjectSetProperty(context, globalObject, printBuf, JSFunctionMake(context, print), kJSPropertyAttributeNone);
    JSCharBufferRelease(printBuf);
    
    JSCharBufferRef nodeBuf = JSCharBufferCreateUTF8("Node");
    JSObjectSetProperty(context, globalObject, nodeBuf, JSConstructorMake(context, JSNode_construct), kJSPropertyAttributeNone);
    JSCharBufferRelease(nodeBuf);
    
    char* script = createStringWithContentsOfFile("minidom.js");
    JSCharBufferRef scriptBuf = JSCharBufferCreateUTF8(script);
    JSValueRef result;
    JSEvaluate(context, globalObject, scriptBuf, NULL, 0, &result);

    if (JSValueIsUndefined(result))
        printf("PASS: Test script executed successfully.\n");
    else {
        printf("FAIL: Test script returned unexcpected value:\n");
        JSCharBufferRef resultBuf = JSValueCopyStringValue(context, result);
        CFStringRef resultCF = CFStringCreateWithJSCharBuffer(kCFAllocatorDefault, resultBuf);
        CFShow(resultCF);
        CFRelease(resultCF);
        JSCharBufferRelease(resultBuf);
    }
    JSCharBufferRelease(scriptBuf);
    free(script);

#if 0 // used for leak/finalize debugging    
    int i;
    for (i = 0; i < 1000; i++) {
        JSObjectRef o = JSObjectMake(context, NULL, NULL);
        (void)o;
    }
    JSGCCollect();
#endif
    
    JSContextDestroy(context);
    printf("PASS: Program exited normally.\n");
    return 0;
}

static JSValueRef print(JSContextRef context, JSObjectRef object, JSObjectRef thisObject, size_t argc, JSValueRef argv[])
{
    if (argc > 0) {
        JSCharBufferRef stringBuf = JSValueCopyStringValue(context, argv[0]);
        size_t numChars = JSCharBufferGetMaxLengthUTF8(stringBuf);
        char string[numChars];
        JSCharBufferGetCharactersUTF8(stringBuf, string, numChars);
        printf("%s\n", string);
    }
    
    return JSUndefinedMake();
}

static char* createStringWithContentsOfFile(const char* fileName)
{
    char* buffer;
    
    int buffer_size = 0;
    int buffer_capacity = 1024;
    buffer = (char*)malloc(buffer_capacity);
    
    FILE* f = fopen(fileName, "r");
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", fileName);
        return 0;
    }
    
    while (!feof(f) && !ferror(f)) {
        buffer_size += fread(buffer + buffer_size, 1, buffer_capacity - buffer_size, f);
        if (buffer_size == buffer_capacity) { // guarantees space for trailing '\0'
            buffer_capacity *= 2;
            buffer = (char*)realloc(buffer, buffer_capacity);
            assert(buffer);
        }
        
        assert(buffer_size < buffer_capacity);
    }
    fclose(f);
    buffer[buffer_size] = '\0';
    
    return buffer;
}
