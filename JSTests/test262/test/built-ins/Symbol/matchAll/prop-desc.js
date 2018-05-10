// Copyright (C) 2018 Peter Wong. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: pending
description: |
    `Symbol.matchAll` property descriptor
info: |
    This property has the attributes { [[Writable]]: false, [[Enumerable]]:
    false, [[Configurable]]: false }.
includes: [propertyHelper.js]
features: [Symbol.match]
---*/

assert.sameValue(typeof Symbol.matchAll, 'symbol');
verifyNotEnumerable(Symbol, 'matchAll');
verifyNotWritable(Symbol, 'matchAll');
verifyNotConfigurable(Symbol, 'matchAll');
