//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var functionCode1 = new Function("arg", "return arg;");
var functionCode2 = undefined;

var captured = "ok";

function setCode2()
{
    var captured = "bob in setCode2";
    
    functionCode2 = new Function("arg", "return arg + ' ' + captured;");
}

setCode2();
WScript.SetTimeout(testFunction, 50);

function testFunction()
{
    telemetryLog(`functionCode1: ${functionCode1("Hello")}`, true); //Hello
    telemetryLog(`functionCode2: ${functionCode2("Hello")}`, true); //Hello ok
}

