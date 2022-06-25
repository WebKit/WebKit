//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

globalVar = {
  a : "Hello World",
  b : /regex/ig
};
function foo() {
  var localVar = {
    a : {
      b : 1
    }
  };
  var x = 1; /**bp:evaluate('localVar', 2);evaluate('globalVar', 1);**/
}
foo();

var x = NaN;
function Level1Func() {
  var x = null;
  Level2Func();
}

function Level2Func() {
  var x = -Infinity;
  Level3Func();
}

function Level3Func() {
  var x = Math.PI;
  var y = 4; /**bp:evaluate('x');setFrame(1);evaluate('x');setFrame(2);evaluate('x');setFrame(3);evaluate('x');**/
}
Level1Func();
WScript.Echo("pass");
