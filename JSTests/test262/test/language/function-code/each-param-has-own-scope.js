// Copyright (C) 2017 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-function-definitions-runtime-semantics-iteratorbindinginitialization
description: A new declarative environment is created, from the originalEnv, for each parameter.
info: |
  Runtime Semantics: IteratorBindingInitialization

  FormalParameter : BindingElement

  ...
  6. Let paramVarEnv be NewDeclarativeEnvironment(originalEnv).
  ...

features: [arrow-function, default-parameters]
---*/

var y;
function f(a = eval("var x = 1; y = 42; x"), b = x) {}

assert.throws(ReferenceError, () => {
  f();
});

assert.sameValue(y, 42);
