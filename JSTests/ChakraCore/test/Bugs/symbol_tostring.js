//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var k;
function test0() {
  try {
    // Below will throw an error and we will try to do implicit toString call on Symbol which will also throw.
    // Under disableImplicit call we were returning nullptr which was wrong.
    [] = ab[Object(Symbol())] = null;
    } catch (e) {
  }
  var ab = '';
}
test0();
test0();
test0();

print("Pass");
