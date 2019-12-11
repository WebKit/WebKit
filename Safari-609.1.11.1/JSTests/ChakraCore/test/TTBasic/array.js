//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = [3, null, {}];
var y = x;
y.baz = 5;

WScript.SetTimeout(testFunction, 50);

/////////////////

function testFunction()
{
    telemetryLog(`Array.isArray(x): ${Array.isArray(x)}`, true);  //true
    telemetryLog(`x.length: ${x.length}`, true); //3

    telemetryLog(`x === y: ${x === y}`, true); //true
    telemetryLog(`x.baz: ${x.baz}`, true); //5
    telemetryLog(`x[0]: ${x[0]}`, true); //3
    telemetryLog(`y[1]: ${y[1]}`, true); //null
    telemetryLog(`x[5]: ${x[5]}`, true); //undefined

    ////
    x[1] = "non-null";
    x[5] = { bar: 3 };
    x.push(10);
    ////

    telemetryLog(`post update -- y[1]: ${y[1]}`, true); //non-null
    telemetryLog(`post update -- x[5] !== null: ${x[5] !== null}`, true); //true
    telemetryLog(`post update -- x[5].bar: ${x[5].bar}`, true); //3
    telemetryLog(`post update -- y[6]: ${y[6]}`, true); //10
}