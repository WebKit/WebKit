//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
function mainFunc(inName) {
  for (i in this) {
    if (i == inName) {
       WScript.Echo("PASS");
    }
  }
}

mainFunc.one = 20;
child = WScript.LoadScriptFile("crosssite_bind_child.js", "samethread");
childFunc = child.setupFunc(mainFunc);
childFunc();