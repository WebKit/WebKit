//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = { foo: 3, bar: null };

Object.defineProperty(x, "b", {
    get: function () { return this.foo + 1; },
    set: function (x) { this.foo = x / 2; }
});

Object.defineProperty(x, "onlyone", {
    get: function () { return this.bar; }
});

WScript.SetTimeout(testFunction, 50);

/////////////////

function testFunction()
{
    telemetryLog(`typeof (x): ${typeof (x)}`, true); //object

    telemetryLog(`x.foo: ${x.foo}`, true); //3
    telemetryLog(`x.b: ${x.b}`, true); //4

    telemetryLog(`x.onlyone: ${x.onlyone}`, true); //null

    ////
    x.b = 12;
    ////

    telemetryLog(`x.foo: ${x.foo}`, true); //6
    telemetryLog(`x.b: ${x.b}`, true); //7
}