// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-String-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1145326;
var summary = 'String.prototype.normalize error when normalization form parameter is not an atom';

print(BUGNUMBER + ": " + summary);

function test() {
  assert.sameValue("abc".normalize("NFKC".split("").join("")), "abc");
  assert.sameValue("abc".normalize("NFKCabc".replace("abc", "")), "abc");
  assert.sameValue("abc".normalize("N" + "F" + "K" + "C"), "abc");
}

if ("normalize" in String.prototype) {
  // String.prototype.normalize is not enabled in all builds.
  test();
}

