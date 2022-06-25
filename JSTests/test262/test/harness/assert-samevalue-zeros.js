// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Positive and negative zero do not satisfy the assertion.
---*/

var threw = false;

try {
  assert.sameValue(0, -0);
} catch(err) {
  threw = true;
  if (err.constructor !== Test262Error) {
    throw new Error(
      'Expected a Test262Error, but a "' + err.constructor.name +
      '" was thrown.'
    );
  }
  assert.notSameValue(err.message.indexOf('-0'), -1);
}

if (threw === false) {
  throw new Error('Expected a Test262Error, but no error was thrown.');
}
