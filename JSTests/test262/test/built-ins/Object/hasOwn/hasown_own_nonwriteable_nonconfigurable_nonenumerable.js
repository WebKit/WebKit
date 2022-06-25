// Copyright (c) 2021 Jamie Kyle.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-object.hasown
description: >
    Properties - [[HasOwnProperty]] (non-writable, non-configurable,
    non-enumerable own value property)
author: Jamie Kyle
features: [Object.hasOwn]
---*/

var o = {};
Object.defineProperty(o, "foo", {
  value: 42
});

assert.sameValue(Object.hasOwn(o, "foo"), true, 'Object.hasOwn(o, "foo") !== true');
