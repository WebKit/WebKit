// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.map
description: Array.prototype.map - applied to String object
---*/

function callbackfn(val, idx, obj) {
  return obj instanceof String;
}

var obj = new String("abc");
var testResult = Array.prototype.map.call(obj, callbackfn);

assert.sameValue(testResult[0], true, 'testResult[0]');
assert.sameValue(testResult[1], true, 'testResult[1]');
assert.sameValue(testResult[2], true, 'testResult[2]');
