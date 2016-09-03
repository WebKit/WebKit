//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var GiantPrintArray = [];
    function makeArrayLength() {
    }
    var obj0 = {};
    var obj1 = {};
    var func0 = function () {
        protoObj0 = obj0;
        var __loopvar2 = 3;
        for (; ; __loopvar2++) {
            if (__loopvar2 === 3 + 3) {
                break;
            }
            function __f() {
                if (obj0.prop0) {
                    GiantPrintArray.push(__loopvar2);
                    Math.sin(Error());
                } else {
                    litObj1 = obj0;
                }
            }
            function __g() {
                __f();
            }
            __f();
        }
    };
    var func1 = function () {
        litObj1.prop0 = obj1;
    };
    var func2 = function () {
        return func0();
    };
    var func3 = function () {
        ary.push(func1(), func0() ? (uniqobj3) : func2());
    };
    obj0.method1 = func3;
    var ary = Array();
    makeArrayLength(func2());
    protoObj0.method1();
    WScript.Echo(GiantPrintArray);
}
test0();
