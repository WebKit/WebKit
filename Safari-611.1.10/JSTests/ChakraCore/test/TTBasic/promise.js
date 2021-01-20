//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var p1 = new Promise((resolve, reject) => resolve(5));  
var p2 = p1.then((val) => { return val + 1; }); // 5  

var p3 = new Promise((resolve, reject) => resolve(10));

var v1 = undefined;
var v2 = undefined;
var v3 = undefined;

function doIt()
{
    p1.then((val) => v1 = val);
    p2.then((val) => v2 = val);
    p3.then((val) => v3 = val);
    
    WScript.SetTimeout(testFunction, 50);
}

WScript.SetTimeout(doIt, 50);

function testFunction()
{            
    telemetryLog(`v1: ${v1}`, true); //5
    telemetryLog(`v2: ${v2}`, true); //6
    telemetryLog(`v3: ${v3}`, true); //10
}


