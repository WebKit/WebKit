//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//-mic:1 -off:simplejit -bgjit- -lic:1
function func1() {
  for (var i = 3; i < ary.length; i++) {
    ary[i] = ary[i];
  }
}
var ary = Array(10).fill(0);
func1();
ary.length = 100;
ary.push(ary.shift());
func1();
func1();
print(ary);
