// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-group-constructor
description: >
  The cleanupSome() method throws if cleanup is currently in progress. 
info: |
  FinalizationGroup.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationGroup be the this value.
  ...
  4. If finalizationGroup.[[IsFinalizationGroupCleanupJobActive]] is true, 
     throw a TypeError exception.

features: [FinalizationGroup, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var called = 0;
var endOfCall = 0;
var fg = new FinalizationGroup(function() {});

function callback(iterator) {
  called += 1;

  if (called === 1) {
    // Atempt to re-enter the callback.
    var nestedCallbackRan = false;
    assert.throws(TypeError, () => {
      fg.cleanupSome(() => { nestedCallbackRan = true });
    });
    assert.sameValue(nestedCallbackRan, false);
  }

  endOfCall += 1;
}

function emptyCells() {
  var o1 = {};
  fg.register(o1, 'holdings 1');

  var prom = asyncGC(o1);
  o1 = null;

  return prom;
}

emptyCells().then(function() {
  fg.cleanupSome(callback);

  assert.sameValue(called, 1, 'callback was called');
  assert.sameValue(endOfCall, 1, 'callback finished');
}).then($DONE, resolveAsyncGC);
