// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-expressions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Test assignment to const and function name bindings. The latter is kind of a
// const binding, but only throws in strict mode.

function notEvaluated() {
  throw new Error("should not be evaluated");
}

// AndAssignExpr with const lexical binding.
{
  const a = false;
  a &&= notEvaluated();
  assert.sameValue(a, false);

  const b = true;
  assertThrowsInstanceOf(() => { b &&= 1; }, TypeError);
  assert.sameValue(b, true);
}

// AndAssignExpr with function name binding.
{
  let f = function fn() {
    fn &&= true;
    assert.sameValue(fn, f);
  };
  f();

  let g = function fn() {
    "use strict";
    assertThrowsInstanceOf(() => { fn &&= 1; }, TypeError);
    assert.sameValue(fn, g);
  };
  g();
}

// OrAssignExpr with const lexical binding.
{
  const a = true;
  a ||= notEvaluated();
  assert.sameValue(a, true);

  const b = false;
  assertThrowsInstanceOf(() => { b ||= 0; }, TypeError);
  assert.sameValue(b, false);
}

// OrAssignExpr with function name binding.
{
  let f = function fn() {
    fn ||= notEvaluated();
    assert.sameValue(fn, f);
  };
  f();

  let g = function fn() {
    "use strict";
    fn ||= notEvaluated();
    assert.sameValue(fn, g);
  };
  g();
}

// CoalesceAssignExpr with const lexical binding.
{
  const a = true;
  a ??= notEvaluated();
  assert.sameValue(a, true);

  const b = null;
  assertThrowsInstanceOf(() => { b ??= 0; }, TypeError);
  assert.sameValue(b, null);
}

// CoalesceAssignExpr with function name binding.
{
  let f = function fn() {
    fn ??= notEvaluated();
    assert.sameValue(fn, f);
  };
  f();

  let g = function fn() {
    "use strict";
    fn ??= notEvaluated();
    assert.sameValue(fn, g);
  };
  g();
}

