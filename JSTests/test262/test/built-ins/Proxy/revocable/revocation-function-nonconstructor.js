// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-proxy.revocable
description: Proxy Revocation functions are not constructors
info: |
  17 ECMAScript Standard Built-in Objects:
    Built-in function objects that are not identified as constructors do not
    implement the [[Construct]] internal method unless otherwise specified
    in the description of a particular function.
includes: [isConstructor.js]
features: [Proxy, Reflect.construct]
---*/

var revocationFunction = Proxy.revocable({}, {}).revoke;

assert.sameValue(Object.prototype.hasOwnProperty.call(revocationFunction, "prototype"), false);
assert.sameValue(isConstructor(revocationFunction), false);
