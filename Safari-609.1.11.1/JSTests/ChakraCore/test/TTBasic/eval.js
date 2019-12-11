//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var evalCode1;
eval("evalCode1 = function() { return evalCode1 + ' ' + captured; }");

var evalCode2 = undefined;
var evalCode3 = undefined;

var captured = "ok";

function setCode2()
{
    var notCaptured = 5;
    var captured = "bob in setCode2";
    eval("evalCode2 = function() { return evalCode2 + ' ' + captured; }");
    
    eval.call(this, "evalCode3 = function() { return evalCode3 + ' ' + captured; }")
}

setCode2();
WScript.SetTimeout(testFunction, 50);

function testFunction()
{
    telemetryLog(`evalCode1: ${evalCode1()}`, true); //function () { return evalCode1 + ' ' + captured; } ok"
    telemetryLog(`evalCode2: ${evalCode2()}`, true); //function () { return evalCode2 + ' ' + captured; } bob in setCode2"
    
    telemetryLog(`evalCode3: ${evalCode3()}`, true); //function () { return evalCode3 + ' ' + captured; } ok"
}

