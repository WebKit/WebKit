//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = true;
var y = false;

WScript.SetTimeout(testFunction, 50);

/////////////////

function testFunction()
{
    telemetryLog(`x: ${x}`, true); //true
    telemetryLog(`y: ${y}`, true); //false

    telemetryLog(`x === y: ${x === y}`, true); //false

    telemetryLog(`x && y: ${x && y}`, true); //false
    telemetryLog(`x || y: ${x || y}`, true); //true
}