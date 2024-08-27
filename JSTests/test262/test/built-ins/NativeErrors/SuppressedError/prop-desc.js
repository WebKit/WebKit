// Copyright (C) 2023 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-suppressederror-objects
description: >
  Property descriptor of SuppressedError
info: |
  SuppressedError Objects

  ECMAScript Standard Built-in Objects:

  Every other data property described in clauses 18 through 26 and in Annex B.2
  has the attributes { [[Writable]]: true, [[Enumerable]]: false,
  [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
features: [explicit-resource-management]
---*/

assert.sameValue(typeof SuppressedError, 'function');

verifyProperty(this, 'SuppressedError', {
  enumerable: false,
  writable: true,
  configurable: true
});
