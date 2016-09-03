//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var three = { val: 3 };
var five = 5;
var six = { val: 6 };

var x = new Map();
var y = x;
y.baz = 5;

var z = new Map();
z.set(1, -1);
z.set(2, -2);
z.set(five, 5);

WScript.SetTimeout(testFunction, 50);

/////////////////

function testFunction()
{
    telemetryLog(`x === y: ${x === y}`, true); //true
    telemetryLog(`x.baz: ${x.baz}`, true); //5
    telemetryLog(`z.has(five): ${z.has(five)}`, true); //true
    telemetryLog(`z.get(five): ${z.get(five)}`, true); //5

    ////
    x.set(three, 3);
    z.delete(five);
    ////

    telemetryLog(`post update 1 -- y.has(three): ${y.has(three)}`, true); //true
    telemetryLog(`post update 1 -- y.get(three): ${y.get(three)}`, true); //3
    telemetryLog(`post update 1 -- z.has(five): ${z.has(five)}`, true); //false

    ////
    z.set(six, 6);
    six = null;

    y.set(three, 4);
    y.set(five, 5);
    ////

    telemetryLog(`post update 2 -- x.has(five): ${x.has(five)}`, true); //true
    telemetryLog(`post update 2 -- x.get(five): ${x.get(five)}`, true); //5
    telemetryLog(`post update 2 -- x.get(three): ${x.get(three)}`, true); //4
}