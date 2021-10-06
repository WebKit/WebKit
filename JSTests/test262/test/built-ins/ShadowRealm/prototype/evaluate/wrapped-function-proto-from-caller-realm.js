// Copyright (C) 2021 Chengzhong Wu. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-shadowrealm.prototype.evaluate
description: >
  WrappedFunctionCreate should create a function derived from the caller realm
info: |
  ShadowRealm.prototype.evaluate ( sourceText )
  ...
  4. Let callerRealm be the current Realm Record.
  5. Let evalRealm be O.[[ShadowRealm]].
  6. Return ? PerformRealmEval(sourceText, callerRealm, evalRealm).

  PerformRealmEval ( sourceText, callerRealm, evalRealm )
  ...
  25. Return ? GetWrappedValue(callerRealm, result).

  GetWrappedValue ( callerRealm, value )
  ...
  2.b. Return ? WrappedFunctionCreate(callerRealm, value).

  WrappedFunctionCreate ( callerRealm, targetFunction )
  ...
  5. Set obj.[[Prototype]] to callerRealm.[[Intrinsics]].[[%Function.prototype%]].
features: [ShadowRealm, cross-realm, Reflect]
---*/

assert.sameValue(
  typeof ShadowRealm.prototype.evaluate,
  'function',
  'This test must fail if ShadowRealm.prototype.evaluate is not a function'
);

var other = $262.createRealm().global;
var OtherShadowRealm = other.eval('ShadowRealm');

var realm = Reflect.construct(OtherShadowRealm, []);
var fn = r.evaluate('() => {}');
assert.sameValue(Object.getPrototypeOf(fn), Function.prototype, 'WrappedFunction should be derived from the caller realm');
