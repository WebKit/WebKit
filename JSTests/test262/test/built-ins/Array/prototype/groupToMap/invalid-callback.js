// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.groupToMap
description: Array.prototype.groupToMap called with non-callable throws TypeError
info: |
  22.1.3.15 Array.prototype.groupToMap ( callbackfn [ , thisArg ] )

  ...

  3. If IsCallable(callbackfn) is false, throw a TypeError exception.

  ...
features: [array-grouping]
---*/


assert.throws(TypeError, function() {
  [].groupToMap(null)
}, "null callback throws TypeError");

assert.throws(TypeError, function() {
  [].groupToMap(undefined)
}, "undefined callback throws TypeError");

assert.throws(TypeError, function() {
  [].groupToMap({})
}, "object callback throws TypeError");
