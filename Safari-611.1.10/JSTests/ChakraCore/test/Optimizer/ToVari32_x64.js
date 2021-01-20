//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
function test0() {
    var loopInvariant = shouldBailout ? 12 : 8;
    var GiantPrintArray = [];
    var protoObj0 = {};
    var obj1 = {};
    var protoObj1 = {};
    var func4 = function () {
    };
    for (var __loopvar0 = loopInvariant; __loopvar0 != loopInvariant + 4; loopInvariant) {
        var __loopvar1 = loopInvariant;
        for (var __loopSecondaryVar1_0 = loopInvariant; ; loopInvariant) {
            while (obj1.prop0) {
                var __loopvar3 = loopInvariant;
                do {
                    var v0 = protoObj1[{}];
                    protoObj1 = protoObj0;
                    var uniqobj1 = [obj1];
                    GiantPrintArray.push(__loopvar0);
                    func4();
                    if (__loopvar3 > loopInvariant + 6) {
                    }
                    __loopvar3 += 2;
                } while (protoObj0);
                GiantPrintArray('arrObj0.prop0 = ' + arrObj0);
                GiantPrintArray('protoObj1.prop0 = ' + protoObj0);
            }
            if (__loopvar1 === loopInvariant) {
                break;
            }
            __loopvar1++;
        }
        __loopvar0++;
    }
}
test0();
test0();
test0();
test0();

WScript.Echo('pass');