// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    toString: If radix is an integer from 2 to 36, but not 10,
    the result is a string, the choice of which is implementation-dependent
es5id: 15.7.4.2_A2_T31
description: radix is 33
---*/
assert.sameValue(Number.prototype.toString(33), "0", 'Number.prototype.toString(33) must return "0"');
assert.sameValue((new Number()).toString(33), "0", '(new Number()).toString(33) must return "0"');
assert.sameValue((new Number(0)).toString(33), "0", '(new Number(0)).toString(33) must return "0"');
assert.sameValue((new Number(-1)).toString(33), "-1", '(new Number(-1)).toString(33) must return "-1"');
assert.sameValue((new Number(1)).toString(33), "1", '(new Number(1)).toString(33) must return "1"');

assert.sameValue(
  (new Number(Number.NaN)).toString(33),
  "NaN",
  '(new Number(Number.NaN)).toString(33) must return "NaN"'
);

assert.sameValue(
  (new Number(Number.POSITIVE_INFINITY)).toString(33),
  "Infinity",
  '(new Number(Number.POSITIVE_INFINITY)).toString(33) must return "Infinity"'
);

assert.sameValue(
  (new Number(Number.NEGATIVE_INFINITY)).toString(33),
  "-Infinity",
  '(new Number(Number.NEGATIVE_INFINITY)).toString(33) must return "-Infinity"'
);
