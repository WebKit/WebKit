// Copyright (C) 2023 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-disposablestack-prototype-object
description: DisposableStack.prototype.constructor
info: |
  DisposableStack.prototype.constructor

  Normative Optional

  The initial value of DisposableStack.prototype.constructor is the intrinsic object %DisposableStack%.

  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.

  This section is to be treated identically to the "Annex B" of ECMA-262, but to be written in-line with the main specification.
includes: [propertyHelper.js]
features: [explicit-resource-management]
---*/

var actual = Object.prototype.hasOwnProperty.call(DisposableStack, 'constructor');

// If implemented, it should conform to the spec text
if (actual) {
  verifyProperty(DisposableStack.prototype, 'constructor', {
    value: DisposableStack,
    writable: true,
    enumerable: false,
    configurable: true
  });
}
