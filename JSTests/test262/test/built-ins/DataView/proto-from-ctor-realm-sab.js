// Copyright (C) 2016 the V8 project authors. All rights reserved.
// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-dataview-buffer-byteoffset-bytelength
description: Default [[Prototype]] value derived from realm of the newTarget
info: |
    [...]
    12. Let O be ? OrdinaryCreateFromConstructor(NewTarget,
        "%DataViewPrototype%", « [[DataView]], [[ViewedArrayBuffer]],
        [[ByteLength]], [[ByteOffset]] »).
    [...]

    9.1.14 GetPrototypeFromConstructor

    [...]
    3. Let proto be ? Get(constructor, "prototype").
    4. If Type(proto) is not Object, then
       a. Let realm be ? GetFunctionRealm(constructor).
       b. Let proto be realm's intrinsic object named intrinsicDefaultProto.
    [...]
features: [cross-realm, Reflect, SharedArrayBuffer]
---*/

var other = $262.createRealm().global;
var C = new other.Function();
C.prototype = null;
var buffer = new SharedArrayBuffer(0);

var o = Reflect.construct(DataView, [buffer, 0], C);

assert.sameValue(Object.getPrototypeOf(o), other.DataView.prototype);
