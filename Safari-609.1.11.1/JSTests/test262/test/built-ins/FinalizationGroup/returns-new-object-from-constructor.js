// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group-target
description: >
  Returns a new ordinary object from the FinalizationGroup constructor
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
features: [FinalizationGroup, for-of]
---*/

var cleanupCallback = function() {};
var fg = new FinalizationGroup(cleanupCallback);

assert.sameValue(Object.getPrototypeOf(fg), FinalizationGroup.prototype);
assert.notSameValue(fg, cleanupCallback, 'does not return the same function');
assert.sameValue(fg instanceof FinalizationGroup, true, 'instanceof');

for (let key of Object.getOwnPropertyNames(fg)) {
  assert(false, `should not set any own named properties: ${key}`);
}

for (let key of Object.getOwnPropertySymbols(fg)) {
  assert(false, `should not set any own symbol properties: ${String(key)}`);
}

assert.sameValue(Object.getPrototypeOf(fg), FinalizationGroup.prototype);
