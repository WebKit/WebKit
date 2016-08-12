// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-@@iterator
es6id: 26.3.2
description: Descriptor for `name` property
info: >
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
flags: [module]
features: [Symbol.iterator]
includes: [propertyHelper.js]
---*/

import * as ns from './name.js';

assert.sameValue(ns[Symbol.iterator].name, '[Symbol.iterator]');

verifyNotEnumerable(ns[Symbol.iterator], 'name');
verifyNotWritable(ns[Symbol.iterator], 'name');
verifyConfigurable(ns[Symbol.iterator], 'name');
