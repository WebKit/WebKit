/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MultithreadedMultiVMExecutionTest.h"

#include "InitializeThreading.h"
#include "JSContextRefPrivate.h"
#include "JavaScript.h"
#include "Options.h"
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <wtf/MainThread.h>

static int failuresFound = 0;

static std::vector<std::thread>& threadsList()
{
    static std::vector<std::thread>* list;
    static std::once_flag flag;
    std::call_once(flag, [] () {
        list = new std::vector<std::thread>();
    });
    return *list;
}

void startMultithreadedMultiVMExecutionTest()
{
    WTF::initializeMainThread();
    JSC::initializeThreading();

#define CHECK(condition, message) do { \
        if (!condition) { \
            printf("FAILED MultithreadedMultiVMExecutionTest: %s\n", message); \
            failuresFound++; \
        } \
    } while (false)

    auto task = [&]() {
        int ret = 0;
        std::string scriptString =
            "const AAA = {A:0, B:1, C:2, D:3};"
            "class Preconditions { static checkArgument(e,t) { if (!e) throw t }};"
            "1 + 2";

        for (int i = 0; i < 1000; ++i) {
            JSClassRef jsClass = JSClassCreate(&kJSClassDefinitionEmpty);
            CHECK(jsClass, "global object class creation");
            JSContextGroupRef contextGroup = JSContextGroupCreate();
            CHECK(contextGroup, "group creation");
            JSGlobalContextRef context = JSGlobalContextCreateInGroup(contextGroup, jsClass);
            CHECK(context, "ctx creation");

            JSStringRef jsScriptString = JSStringCreateWithUTF8CString(scriptString.c_str());
            CHECK(jsScriptString, "script to jsString");

            JSValueRef jsScript = JSEvaluateScript(context, jsScriptString, nullptr, nullptr, 0, nullptr);
            CHECK(jsScript, "script eval");
            JSStringRelease(jsScriptString);

            JSGlobalContextRelease(context);
            JSContextGroupRelease(contextGroup);
            JSClassRelease(jsClass);
        }

        return ret;
    };
    for (int t = 0; t < 8; ++t)
        threadsList().push_back(std::thread(task));
}

int finalizeMultithreadedMultiVMExecutionTest()
{
    auto& threads = threadsList();
    for (auto& thread : threads)
        thread.join();

    if (failuresFound)
        printf("FAILED MultithreadedMultiVMExecutionTest\n");
    return (failuresFound > 0);
}
