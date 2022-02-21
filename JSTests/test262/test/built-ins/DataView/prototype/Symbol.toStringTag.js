// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-dataview.prototype-@@tostringtag
description: >
    `Symbol.toStringTag` property descriptor
info: |
    The initial value of the @@toStringTag property is the String value
    "DataView".

    This property has the attributes { [[Writable]]: false, [[Enumerable]]:
    false, [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [Symbol.toStringTag]
---*/

assert.sameValue(
  DataView.prototype[Symbol.toStringTag],
  'DataView',
  'The value of DataView.prototype[Symbol.toStringTag] is expected to be "DataView"'
);

verifyNotEnumerable(DataView.prototype, Symbol.toStringTag);
verifyNotWritable(DataView.prototype, Symbol.toStringTag);
verifyConfigurable(DataView.prototype, Symbol.toStringTag);
