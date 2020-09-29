// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.item
description: >
  %TypedArray%.prototype.item.name value and descriptor.
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
  TypedArray.prototype.item.name, 'item',
  'The value of TypedArray.prototype.item.name is "item"'
);

verifyNotEnumerable(TypedArray.prototype.item, 'name');
verifyNotWritable(TypedArray.prototype.item, 'name');
verifyConfigurable(TypedArray.prototype.item, 'name');
