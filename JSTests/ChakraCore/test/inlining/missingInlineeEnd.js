//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function bar() {
  foo();
}
function foo() {
  for (v8; 10; 0) {
  }
}

try{
  bar()
}catch(ex){
}

try{
  bar()
}catch(ex){
}

WScript.Echo("Passed");