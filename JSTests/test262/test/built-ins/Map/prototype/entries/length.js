// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-map.prototype.entries
description: >
  Map.prototype.entries.length value and descriptor.
info: |
  Map.prototype.entries ( )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.entries.length, 0,
  'The value of `Map.prototype.entries.length` is `0`'
);

verifyNotEnumerable(Map.prototype.entries, 'length');
verifyNotWritable(Map.prototype.entries, 'length');
verifyConfigurable(Map.prototype.entries, 'length');
