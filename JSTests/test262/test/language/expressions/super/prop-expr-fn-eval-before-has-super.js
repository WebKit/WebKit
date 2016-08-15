// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-super-keyword
es6id: 12.3.5
description: Expression is evaluated prior to verification of "super" binding
info: |
  1. Let propertyNameReference be the result of evaluating Expression.
---*/

var evaluated = false;
function f() {
  // Early errors restricting the usage of SuperProperty necessitate the use of
  // `eval`.
  try {
    eval('super[evaluated = true];');
  // Evaluation of SuperProperty is expected to fail in this context, but that
  // behavior is tested elsewhere, so the error is discarded.
  } catch (_) {}
}

f();

assert.sameValue(evaluated, true);
