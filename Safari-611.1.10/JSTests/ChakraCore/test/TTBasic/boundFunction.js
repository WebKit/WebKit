//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

this.x = 9;
var module = {
    x: 81,
    getX: function (y) { return this.x + y; }
};

var getX = module.getX;
var boundGetX = getX.bind(module, 3);

WScript.SetTimeout(testFunction, 50);

/////////////////

function testFunction()
{
    telemetryLog(`getX(1): ${getX(1)}`, true); //10
    telemetryLog(`module.getX(1): ${module.getX(1)}`, true); //82
    telemetryLog(`boundGetX(): ${boundGetX()}`, true); //84
}