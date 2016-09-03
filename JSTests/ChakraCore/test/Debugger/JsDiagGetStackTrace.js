//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/**exception(all):stack();**/

function fromEval() {
  try {
    throw new Error('Caught Error');
  } catch (ex) {}
}

function foo() {
  eval("fromEval();");
}
foo();

function FuncLevel2() {
  var level2Var = 1;
  function FuncLevel3() {
    var level3Var = level2Var; /**bp:stack();**/
  }
  FuncLevel3();
}

var globalVar = Math;

function FuncLevel1() {
  var level1Var = globalVar;
  FuncLevel2();
}
FuncLevel1(1);
WScript.Echo("pass");
