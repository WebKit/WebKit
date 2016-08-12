// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-module-namespace-exotic-objects-delete-p
description: >
    [[Delete]] behavior for a key that does not describe an exported binding
info: |
    [...]
    2. Let exports be the value of O's [[Exports]] internal slot.
    3. If P is an element of exports, return false.
    4. Return true.
flags: [module]
features: [Reflect, Symbol, Symbol.iterator, Symbol.toStringTag]
---*/

import * as ns from './delete-non-exported.js';
var sym = Symbol('test262');

assert(delete ns.undef, 'delete: undef');
assert(Reflect.deleteProperty(ns, 'undef'), 'Reflect.deleteProperty: undef');

assert(delete ns.default, 'delete: default');
assert(
  Reflect.deleteProperty(ns, 'default'), 'Reflect.deleteProperty: default'
);

assert(delete ns[Symbol.iterator], 'delete: Symbol.iterator');
assert(
  Reflect.deleteProperty(ns, Symbol.iterator),
  'Reflect.deleteProperty: Symbol.iterator'
);

assert(delete ns[Symbol.toStringTag], 'delete: Symbol.toStringTag');
assert(
  Reflect.deleteProperty(ns, Symbol.toStringTag),
  'Reflect.deleteProperty: Symbol.toStringTag'
);

assert(delete ns[sym], 'delete: symbol');
assert(Reflect.deleteProperty(ns, sym), 'Reflect.deleteProperty: symbol');
