//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Global function
var x = 1;
function foo() {
  x = 2;
}
WScript.DumpFunctionPosition(foo);

// Function property
var obj = {
  func : function () {
    WScript.Echo('');
  }
};
WScript.DumpFunctionPosition(obj.func);

var global = WScript.LoadScript("function foo(){}", "samethread", "dummyFileName.js");
WScript.DumpFunctionPosition(global.foo);

var evalFunc = eval('new Function("a", "b", "/*some comments\\r\\n*/    return a + b;")');
WScript.DumpFunctionPosition(evalFunc);

/*some function not at 0 column*/function blah() {
  /* First statement not at 0 */
  var xyz = 1;
}
WScript.DumpFunctionPosition(blah);

// Shouldn't get functionPosition of built-ins
WScript.DumpFunctionPosition(JSON.stringify);
WScript.DumpFunctionPosition(eval);

// Shouldn't get functionPosition of library code
WScript.DumpFunctionPosition(Intl.DateTimeFormat);
WScript.Echo("pass");
