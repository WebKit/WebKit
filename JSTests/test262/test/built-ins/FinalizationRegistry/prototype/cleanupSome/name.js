// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry.prototype.cleanupSome
description: FinalizationRegistry.prototype.cleanupSome.name property descriptor
info: |
  FinalizationRegistry.prototype.cleanupSome.name value and property descriptor

  17 ECMAScript Standard Built-in Objects

  Every built-in function object, including constructors, that is not
  identified as an anonymous function has a name property whose value
  is a String. Unless otherwise specified, this value is the name that
  is given to the function in this specification. For functions that
  are specified as properties of objects, the name value is the
  property name string used to access the function. [...]

  Unless otherwise specified, the name property of a built-in function
  object, if it exists, has the attributes { [[Writable]]: false,
  [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [FinalizationRegistry.prototype.cleanupSome, FinalizationRegistry]
---*/

verifyProperty(FinalizationRegistry.prototype.cleanupSome, 'name', {
  value: 'cleanupSome',
  writable: false,
  enumerable: false,
  configurable: true
});
