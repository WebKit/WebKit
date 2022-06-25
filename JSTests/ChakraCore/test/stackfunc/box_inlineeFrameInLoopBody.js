//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var runningJITtedCode = false;
function test0() {
  var func0 = function () {
    function v1() {
      function v2() {
        return v2;
      }
      if (runningJITtedCode) {
        return v2();
      }
    }
    var v3 = v1();
  };
  for (var __loopvar0 = 4 - 6;;) {
    if (__loopvar0 === 4) {
      break;
    }
    __loopvar0 += 2;
    func0();
  }
}
test0();
runningJITtedCode = true;
test0();
WScript.Echo("passed");