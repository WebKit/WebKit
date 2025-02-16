// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-generators-shell.js, deepEqual.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// This file was written by Andy Wingo <wingo@igalia.com> and originally
// contributed to V8 as generators-objects.js, available here:
//
// http://code.google.com/p/v8/source/browse/branches/bleeding_edge/test/mjsunit/harmony/generators-objects.js

// Test aspects of the generator runtime.

// Test the properties and prototype of a generator object.
function TestGeneratorObject() {
  function* g() { yield 1; }

  var iter = g();
  assert.sameValue(Object.getPrototypeOf(iter), g.prototype);
  assertTrue(iter instanceof g);
  assert.sameValue(String(iter), "[object Generator]");
  assert.deepEqual(Object.getOwnPropertyNames(iter), []);
  assertNotEq(g(), iter);
}
TestGeneratorObject();


// Test the methods of generator objects.
function TestGeneratorObjectMethods() {
  function* g() { yield 1; }
  var iter = g();

  assert.sameValue(iter.next.length, 1);
  assert.sameValue(iter.return.length, 1);
  assert.sameValue(iter.throw.length, 1);

  function TestNonGenerator(non_generator) {
    assertThrowsInstanceOf(function() { iter.next.call(non_generator); }, TypeError);
    assertThrowsInstanceOf(function() { iter.next.call(non_generator, 1); }, TypeError);
    assertThrowsInstanceOf(function() { iter.return.call(non_generator, 1); }, TypeError);
    assertThrowsInstanceOf(function() { iter.throw.call(non_generator, 1); }, TypeError);
    assertThrowsInstanceOf(function() { iter.close.call(non_generator); }, TypeError);
  }

  TestNonGenerator(1);
  TestNonGenerator({});
  TestNonGenerator(function(){});
  TestNonGenerator(g);
  TestNonGenerator(g.prototype);
}
TestGeneratorObjectMethods();


