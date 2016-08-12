// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-super-keyword
es6id: 12.3.5
description: ReferenceError is thrown when environment has no "super" binding
info: |
  [...]
  4. If the code matched by the syntactic production that is being evaluated is
     strict mode code, let strict be true, else let strict be false.
  5. Return ? MakeSuperPropertyReference(propertyKey, strict).

  12.3.5.3 Runtime Semantics: MakeSuperPropertyReference

  1. Let env be GetThisEnvironment( ).
  2. If env.HasSuperBinding() is false, throw a ReferenceError exception.
---*/

var caught;
function f() {
  // Early errors restricting the usage of SuperProperty necessitate the use of
  // `eval`.
  try {
    eval('super["x"];');
  } catch (err) {
    caught = err;
  }
}

f();

assert.sameValue(typeof caught, 'object');
assert.sameValue(caught.constructor, ReferenceError);
