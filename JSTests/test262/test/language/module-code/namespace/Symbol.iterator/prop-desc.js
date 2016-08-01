// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-@@iterator
es6id: 26.3.2
description: Property descriptor
info: >
    ES6 Section 17

    Every other data property described in clauses 18 through 26 and in Annex
    B.2 has the attributes { [[Writable]]: true, [[Enumerable]]: false,
    [[Configurable]]: true } unless otherwise specified.
flags: [module]
features: [Symbol.iterator]
---*/

import * as ns from './prop-desc.js';

assert.sameValue(typeof ns[Symbol.iterator], 'function');

// propertyHelper.js is not appropriate for this test because it assumes that
// the object exposes the ordinary object's implementation of [[Get]], [[Set]],
// [[Delete]], and [[OwnPropertyKeys]], which the module namespace exotic
// object does not.
var desc = Object.getOwnPropertyDescriptor(ns, Symbol.iterator);

assert.sameValue(desc.enumerable, false, 'reports as non-enumerable');
assert.sameValue(desc.writable, true, 'reports as writable');
assert.sameValue(desc.configurable, true, 'reports as configurable');
