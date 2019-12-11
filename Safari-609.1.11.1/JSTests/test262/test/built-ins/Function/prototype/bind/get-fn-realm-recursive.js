// Copyright (C) 2019 Aleksey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-getfunctionrealm
description: >
    The realm of a bound function exotic object is the realm of its target
    function. GetFunctionRealm is called recursively.
info: |
    7.3.22 GetFunctionRealm ( obj )

    [...]
    2. If obj has a [[Realm]] internal slot, then
       a. Return obj.[[Realm]].
    3. If obj is a Bound Function exotic object, then
       a. Let target be obj.[[BoundTargetFunction]].
       b. Return ? GetFunctionRealm(target).
features: [cross-realm]
---*/

var other = $262.createRealm().global;
var C = new other.Function();
C.prototype = null;
var B = C.bind().bind();

assert.sameValue(Object.getPrototypeOf(new B()), other.Object.prototype);
