// Copyright (C) 2017 Igalia, S. L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-bigint-@@tostringtag
description: >
    `Symbol.toStringTag` property descriptor
info: |
    The initial value of the @@toStringTag property is the String value
    "BigInt".

    This property has the attributes { [[Writable]]: false, [[Enumerable]]:
    false, [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [Symbol.toStringTag, BigInt, Symbol]
---*/

verifyProperty(BigInt.prototype, Symbol.toStringTag, {
  value: "BigInt",
  writable: false,
  enumerable: false,
  configurable: true
});

assert.sameValue(Object.prototype.toString.call(3n), "[object BigInt]");
assert.sameValue(Object.prototype.toString.call(Object(3n)), "[object BigInt]");

// Verify that Object.prototype.toString does not have special casing for BigInt
// as it does for most other primitive types
Object.defineProperty(BigInt.prototype, Symbol.toStringTag, {
  value: "FooBar",
  writable: false,
  enumerable: false,
  configurable: true
});

assert.sameValue(Object.prototype.toString.call(3n), "[object FooBar]");
assert.sameValue(Object.prototype.toString.call(Object(3n)), "[object FooBar]");
