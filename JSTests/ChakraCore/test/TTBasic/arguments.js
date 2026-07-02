//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = foo(1, 2, 3);
var y = fooDeleted(1, 2, 3);

function foo(a, b, c)
{
    var res = {};
    var args = arguments;
    
    res.length = function() { return args.length; };
    res.named = function() { return b; };
    res.position = function() { return args[1]; };
   
    return res;
}

function fooDeleted(a, b, c)
{
    delete arguments[1];

    var res = {};
    var args = arguments;
    
    res.length = function() { return args.length; };
    res.named = function() { return b; }; 
    res.position = function() { return args[1]; };
   
    return res;
}

WScript.SetTimeout(testFunction, 20);

function testFunction()
{
    telemetryLog(`xlength: ${x.length()}`, true); //3
    telemetryLog(`xnamed: ${x.named()}`, true); //2
    telemetryLog(`xposition: ${x.position()}`, true); //2
    
    telemetryLog(`ylength: ${y.length()}`, true); //3
    telemetryLog(`ynamed: ${y.named()}`, true); //2
    telemetryLog(`yposition: ${y.position()}`, true); //undefined
}


