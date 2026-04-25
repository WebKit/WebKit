//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = 'Hello';

var xs = Symbol("Hello");
var ys = xs;

var zs = Symbol("Hello");

var obj = {};
obj[x] = 1;
obj[xs] = 2;
obj[zs] = 3;

var symObj = Object(zs);

WScript.SetTimeout(testFunction, 50);

/////////////////

function testFunction()
{
    telemetryLog(`typeof zs: ${typeof(zs)}`, true); //symbol
    telemetryLog(`typeof symObj: ${typeof(symObj)}`, true); //object
    
    telemetryLog(`xs == ys: ${xs == ys}`, true); //true
    telemetryLog(`xs == zs: ${xs == zs}`, true); //false

    telemetryLog(`obj[x]: ${obj[x]}`, true); //1
    telemetryLog(`obj.Hello: ${obj.Hello}`, true); //1

    telemetryLog(`obj[xs]: ${obj[xs]}`, true); //2
    telemetryLog(`obj[ys]: ${obj[ys]}`, true); //2

    telemetryLog(`obj[zs]: ${obj[zs]}`, true); //3
    telemetryLog(`obj[symObj]: ${obj[symObj]}`, true); //3
}