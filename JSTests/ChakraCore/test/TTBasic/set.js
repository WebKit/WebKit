//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = new Set();
var y = x;
y.baz = 5;

var z = new Set();
z.add(5);

WScript.SetTimeout(testFunction, 50);

/////////////////

function testFunction()
{
    telemetryLog(`x === y: ${x === y}`, true); //true
    telemetryLog(`x.baz: ${x.baz}`, true); //5
    telemetryLog(`z.size: ${z.size}`, true); //1
    telemetryLog(`z.has(5): ${z.has(5)}`, true); //true

    ////
    x.add(3);
    z.delete(3);
    z.delete(5);
    ////

    telemetryLog(`post update 1 -- y.has(3): ${y.has(3)}`, true); //true
    telemetryLog(`post update 1 -- z.size: ${z.size}`, true); //0
    telemetryLog(`post update 1 -- z.has(5): ${z.has(5)}`, true); //false

    ////
    y.add(3);
    y.add(5);
    ////

    telemetryLog(`post update 2 -- x.has(5): ${x.has(5)}`, true); //true
    telemetryLog(`post update 2 -- x.size: ${x.size}`, true); //2
}