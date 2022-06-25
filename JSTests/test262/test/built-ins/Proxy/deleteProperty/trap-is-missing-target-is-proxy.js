// Copyright (C) 2020 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-proxy-object-internal-methods-and-internal-slots-delete-p
description: >
  If "deleteProperty" trap is null or undefined, [[Delete]] call is
  properly forwarded to [[ProxyTarget]] (which is also a Proxy object).
info: |
  [[Delete]] ( P )

  [...]
  5. Let target be O.[[ProxyTarget]].
  6. Let trap be ? GetMethod(handler, "deleteProperty").
  7. If trap is undefined, then
    a. Return ? target.[[Delete]](P).
features: [Proxy, Reflect]
---*/

var plainObject = {
  get foo() {},
};

Object.defineProperty(plainObject, "bar", {
  configurable: false,
});

var plainObjectTarget = new Proxy(plainObject, {});
var plainObjectProxy = new Proxy(plainObjectTarget, {});

assert(delete plainObjectProxy.foo);
assert(!plainObject.hasOwnProperty("foo"));

assert(!Reflect.deleteProperty(plainObjectProxy, "bar"));
assert(plainObject.hasOwnProperty("bar"));


var func = function() {};
var funcTarget = new Proxy(func, {});
var funcProxy = new Proxy(funcTarget, {});

assert(delete funcProxy.length);
assert(!func.hasOwnProperty("length"));

assert.throws(TypeError, function() {
  "use strict";
  delete funcProxy.prototype;
});
