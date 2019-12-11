// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group-target
description: >
  Return abrupt from getting the NewTarget prototype
info: |
  FinalizationGroup ( cleanupCallback )

  ...
  3. Let finalizationGroup be ? OrdinaryCreateFromConstructor(NewTarget,  "%FinalizationGroupPrototype%", « [[Realm]], [[CleanupCallback]], [[Cells]], [[IsFinalizationGroupCleanupJobActive]] »).
  ...
  9. Return finalizationGroup.

  OrdinaryCreateFromConstructor ( constructor, intrinsicDefaultProto [ , internalSlotsList ] )

  ...
  2. Let proto be ? GetPrototypeFromConstructor(constructor, intrinsicDefaultProto).
  3. Return ObjectCreate(proto, internalSlotsList).

  GetPrototypeFromConstructor ( constructor, intrinsicDefaultProto )

  3. Let proto be ? Get(constructor, 'prototype').
features: [FinalizationGroup, Reflect.construct]
---*/

var calls = 0;
var newTarget = function() {}.bind(null);
Object.defineProperty(newTarget, 'prototype', {
  get: function() {
    calls += 1;
    throw new Test262Error();
  }
});

assert.throws(Test262Error, function() {
  Reflect.construct(FinalizationGroup, [function() {}], newTarget);
});

assert.sameValue(calls, 1);
