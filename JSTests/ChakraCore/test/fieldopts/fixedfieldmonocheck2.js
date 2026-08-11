//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    function leaf() {
    }
    var obj0 = {};
    var obj1 = {};
    var arrObj0 = {};
    var func1 = function () {
        (function () {
            while (this) {
                arrObj0.prop5 = { 6: arrObj0.prop1 };
                for (; arrObj0.prop5.prop1; i32) {
                }
                if (78) {
                    leaf(arguments);
                    break;
                }
            }
        }());
    };
    var func2 = function () {
        eval();
    };
    obj0.method0 = func1;
    obj0.method1 = obj0.method0;
    obj1.method1 = obj0.method1;
    var ary = Array();
    var i32 = new Int32Array();
    arrObj0.prop1 = -195;
    obj0.method0();
    function v37() {
        for (var __loopvar1001 = 7; obj1.method1() ;) {
        }
    }
    var v44 = v37();
}
test0();
test0();
test0();

WScript.Echo('pass');