// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-weakset.prototype.has
description: >
  WeakSet.prototype.has.name value and writability.
info: |
  WeakSet.prototype.has ( value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  WeakSet.prototype.has.name, 'has',
  'The value of WeakSet.prototype.has.name is "has"'
);

verifyNotEnumerable(WeakSet.prototype.has, 'name');
verifyNotWritable(WeakSet.prototype.has, 'name');
verifyConfigurable(WeakSet.prototype.has, 'name');
