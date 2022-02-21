// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The valueOf method returns its "this" value
es5id: 15.2.4.4_A1_T2
description: "\"this\" value is a boolean"
---*/
assert.sameValue(
  typeof Object.prototype.valueOf,
  "function",
  'The value of `typeof Object.prototype.valueOf` is expected to be "function"'
);

var obj = new Object(true);

assert.sameValue(typeof obj.valueOf, "function", 'The value of `typeof obj.valueOf` is expected to be "function"');
assert.sameValue(obj.valueOf(), true, 'obj.valueOf() must return true');
