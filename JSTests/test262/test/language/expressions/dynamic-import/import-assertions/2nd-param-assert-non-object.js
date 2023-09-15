// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  Rejects promise when the `assert` property of the second argument is neither
  undefined nor an object
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
              1. Perform ! Call(promiseCapability.[[Reject]], undefined, « a
                 newly created TypeError object »).
              2. Return promiseCapability.[[Promise]].
    [...]
features: [dynamic-import, import-assertions, Symbol, BigInt]
flags: [async]
---*/

function test(promise, valueType) {
  return promise.then(function() {
      throw new Test262Error('Promise for ' + valueType + ' was not rejected.');
    }, function(error) {
      assert.sameValue(error.constructor, TypeError, valueType);
    });
}

Promise.all([
    test(import('./2nd-param_FIXTURE.js', {assert:null}), 'null'),
    test(import('./2nd-param_FIXTURE.js', {assert:false}), 'boolean'),
    test(import('./2nd-param_FIXTURE.js', {assert:23}), 'number'),
    test(import('./2nd-param_FIXTURE.js', {assert:''}), 'string'),
    test(import('./2nd-param_FIXTURE.js', {assert:Symbol('')}), 'symbol'),
    test(import('./2nd-param_FIXTURE.js', {assert:23n}), 'bigint')
  ])
  .then(function() {})
  .then($DONE, $DONE);
