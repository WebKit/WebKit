// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-RegExp-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
assert.sameValue(RegExp(/foo/my).flags, "my");
assert.sameValue(RegExp(/foo/, "gi").flags, "gi");
assert.sameValue(RegExp(/foo/my, "gi").flags, "gi");
assert.sameValue(RegExp(/foo/my, "").flags, "");
assert.sameValue(RegExp(/foo/my, undefined).flags, "my");
assertThrowsInstanceOf(() => RegExp(/foo/my, null), SyntaxError);
assertThrowsInstanceOf(() => RegExp(/foo/my, "foo"), SyntaxError);

assert.sameValue(/a/.compile("b", "gi").flags, "gi");
assert.sameValue(/a/.compile(/b/my).flags, "my");
assert.sameValue(/a/.compile(/b/my, undefined).flags, "my");
assertThrowsInstanceOf(() => /a/.compile(/b/my, "gi"), TypeError);
assertThrowsInstanceOf(() => /a/.compile(/b/my, ""), TypeError);
assertThrowsInstanceOf(() => /a/.compile(/b/my, null), TypeError);
assertThrowsInstanceOf(() => /a/.compile(/b/my, "foo"), TypeError);

