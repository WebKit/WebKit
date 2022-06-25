// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.1.3.27
description: Descriptor for `name` property
info: |
  The value of the name property of this function is "[Symbol.iterator]".

  ES6 Section 17: ECMAScript Standard Built-in Objects

  Every built-in Function object, including constructors, that is not
  identified as an anonymous function has a name property whose value is a
  String. Unless otherwise specified, this value is the name that is given to
  the function in this specification.

  [...]

  Unless otherwise specified, the name property of a built-in Function
  object, if it exists, has the attributes { [[Writable]]: false,
  [[Enumerable]]: false, [[Configurable]]: true }.
features: [Symbol.iterator]
includes: [propertyHelper.js]
---*/

assert.sameValue(String.prototype[Symbol.iterator].name, '[Symbol.iterator]');

verifyNotEnumerable(String.prototype[Symbol.iterator], 'name');
verifyNotWritable(String.prototype[Symbol.iterator], 'name');
verifyConfigurable(String.prototype[Symbol.iterator], 'name');
