// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 12.2.5
description: >
    In an object, duplicate computed property getter names produce only a single property of
    that name, whose value is the value of the last property of that name.
---*/
var calls = 0;
var A = {
  set ['a'](_) {
    calls++;
  }
};
A.a = 'A';
assert.sameValue(calls, 1, "The value of `calls` is `1`, after executing `A.a = 'A';`");
