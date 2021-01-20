// Copyright (C) 2018 Valerie Young. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-asynciteratorprototype-asynciterator
description: Return value of @@asyncIterator
info: |
  %AsyncIteratorPrototype% [ @@asyncIterator ] ( )
    1. Return the this value.
features: [Symbol.asyncIterator, async-iteration]
---*/

async function* generator() {}
var AsyncIteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf(generator.prototype))

var thisValue = {};

assert.sameValue(
  AsyncIteratorPrototype[Symbol.asyncIterator].call(thisValue),
  thisValue
);
