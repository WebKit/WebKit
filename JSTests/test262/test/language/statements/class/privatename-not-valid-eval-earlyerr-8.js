// Copyright (C) 2017 Valerie Young. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-scripts-static-semantics-early-errors
description: Early error when referencing privatename on object, outside of class.
info: |
  Static Semantics: Early Errors
    1. Let names be an empty List.
      ...
    3. If AllPrivateNamesValid of ScriptBody with the argument names is false, throw a SyntaxError exception.
features: [class-fields-private]
---*/

var executed = false;

function f() {
  eval("executed = true; this.#x;");
}

assert.throws(SyntaxError, function() {
  f();
});

assert.sameValue(executed, false);

