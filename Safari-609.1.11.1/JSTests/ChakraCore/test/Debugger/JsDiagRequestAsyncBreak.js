//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/**onasyncbreak:evaluate('count');enableBp('A');**/

var count = 0;
while (count++ < 5) {
  if (count == 3) {
    WScript.RequestAsyncBreak();
  }
}

function foo() {
  var x = 1;
  x = 2; /**loc(A):stack();**/
}
foo();

WScript.Echo("pass");
