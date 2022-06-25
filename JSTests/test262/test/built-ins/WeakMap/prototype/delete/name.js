// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-weakmap.prototype.delete
description: >
  WeakMap.prototype.delete.name value and writability.
info: |
  WeakMap.prototype.delete ( value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  WeakMap.prototype.delete.name, 'delete',
  'The value of WeakMap.prototype.delete.name is "delete"'
);

verifyNotEnumerable(WeakMap.prototype.delete, 'name');
verifyNotWritable(WeakMap.prototype.delete, 'name');
verifyConfigurable(WeakMap.prototype.delete, 'name');
