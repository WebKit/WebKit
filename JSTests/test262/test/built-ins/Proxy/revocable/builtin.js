// Copyright (C) 2020 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-proxy.revocable
description: >
  Requirements for built-in functions, defined in introduction of chapter 17,
  are satisfied.
includes: [isConstructor.js]
features: [Proxy, Reflect.construct]
---*/

assert(Object.isExtensible(Proxy.revocable));
assert.sameValue(typeof Proxy.revocable, 'function');
assert.sameValue(Object.prototype.toString.call(Proxy.revocable), '[object Function]');
assert.sameValue(Object.getPrototypeOf(Proxy.revocable), Function.prototype);

assert.sameValue(Proxy.revocable.hasOwnProperty('prototype'), false);
assert.sameValue(isConstructor(Proxy.revocable), false);
