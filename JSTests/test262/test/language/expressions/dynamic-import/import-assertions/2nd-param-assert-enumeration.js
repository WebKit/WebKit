// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  Follows the semantics of the EnumerableOwnPropertyNames abstract operation
  during assertion enumeration
esid: sec-import-call-runtime-semantics-evaluation
info: |
  2.1.1.1 EvaluateImportCall ( specifierExpression [ , optionsExpression ] )
    [...]
    6. Let promiseCapability be ! NewPromiseCapability(%Promise%).
    7. Let specifierString be ToString(specifier).
    8. IfAbruptRejectPromise(specifierString, promiseCapability).
    9. Let assertions be a new empty List.
    10. If options is not undefined, then
        a. If Type(options) is not Object,
           [...]
        b. Let assertionsObj be Get(options, "assert").
        c. IfAbruptRejectPromise(assertionsObj, promiseCapability).
        d. If assertionsObj is not undefined,
           i. If Type(assertionsObj) is not Object,
              [...]
           ii. Let keys be EnumerableOwnPropertyNames(assertionsObj, key).
    [...]
features: [dynamic-import, import-assertions, Symbol, Proxy]
flags: [async]
---*/

var symbol = Symbol('');
var target = {
  enumerable1: '',
  enumerable2: '',
  [symbol]: '',
  unreported: '',
  nonEnumerable: ''
};
var descriptors = {
  enumerable1: {configurable: true, enumerable: true},
  enumerable2: {configurable: true, enumerable: true},
  [symbol]: {configurable: true, enumerable: true},
  nonEnumerable: {configurable: true, enumerable: false}
};
var log = [];

var options = {
  assert: new Proxy({}, {
    ownKeys: function() {
      return ['enumerable1', symbol, 'nonEnumerable', 'absent', 'enumerable2'];
    },
    get(_, name) {
      log.push(name);
      return target[name];
    },
    getOwnPropertyDescriptor(target, name) {
      return descriptors[name];
    }
  })
};

import('./2nd-param_FIXTURE.js', options)
  .then(function(module) {
    assert.sameValue(module.default, 262);
  })
  .then($DONE, $DONE);

assert.sameValue(log.length, 2);
assert.sameValue(log[0], 'enumerable1');
assert.sameValue(log[1], 'enumerable2');
