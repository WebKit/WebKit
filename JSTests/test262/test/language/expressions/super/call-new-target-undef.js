// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-super-keyword
es6id: 12.3.5
description: SuperCall requires that NewTarget is defined
info: |
  1. Let newTarget be GetNewTarget().
  2. If newTarget is undefined, throw a ReferenceError exception.
---*/

var evaluatedArg = false;
function f() {
  // Early errors restricting the usage of SuperCall necessitate the use of
  // `eval`.
  eval('super(evaluatedArg = true);');
}

assert.throws(ReferenceError, function() {
  f();
});

assert.sameValue(
  evaluatedArg, false, 'did not perform ArgumentsListEvaluation'
);
