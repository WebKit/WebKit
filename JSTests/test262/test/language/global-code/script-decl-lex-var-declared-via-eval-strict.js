// Copyright (C) 2023 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-globaldeclarationinstantiation
description: No let binding collision with existing var declaration due to strict-mode eval().
info: |
  PerformEval ( x, strictCaller, direct )

  [...]
  16. If direct is true, then
    a. Let lexEnv be NewDeclarativeEnvironment(runningContext's LexicalEnvironment).
  [...]
  18. If strictEval is true, set varEnv to lexEnv.
flags: [onlyStrict]
---*/

eval('var test262Var;');
eval('function test262Fn() {}');

$262.evalScript('let test262Var = 1;');
assert.sameValue(test262Var, 1);

$262.evalScript('const test262Fn = 2;');
assert.sameValue(test262Fn, 2);
