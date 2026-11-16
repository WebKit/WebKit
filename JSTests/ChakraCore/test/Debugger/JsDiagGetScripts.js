//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var global = WScript.LoadScript("", "samethread", "dummyFileName.js");
eval('var x = "code in eval"');
eval('eval(\'var y = "Eval inside eval"\');');
var func = new Function("a", "b", "return a + b;");
var x = 1;/**bp:dumpSourceList();**/
WScript.Echo("pass");
