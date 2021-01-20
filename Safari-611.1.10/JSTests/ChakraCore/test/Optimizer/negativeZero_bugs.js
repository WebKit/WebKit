//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(addOrSub) {
  function makeArrayLength() {
  }
  var obj0 = {};
  var obj1 = {};
  var protoObj1 = {};
  var arrObj0 = {};

  var func4 = function () {
    return arrObj0 * (f > obj1.prop1 ? h++ : Function());
  };
  
  var f = 1;
  var h = 0;
  obj1.prop1 = -1;
  switch(addOrSub)
  {
    case 1:
      f /= ((1 - 1) * -1) - -(func4.call(arrObj0) << (typeof arrObj0.length == null));
      break;

    case 2:
      f /= ((1 - 1) * -1) + -(func4.call(arrObj0) << (typeof arrObj0.length == null));
      break;
  }
  
  func4();
  
  print(h);
}
test0(1);
test0(2);

test0(1);
test0(2);