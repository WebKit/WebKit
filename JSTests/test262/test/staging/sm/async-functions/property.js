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
var BUGNUMBER = 1185106;
var summary = "async name token in property and object destructuring pattern";

print(BUGNUMBER + ": " + summary);

{
  let a = { async: 10 };
  assert.sameValue(a.async, 10);
}

{
  let a = { async() {} };
  assert.sameValue(a.async instanceof Function, true);
  assert.sameValue(a.async.name, "async");
}

{
  let async = 11;
  let a = { async };
  assert.sameValue(a.async, 11);
}

{
  let { async } = { async: 12 };
  assert.sameValue(async, 12);
}

{
  let { async = 13 } = {};
  assert.sameValue(async, 13);
}

{
  let { async: a = 14 } = {};
  assert.sameValue(a, 14);
}

{
  let { async, other } = { async: 15, other: 16 };
  assert.sameValue(async, 15);
  assert.sameValue(other, 16);

  let a = { async, other };
  assert.sameValue(a.async, 15);
  assert.sameValue(a.other, 16);
}

