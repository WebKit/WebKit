//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
  var loopInvariant = 11;
  var obj0 = {};
  var arrObj0 = {};
  var func1 = function (argMath0) {
    for (var _strvar4 in i32) {
      loopInvariant;
      i32[_strvar4] = argMath0;
    }
  };
  var i32 = new Int32Array(256);
  arrObj0 = new Proxy(arrObj0, Object());
  func1(arrObj0);
}
try {
  test0();
  print("PASSED");
} catch (e) {
  print("FAILED");
}
