// Copyright (C) 2015 Mike Pennisi. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype-@@unscopables
description: >
    Property descriptor for initial value of `Symbol.unscopables` property
info: |
    This property has the attributes { [[Writable]]: false, [[Enumerable]]:
    false, [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [Symbol.unscopables]
---*/

verifyNotEnumerable(Array.prototype, Symbol.unscopables);
verifyNotWritable(Array.prototype, Symbol.unscopables);
verifyConfigurable(Array.prototype, Symbol.unscopables);
