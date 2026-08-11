//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var xi = [2, 1, 0];
var xd = [2.5, 1.5, 0.5];

WScript.SetTimeout(testFunction, 50);

/////////////////

function testFunction()
{
    telemetryLog(`Array.isArray(xi): ${Array.isArray(xi)}`, true); //true
    telemetryLog(`xi.length: ${xi.length}`, true); //3
    telemetryLog(`xi[1]: ${xi[1]}`, true); //1

    ////
    xi[1] = 5;
    xi.push(10);
    ////

    telemetryLog(`post update -- xi[1]: ${xi[1]}`, true); //5
    telemetryLog(`post update -- xi.length: ${xi.length}`, true); //4
    telemetryLog(`post update -- xi[3]: ${xi[3]}`, true); //10
    telemetryLog(`post update -- xi[5]: ${xi[5]}`, true); //undefined

    telemetryLog(`Array.isArray(xd): ${Array.isArray(xd)}`, true); //true
    telemetryLog(`xd.length: ${xd.length}`, true); //3
    telemetryLog(`xd[1]: ${xd[1]}`, true); //1.5

    ////
    xd[1] = 5.0;
    xd.push(10.0);
    xd[10] = 10.0;
    ////

    telemetryLog(`post update -- xd[1]: ${xd[1]}`, true); //5.0
    telemetryLog(`post update -- xd.length: ${xd.length}`, true); //11
    telemetryLog(`post update -- xd[3]: ${xd[3]}`, true); //10.0
    telemetryLog(`post update -- xd[5]: ${xd[5]}`, true); //undefined
    telemetryLog(`post update -- xd[10]: ${xd[10]}`, true); //10.0
}