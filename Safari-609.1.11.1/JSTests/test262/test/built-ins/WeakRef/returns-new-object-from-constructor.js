// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-weak-ref-target
description: >
  Returns a new ordinary object from the WeakRef constructor
info: |
  WeakRef ( target )

  ...
  3. Let weakRef be ? OrdinaryCreateFromConstructor(NewTarget,  "%WeakRefPrototype%", « [[Target]] »).
  4. Perfom ! KeepDuringJob(target).
  5. Set weakRef.[[Target]] to target.
  6. Return weakRef.

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
features: [WeakRef]
---*/

var target = {};
var wr = new WeakRef(target);

assert.notSameValue(wr, target, 'does not return the same object');
assert.sameValue(wr instanceof WeakRef, true, 'instanceof');

for (let key of Object.getOwnPropertyNames(wr)) {
  assert(false, `should not set any own named properties: ${key}`);
}

for (let key of Object.getOwnPropertySymbols(wr)) {
  assert(false, `should not set any own symbol properties: ${String(key)}`);
}

assert.sameValue(Object.getPrototypeOf(wr), WeakRef.prototype);


