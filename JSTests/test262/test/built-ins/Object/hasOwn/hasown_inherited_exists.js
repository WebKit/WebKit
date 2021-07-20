// Copyright (c) 2021 Jamie Kyle.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-object.hasown
description: Properties - [[HasOwnProperty]] (old style inherited property)
author: Jamie Kyle
features: [Object.hasOwn]
---*/

var base = {
  foo: 42
};
var o = Object.create(base);

assert.sameValue(Object.hasOwn(o, "foo"), false, 'Object.hasOwn(o, "foo")');
