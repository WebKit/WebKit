// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group-target
description: Instances of FinalizationGroup are extensible
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

  ObjectCreate ( proto [ , internalSlotsList ] )

  4. Set obj.[[Prototype]] to proto.
  5. Set obj.[[Extensible]] to true.
  6. Return obj.
features: [FinalizationGroup]
---*/

var fg = new FinalizationGroup(function() {});
assert.sameValue(Object.isExtensible(fg), true);
