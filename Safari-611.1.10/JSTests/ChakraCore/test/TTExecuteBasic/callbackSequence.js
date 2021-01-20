//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

telemetryLog("Start Global Code", true);

function foo1()
{
    telemetryLog("Start Foo1", true);
    
    WScript.Echo("Hello World - CallBack 1");
    WScript.SetTimeout(foo2, 200);
    
    telemetryLog("End Foo1", true);
}

function foo2()
{
    telemetryLog("Start Foo2", true);
    
    WScript.Echo("Hello World - CallBack 2");
    WScript.SetTimeout(foo3, 200);

    telemetryLog("End Foo2", true);
}

function foo3()
{
    telemetryLog("Start Foo3", true);
    
    WScript.Echo("Hello World - CallBack 3");
    
    telemetryLog("End Foo3", true);
}

WScript.SetTimeout(foo1, 200);
WScript.Echo("Hello World - Global");

telemetryLog("End Global Code", true);

