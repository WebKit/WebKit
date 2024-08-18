// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-symbol.asyncdispose
description: >
    `Symbol.asyncDispose` property descriptor
info: |
    This property has the attributes { [[Writable]]: false, [[Enumerable]]:
    false, [[Configurable]]: false }.
includes: [propertyHelper.js]
features: [explicit-resource-management]
---*/

assert.sameValue(typeof Symbol.asyncDispose, 'symbol');
verifyNotEnumerable(Symbol, 'asyncDispose');
verifyNotWritable(Symbol, 'asyncDispose');
verifyNotConfigurable(Symbol, 'asyncDispose');
