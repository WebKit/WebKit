// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-weak-ref.prototype.deref
description: WeakRef deref should not prevent a GC event
info: |
  WeakRef.prototype.deref ( )

  ...
  4. Let target be the value of weakRef.[[Target]].
  5. If target is not empty,
    a. Perform ! KeepDuringJob(target).
    b. Return target.
  6. Return undefined.
features: [cleanupSome, WeakRef, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var deref = false;
var wr;

function emptyCells() {
  var target = {};
  wr = new WeakRef(target);

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  deref = wr.deref();
  assert.sameValue(deref, undefined);
}).then($DONE, resolveAsyncGC);
