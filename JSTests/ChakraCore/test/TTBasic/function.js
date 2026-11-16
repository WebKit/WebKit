//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function f()
{
    return "called f";
}
f.foo = 3;
            
var g = f;

WScript.SetTimeout(testFunction, 50);

/////////////////

function testFunction()
{
    telemetryLog(`f !== null: ${f !== null}`, true); //true
    telemetryLog(`f === g: ${f === g}`, true); //true
    telemetryLog(`g.foo: ${g.foo}`, true); //3

    telemetryLog(`f(): ${f()}`, true); //called f
    telemetryLog(`g(): ${g()}`, true); //called f
}