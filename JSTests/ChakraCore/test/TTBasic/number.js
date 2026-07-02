//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = 3;
var y = 5;

var xd = 4.6;
var yd = 9.2;

var myInf = Infinity;

WScript.SetTimeout(testFunction, 50);

/////////////////

function testFunction()
{
    telemetryLog(`x: ${x}`, true); //3
    telemetryLog(`y: ${y}`, true); //5
    telemetryLog(`xd: ${xd}`, true); //4.6
    telemetryLog(`yd: ${yd}`, true); //9.2

    telemetryLog(`x + y: ${x + y}`, true); //8
    telemetryLog(`x - y: ${x - y}`, true); //-2
    telemetryLog(`x * y: ${x * y}`, true); //15
    telemetryLog(`x / y: ${x / y}`, true); //0.6

    telemetryLog(`isFinite(xd): ${isFinite(xd)}`, true); //true
    telemetryLog(`isFinite(myInf): ${isFinite(myInf)}`, true); //false
    telemetryLog(`isFinite(Infinity): ${isFinite(Infinity)}`, true); //false

    telemetryLog(`Math.abs(-2): ${Math.abs(-2)}`, true); //2
    telemetryLog(`Math.floor(1.5): ${Math.floor(1.5)}`, true); //1.0
}