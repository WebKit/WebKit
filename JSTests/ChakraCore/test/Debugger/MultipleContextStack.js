//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ScriptFunc1()
// ScriptFunc2()
// Script1Func1()
// Script1Func2()
// Script2Func1()
// Script2Func2()
// ScriptFunc3()
// Script1Func3()
// Script2Func3()

var script1 = WScript.LoadScript("\
  var scriptFunc2; \
  var scriptFunc3; \
  function Script1Func1() { scriptFunc2(); } \
  function Script1Func2() { Script1Func1(); } \
  function Script1Func3() { scriptFunc3(); } \
  function setFunc2(func) { scriptFunc2 = func; } \
  function setFunc3(func) { scriptFunc3 = func; }",
  "samethread", "Script1.js");
  
var script2 = WScript.LoadScript(" \
  var script1Func2; \
  var script1Func3; \
  function Script2Func1() { script1Func2(); } \
  function Script2Func2() { Script2Func1(); } \
  function Script2Func3() { script1Func3(); } \
  function setFunc2(func) { script1Func2 = func; } \
  function setFunc3(func) { script1Func3 = func; }",
  "samethread", "Script2.js");
  
function Func2() {
  Func1();
}

function Func3() {
  script2.Script2Func2();
}

function Func1() {
  var x = 1; /**bp:stack();locals(1);**/;
}

script2.setFunc2(script1.Script1Func2);
script1.setFunc2(Func2);
script1.setFunc3(Func3);
script2.setFunc3(script1.Script1Func3);
script2.Script2Func3();
WScript.Echo("pass");
