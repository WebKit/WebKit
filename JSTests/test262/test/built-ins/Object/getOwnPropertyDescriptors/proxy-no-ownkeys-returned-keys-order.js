// Copyright (C) 2020 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-object.getownpropertydescriptors
description: >
  If Proxy "ownKeys" trap is missing, keys are sorted by type in ascending
  chronological order.
info: |
  Object.getOwnPropertyDescriptors ( O )

  [...]
  2. Let ownKeys be ? obj.[[OwnPropertyKeys]]().
  [...]
  4. For each element key of ownKeys in List order, do
    a. Let desc be ? obj.[[GetOwnProperty]](key).

  [[OwnPropertyKeys]] ( )

  [...]
  6. If trap is undefined, then
    a. Return ? target.[[OwnPropertyKeys]]().

  OrdinaryOwnPropertyKeys ( O )

  [...]
  3. For each own property key P of O such that Type(P) is String and P is
  not an array index, in ascending chronological order of property creation, do
    a. Add P as the last element of keys.
  4. For each own property key P of O such that Type(P) is Symbol,
  in ascending chronological order of property creation, do
    a. Add P as the last element of keys.
  5. Return keys.
features: [Proxy, Symbol]
includes: [compareArray.js]
---*/

var target = {};
var sym = Symbol();
target[sym] = 1;
target.foo = 2;
target[0] = 3;

var getOwnKeys = [];
var proxy = new Proxy(target, {
  getOwnPropertyDescriptor: function(_target, key) {
    getOwnKeys.push(key);
  },
});

Object.getOwnPropertyDescriptors(proxy);
assert.compareArray(getOwnKeys, ["0", "foo", sym]);
