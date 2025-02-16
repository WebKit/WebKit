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
// NamedEvaluation applies to short-circuit assignment.

{
  let a;
  a ??= function(){};
  assert.sameValue(a.name, "a");
}

{
  let a = false;
  a ||= function(){};
  assert.sameValue(a.name, "a");
}

{
  let a = true;
  a &&= function(){};
  assert.sameValue(a.name, "a");
}

// No name assignments for parenthesised left-hand sides.

{
  let a;
  (a) ??= function(){};
  assert.sameValue(a.name, "");
}

{
  let a = false;
  (a) ||= function(){};
  assert.sameValue(a.name, "");
}

{
  let a = true;
  (a) &&= function(){};
  assert.sameValue(a.name, "");
}

