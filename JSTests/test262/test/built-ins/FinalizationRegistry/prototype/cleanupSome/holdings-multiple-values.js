// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-registry-constructor
description: >
  Iterates over different type values in holdings
info: |
  FinalizationRegistry.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationRegistry be the this value.
  ...
  5. Perform ! CleanupFinalizationRegistry(finalizationRegistry, callback).
  ...

  CleanupFinalizationRegistry ( finalizationRegistry [ , callback ] )

  ...
  3. While finalizationRegistry.[[Cells]] contains a Record cell such that cell.[[WeakRefTarget]] is ~empty~, then an implementation may perform the following steps,
    a. Choose any such cell.
    b. Remove cell from finalizationRegistry.[[Cells]].
    c. Perform ? Call(callback, undefined, << cell.[[HeldValue]] >>).
  ...


features: [FinalizationRegistry, Symbol, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

function check(value, expectedName) {
  var holdings = [];
  var called = 0;
  var finalizationRegistry = new FinalizationRegistry(function() {});

  function callback(holding) {
    called += 1;
    holdings.push(holding);
  }

  // This is internal to avoid conflicts
  function emptyCells(value) {
    var target = {};
    finalizationRegistry.register(target, value);

    var prom = asyncGC(target);
    target = null;

    return prom;
  }

  return emptyCells(value).then(function() {
    finalizationRegistry.cleanupSome(callback);
    assert.sameValue(called, 1, expectedName);
    assert.sameValue(holdings.length, 1, expectedName);
    assert.sameValue(holdings[0], value, expectedName);
  });
}

Promise.all([
  check(undefined, 'undefined'),
  check(null, 'null'),
  check('', 'the empty string'),
  check({}, 'object'),
  check(42, 'number'),
  check(true, 'true'),
  check(false, 'false'),
  check(Symbol(1), 'symbol'),
]).then(() => $DONE(), resolveAsyncGC);
