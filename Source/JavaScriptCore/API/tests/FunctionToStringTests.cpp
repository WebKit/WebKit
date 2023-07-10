/*
 * Copyright (C) 2023 Colin Vidal <colin@cvidal.org> All rights reserved.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "FunctionToStringTests.h"

#include "InitializeThreading.h"
#include "JavaScript.h"
#include <stdio.h>

int testFunctionToString()
{
    const auto inputScript = R"(
        var valid = true;

        function foo2   () {}
        valid &&= foo2.toString() == "function foo2   () {}"

        function       foo3()   {}
        valid &&= foo3.toString() == "function       foo3()   {}"

        function*  fooGen   (){}
        valid &&= fooGen.toString() == "function*  fooGen   (){}"

        async function fnasync() {}
        valid &&= fnasync.toString() == "async function fnasync() {}"

        let f1 = async function() {}
        valid &&= f1.toString() == "async function() {}"

        let f2 = async()=>{}
        valid &&= f2.toString() == "async()=>{}"

        let f3 = async  ()    =>     {}
        valid &&= f3.toString() == "async  ()    =>     {}"

        let asyncGenExpr = async function*()  {}
        valid &&= asyncGenExpr.toString() == "async function*()  {}"

        async function* asyncGenDecl() {}
        valid &&= asyncGenDecl.toString() == "async function* asyncGenDecl() {}"

        async  function  *  fga  (  x  ,  y  )  {  ;  ;  }
        valid &&= fga.toString() == "async  function  *  fga  (  x  ,  y  )  {  ;  ;  }"

        let fDeclAndExpr = { async f  (  )  {  } }.f;
        valid &&= fDeclAndExpr.toString() == "async f  (  )  {  }"

        let fAsyncGenInStaticMethod = class { static async  *  f  (  )  {  } }.f
        valid &&= fAsyncGenInStaticMethod.toString() == "async  *  f  (  )  {  }"

        let fPropFuncGen = { *  f  (  )  {  } }.f;
        valid &&= fPropFuncGen.toString() == "*  f  (  )  {  }"

        let fGetter = Object.getOwnPropertyDescriptor(class { static get  f  (  )  {  } }, "f").get
        valid &&= fGetter.toString() == "get  f  (  )  {  }"

        let g = class { static [  "g"  ]  (  )  {  } }.g
        valid &&= g.toString() == '[  "g"  ]  (  )  {  }'

        let fMethodObject = { f  (  )  {  } }.f
        valid &&= fMethodObject.toString() == "f  (  )  {  }"

        let fComputedProp = { [  "f"  ]  (  )  {  } }.f
        valid &&= fComputedProp.toString() == '[  "f"  ]  (  )  {  }'
    
        let gAsyncPropFunc = { async  [  "g"  ]  (  )  {  } }.g
        valid &&= gAsyncPropFunc.toString() == 'async  [  "g"  ]  (  )  {  }'
    
        valid
    )";

    JSC::initialize();

    JSGlobalContextRef context = JSGlobalContextCreateInGroup(nullptr, nullptr);
    JSStringRef script = JSStringCreateWithUTF8CString(inputScript);
    JSValueRef exception = nullptr;
    JSValueRef resultRef = JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
    JSStringRelease(script);

    auto failed = false;
    if (!JSValueIsBoolean(context, resultRef) || !JSValueToBoolean(context, resultRef))
        failed = true;

    JSGlobalContextRelease(context);
    printf("%s: function toString tests.\n", failed ? "FAIL" : "PASS");

    return failed;
}
