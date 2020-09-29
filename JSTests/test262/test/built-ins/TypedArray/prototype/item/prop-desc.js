// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.item
description: >
  Property type and descriptor.
info: |
  %TypedArray%.prototype.item( index )

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js, testTypedArray.js]
features: [TypedArray.prototype.item]
---*/
assert.sameValue(
  typeof TypedArray.prototype.item,
  'function',
  'The value of `typeof TypedArray.prototype.item` is "function"'
);

verifyNotEnumerable(TypedArray.prototype, 'item');
verifyWritable(TypedArray.prototype, 'item');
verifyConfigurable(TypedArray.prototype, 'item');
