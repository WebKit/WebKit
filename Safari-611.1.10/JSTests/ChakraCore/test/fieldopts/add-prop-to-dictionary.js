//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var GiantPrintArray = [];
var obj0 = {};
var litObj0 = { prop1: 3.14159265358979 };
var func0 = function () {
    var fPolyProp = function (o) {
        WScript.Echo(o.prop1 + o.prop2);
    };
    fPolyProp(litObj0);
};
var func1 = function () {
    litObj1 = litObj0;
    while (func0()) {
    }
};
obj0.method1 = func1;
var IntArr1 = Array();
protoObj0 = Object(obj0);
(function (argMath9) {
    litObj0.v18 = argMath9;
    litObj0.v19 = argMath9;
}());
var func9 = function () {
    protoObj0.method1();
};
var v50 = {};
Object.defineProperty(v50, '__getterprop4', {
    get: function () {
        delete litObj0.prop0;
        IntArr1.push(parseInt(func9()), litObj0.prop2 = -107);
    }
});
var uniqobj21 = [protoObj0];
var uniqobj22 = uniqobj21[0];
uniqobj22.method1();
var uniqobj23 = [protoObj0];
var uniqobj24 = uniqobj23[0];
uniqobj24.method1();
GiantPrintArray.push(v50.__getterprop4);
typeof func1.call(litObj1);
