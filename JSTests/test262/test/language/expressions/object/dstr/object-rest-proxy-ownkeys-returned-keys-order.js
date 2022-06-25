// Copyright (C) 2020 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-destructuring-binding-patterns-runtime-semantics-restbindinginitialization
description: >
  Proxy keys are iterated in order they were provided by "ownKeys" trap.
info: |
  BindingRestProperty : ... BindingIdentifier

  [...]
  3. Perform ? CopyDataProperties(restObj, value, excludedNames).

  CopyDataProperties ( target, source, excludedItems )

  [...]
  5. Let keys be ? from.[[OwnPropertyKeys]]().
  6. For each element nextKey of keys in List order, do
    [...]
    c. If excluded is false, then
      i. Let desc be ? from.[[GetOwnProperty]](nextKey).

  [[OwnPropertyKeys]] ( )

  [...]
  7. Let trapResultArray be ? Call(trap, handler, « target »).
  8. Let trapResult be ? CreateListFromArrayLike(trapResultArray, « String, Symbol »).
  [...]
  23. Return trapResult.
features: [object-rest, destructuring-binding, Proxy, Symbol]
includes: [compareArray.js]
---*/

var getOwnKeys = [];
var ownKeysResult = [Symbol(), "foo", "0"];
var proxy = new Proxy({}, {
  getOwnPropertyDescriptor: function(_target, key) {
    getOwnKeys.push(key);
  },
  ownKeys: function() {
    return ownKeysResult;
  },
});

let {...$} = proxy;
assert.compareArray(getOwnKeys, ownKeysResult);
