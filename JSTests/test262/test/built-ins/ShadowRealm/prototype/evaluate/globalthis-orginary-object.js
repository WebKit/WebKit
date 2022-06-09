// Copyright (C) 2021 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-shadowrealm.prototype.evaluate
description: >
  The globalThis must be an ordinary object from OrdinaryObjectCreate
info: |
  ShadowRealm ( )

  ...
  3. Let realmRec be CreateRealm().
  4. Set O.[[ShadowRealm]] to realmRec.
  ...
  10. Perform ? SetRealmGlobalObject(realmRec, undefined, undefined).
  11. Perform ? SetDefaultGlobalBindings(O.[[ShadowRealm]]).
  12. Perform ? HostInitializeShadowRealm(O.[[ShadowRealm]]).

  SetRealmGlobalObject ( realmRec, globalObj, thisValue )

  1. If globalObj is undefined, then
    a. Let intrinsics be realmRec.[[Intrinsics]].
    b. Set globalObj to ! OrdinaryObjectCreate(intrinsics.[[%Object.prototype%]]).
  2. Assert: Type(globalObj) is Object.
  3. If thisValue is undefined, set thisValue to globalObj.
  ...

  OrdinaryObjectCreate ( proto [ , additionalInternalSlotsList ] )

  1. Let internalSlotsList be « [[Prototype]], [[Extensible]] ».
  2. If additionalInternalSlotsList is present, append each of its elements to internalSlotsList.
  3. Let O be ! MakeBasicObject(internalSlotsList).
  4. Set O.[[Prototype]] to proto.
  5. Return O.

  MakeBasicObject ( internalSlotsList )

  ...
  5. If internalSlotsList contains [[Extensible]], set obj.[[Extensible]] to true.
features: [ShadowRealm]
---*/

assert.sameValue(
  typeof ShadowRealm.prototype.evaluate,
  'function',
  'This test must fail if ShadowRealm.prototype.evaluate is not a function'
);

const r = new ShadowRealm();

assert.sameValue(
  r.evaluate('Object.getPrototypeOf(globalThis) === Object.prototype'),
  true,
  'The [[Prototype]] of globalThis is Object.prototype'
);

assert.sameValue(
  r.evaluate('Object.isExtensible(globalThis)'),
  true,
  'globalThis is extensible'
);

assert.sameValue(
  r.evaluate('globalThis.constructor === Object'),
  true,
  'globalThis.constructor is Object'
);
