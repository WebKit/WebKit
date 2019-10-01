// Copyright (C) 2019  Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    Collection of functions used to capture references cleanup from garbage collectors
features: [Symbol, async-functions]
flags: [non-deterministic]
features: [FinalizationGroup]
defines: [asyncGC, asyncGCDeref, resolveAsyncGC]
---*/

function asyncGC(...targets) {
  var fg = new FinalizationGroup(() => {});
  var length = targets.length;

  for (let target of targets) {
    fg.register(target, 'target');
    target = null;
  }

  targets = null;

  return Promise.resolve('tick').then(() => asyncGCDeref()).then(() => {
    var names;

    // consume iterator to capture names
    fg.cleanupSome(iter => { names = [...iter]; });

    if (!names || names.length != length) {
      throw asyncGC.notCollected;
    }
  });
}

asyncGC.notCollected = Symbol('Object was not collected');

async function asyncGCDeref() {
  var trigger;

  // TODO: Remove this when $262.clearKeptObject becomes documented and required
  if ($262.clearKeptObjects) {
    trigger = $262.clearKeptObjects();
  }

  await $262.gc();

  return Promise.resolve(trigger);
}

function resolveAsyncGC(err) {
  if (err === asyncGC.notCollected) {
    // Do not fail as GC can't provide necessary resources.
    $DONE();
  }

  $DONE(err);
}
