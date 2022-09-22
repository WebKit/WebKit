// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.group
description: Array.prototype.group returns a null prototype object
info: |
  22.1.3.14 Array.prototype.group ( callbackfn [ , thisArg ] )

  ...

  7. Let obj be OrdinaryObjectCreate(null).
  ...
  9. Return obj.

  ...
features: [array-grouping]
---*/

const array = [1, 2, 3];

const obj = array.group(function (i) {
  return i % 2 === 0 ? 'even' : 'odd';
});

assert.sameValue(Object.getPrototypeOf(obj), null);
assert.sameValue(obj.hasOwnProperty, undefined);
