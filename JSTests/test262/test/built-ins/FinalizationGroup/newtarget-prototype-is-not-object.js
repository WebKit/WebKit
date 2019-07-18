// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group-target
description: >
  [[Prototype]] defaults to %FinalizationGroupPrototype% if NewTarget.prototype is not an object.
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
  4. If Type(proto) is not Object, then
    a. Let realm be ? GetFunctionRealm(constructor).
    b. Set proto to realm's intrinsic object named intrinsicDefaultProto.
  5. Return proto.
features: [FinalizationGroup, Reflect.construct, Symbol]
---*/

var fg;
function newTarget() {}
function fn() {}

newTarget.prototype = undefined;
fg = Reflect.construct(FinalizationGroup, [fn], newTarget);
assert.sameValue(Object.getPrototypeOf(fg), FinalizationGroup.prototype, 'newTarget.prototype is undefined');

newTarget.prototype = null;
fg = Reflect.construct(FinalizationGroup, [fn], newTarget);
assert.sameValue(Object.getPrototypeOf(fg), FinalizationGroup.prototype, 'newTarget.prototype is null');

newTarget.prototype = true;
fg = Reflect.construct(FinalizationGroup, [fn], newTarget);
assert.sameValue(Object.getPrototypeOf(fg), FinalizationGroup.prototype, 'newTarget.prototype is a Boolean');

newTarget.prototype = '';
fg = Reflect.construct(FinalizationGroup, [fn], newTarget);
assert.sameValue(Object.getPrototypeOf(fg), FinalizationGroup.prototype, 'newTarget.prototype is a String');

newTarget.prototype = Symbol();
fg = Reflect.construct(FinalizationGroup, [fn], newTarget);
assert.sameValue(Object.getPrototypeOf(fg), FinalizationGroup.prototype, 'newTarget.prototype is a Symbol');

newTarget.prototype = 1;
fg = Reflect.construct(FinalizationGroup, [fn], newTarget);
assert.sameValue(Object.getPrototypeOf(fg), FinalizationGroup.prototype, 'newTarget.prototype is a Number');
