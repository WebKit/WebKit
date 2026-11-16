//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var __counter = 0;
function test0() {
  __counter++;
  var obj0 = {};
  var protoObj0 = {};
  var obj1 = {};
  var func0 = function () {
  };
  var func4 = function () {
    return func4.caller;
  };
  obj0.method1 = func0;
  obj1.method0 = func4;
  Object.prototype.method0 = obj0.method1;
  var ary = Array();
  ary[0] = 41697303.1;
  var protoObj1 = Object(obj1);
  for (var _strvar35 in ary) {
    function v18() {
      for (var v21 = 0; v21 < 3; v21++) {
        (function () {
          var uniqobj8 = [
            protoObj1,
            protoObj0
          ];
          uniqobj8[__counter % uniqobj8.length].method0();
        }());
      }
    }
    v18();
  }
}
test0();
test0();
WScript.Echo("Passed");
