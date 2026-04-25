//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo(a){
  "use strict";
  a = 42;
  
  var res = {};
  var args = arguments;
    
  res.a = function() { return a; };
  res.arg = function() { return args[0]; };

  return res;
}

var x = foo(17);

WScript.SetTimeout(testFunction, 50);

/////////////////

function testFunction()
{
    telemetryLog(`a: ${x.a()}`, true); //42
    telemetryLog(`arg: ${x.arg()}`, true); //17
}