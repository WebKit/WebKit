// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.item
description: >
  TypedArray.prototype.item.length value and descriptor.
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

assert.sameValue(
  TypedArray.prototype.item.length, 1,
  'The value of TypedArray.prototype.item.length is 1'
);

verifyNotEnumerable(TypedArray.prototype.item, 'length');
verifyNotWritable(TypedArray.prototype.item, 'length');
verifyConfigurable(TypedArray.prototype.item, 'length');
