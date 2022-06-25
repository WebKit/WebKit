// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 19.2.3.6
description: Function.prototype[Symbol.hasInstance] property descriptor
info: |
    This property has the attributes { [[Writable]]: false, [[Enumerable]]:
    false, [[Configurable]]: false }.
features: [Symbol.hasInstance]
includes: [propertyHelper.js]
---*/

assert.sameValue(typeof Function.prototype[Symbol.hasInstance], 'function');

verifyNotEnumerable(Function.prototype, Symbol.hasInstance);
verifyNotWritable(Function.prototype, Symbol.hasInstance);
verifyNotConfigurable(Function.prototype, Symbol.hasInstance);
