// Copyright (C) 2018 Ujjwal Sharma. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.numberformat.prototype-@@tostringtag
description: >
  Check that the initial value of the property is "Object" and that any changes
  made by reconfiguring are reflected.
---*/

assert.sameValue(Intl.NumberFormat.prototype[Symbol.toStringTag], 'Object');
assert.sameValue(
  Object.prototype.toString.call(new Intl.NumberFormat()),
  '[object Object]'
);

Object.defineProperty(Intl.NumberFormat.prototype, Symbol.toStringTag, {
  value: 'Alpha'
});

assert.sameValue(Intl.NumberFormat.prototype[Symbol.toStringTag], 'Alpha');
assert.sameValue(
  Object.prototype.toString.call(new Intl.NumberFormat()),
  '[object Alpha]'
);
