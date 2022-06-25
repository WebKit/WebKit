// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-internalizejsonproperty
description: Abrupt completion from array property deletion while reviving
info: |
  JSON.parse ( text [ , reviver ] )

  [...]
  7. If IsCallable(reviver) is true, then
     [...]
     e. Return ? InternalizeJSONProperty(root, rootName).

  Runtime Semantics: InternalizeJSONProperty ( holder, name)

  1. Let val be ? Get(holder, name).
  2. If Type(val) is Object, then
     a. Let isArray be ? IsArray(val).
     b. If isArray is true, then
        i. Set I to 0.
        ii. Let len be ? ToLength(? Get(val, "length")).
        iii. Repeat while I < len,
             1. Let newElement be ? InternalizeJSONProperty(val, !
                ToString(I)).
             2. If newElement is undefined, then
                a. Perform ? val.[[Delete]](! ToString(I)).
features: [Proxy]
---*/

var badDelete = new Proxy([0], {
  deleteProperty: function() {
    throw new Test262Error();
  }
});

assert.throws(Test262Error, function() {
  JSON.parse('[0,0]', function() {
    this[1] = badDelete;
  });
});
