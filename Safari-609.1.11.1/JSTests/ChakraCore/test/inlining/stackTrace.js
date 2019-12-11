//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if (this.WScript && this.WScript.LoadScriptFile) {
    this.WScript.LoadScriptFile("TrimStackTracePath.js");
}

function Dump(output)
{
  if (this.WScript)
  {
    WScript.Echo(output);
  }
  else
  {
    alert(output);
  }
}

function test0() {
  var obj0 = {};
 
  var func4 = function () {
    var a = ui8;
    func0();
  };
  var func0 = function() {
    for (; prop0 < 100; ) {
      argMath5;
    }
  }
  obj0.method1 = func4;
  var ui8 = new Uint8Array(256);
  prop0 = Infinity;
  obj0.method1();
  prop0 = -1766989739;
  obj0.method1();
}

try {
  test0();
} catch(e) {
  Dump(TrimStackTracePath(e.stack));
}

