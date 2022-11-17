// Copyright (C) 2019 Mathieu Hofman. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry.prototype.unregister
description: >
  Cannot unregister a cell referring to an Object that has been cleaned up
info: |
  FinalizationRegistry.prototype.cleanupSome ( [ _callback_ ] )
  4. Perform ? CleanupFinalizationRegistry(_finalizationRegistry_, _callback_).

  CleanupFinalizationRegistry ( _finalizationRegistry_ ):
  3. While _finalizationRegistry_.[[Cells]] contains a Record _cell_ such that
    _cell_.[[WeakRefTarget]] is ~empty~, then an implementation may perform the
    following steps:
    a. Choose any such _cell_.
    b. Remove _cell_ from _finalizationRegistry_.[[Cells]].
    c. Perform ? HostCallJobCallback(_callback_, *undefined*,
      « _cell_.[[HeldValue]] »).

  FinalizationRegistry.prototype.unregister ( _unregisterToken_ )
  4. Let _removed_ be *false*.
  5. For each Record { [[WeakRefTarget]], [[HeldValue]], [[UnregisterToken]] }
    _cell_ of _finalizationRegistry_.[[Cells]], do
    a. If _cell_.[[UnregisterToken]] is not ~empty~ and
      SameValue(_cell_.[[UnregisterToken]], _unregisterToken_) is *true*, then
      i. Remove _cell_ from _finalizationRegistry_.[[Cells]].
      ii. Set _removed_ to *true*.
  6. Return _removed_.
features: [FinalizationRegistry.prototype.cleanupSome, FinalizationRegistry, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var value = 'target!';
var token = {};
var finalizationRegistry = new FinalizationRegistry(function() {});

function emptyCells() {
  var target = {};
  finalizationRegistry.register(target, value, token);

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  var called = 0;
  var holdings = [];
  finalizationRegistry.cleanupSome(function cb(holding) {
    called += 1;
    holdings.push(holding);
  });

  assert.sameValue(called, 1);
  assert.sameValue(holdings.length, 1);
  assert.sameValue(holdings[0], value);

  var res = finalizationRegistry.unregister(token);
  assert.sameValue(res, false, 'unregister after iterating over it in cleanup');
}).then($DONE, resolveAsyncGC);
