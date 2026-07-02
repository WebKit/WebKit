//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = [];
x[2] = 5;
x.foo = 3;

Object.defineProperty(x, '1', {
    get: function () { return this.foo + 1; },
    set: function (x) { this.foo = x / 2; }
});

Object.defineProperty(x, '11', {
    get: function () { return this.foo; }
});

WScript.SetTimeout(testFunction, 50);

/////////////////

function testFunction()
{
    telemetryLog(`Array.isArray(x): ${Array.isArray(x)}`, true); //true

    telemetryLog(`x.foo: ${x.foo}`, true); //3

    telemetryLog(`x[1]: ${x[1]}`, true); //4
    telemetryLog(`x[11]: ${x[11]}`, true); //3

    ////
    x[1] = 12;
    ////

    telemetryLog(`x[1]: ${x[1]}`, true); //7
    telemetryLog(`x[11]: ${x[11]}`, true); //6
}