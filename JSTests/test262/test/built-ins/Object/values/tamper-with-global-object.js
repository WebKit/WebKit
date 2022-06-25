// Copyright (C) 2015 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-object.values
description: >
    Object.values should not have its behavior impacted by modifications to the global property Object
author: Jordan Harband
---*/

function fakeObject() {
  throw new Test262Error('The overriden version of Object was called!');
}
fakeObject.values = Object.values;

var global = Function('return this;')();
global.Object = fakeObject;

assert.sameValue(Object, fakeObject, 'Sanity check failed: could not modify the global Object');
assert.sameValue(Object.values(1).length, 0, 'Expected number primitive to have zero values');
