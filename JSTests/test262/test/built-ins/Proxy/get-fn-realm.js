// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-getfunctionrealm
description: >
    The realm of a proxy exotic object is the realm of its target function
info: |
    7.3.22 GetFunctionRealm ( obj )

    [...]
    2. If obj has a [[Realm]] internal slot, then
       a. Return obj.[[Realm]].
    [...]
    4. If obj is a Proxy exotic object, then
       a. If obj.[[ProxyHandler]] is null, throw a TypeError exception.
       b. Let proxyTarget be obj.[[ProxyTarget]].
       c. Return ? GetFunctionRealm(proxyTarget).
features: [cross-realm, Proxy]
---*/

var other = $262.createRealm().global;
var C = new other.Function();
C.prototype = null;
var P = new Proxy(C, {});

assert.sameValue(Object.getPrototypeOf(new P()), other.Object.prototype);
