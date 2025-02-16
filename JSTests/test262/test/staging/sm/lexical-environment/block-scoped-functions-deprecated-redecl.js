// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
{
  assert.sameValue(f(), 4);
  function f() { return 3; }
  assert.sameValue(f(), 4);
  function f() { return 4; }
  assert.sameValue(f(), 4);
}

// Annex B still works.
assert.sameValue(f(), 4);

// The same thing with labels.
{
  assert.sameValue(f(), 4);
  function f() { return 3; }
  assert.sameValue(f(), 4);
  l: function f() { return 4; }
  assert.sameValue(f(), 4);
}

// Annex B still works.
assert.sameValue(f(), 4);

function test() {
  {
    assert.sameValue(f(), 2);
    function f() { return 1; }
    assert.sameValue(f(), 2);
    function f() { return 2; }
    assert.sameValue(f(), 2);
  }

  // Annex B still works.
  assert.sameValue(f(), 2);
}

test();

var log = '';

try {
  // Strict mode still cannot redeclare.
  eval(`"use strict";
  {
    function f() { }
    function f() { }
  }`);
} catch (e) {
  assert.sameValue(e instanceof SyntaxError, true);
  log += 'e';
}

try {
  // Redeclaring an explicitly 'let'-declared binding doesn't work.
  eval(`{
    let x = 42;
    function x() {}
  }`);
} catch (e) {
  assert.sameValue(e instanceof SyntaxError, true);
  log += 'e';
}

try {
  // Redeclaring an explicitly 'const'-declared binding doesn't work.
  eval(`{
    const x = 42;
    function x() {}
  }`);
} catch (e) {
  assert.sameValue(e instanceof SyntaxError, true);
  log += 'e';
}

assert.sameValue(log, 'eee');

