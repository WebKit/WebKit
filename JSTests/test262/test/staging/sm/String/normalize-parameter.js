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
var BUGNUMBER = 918987;
var summary = 'String.prototype.normalize - passing wrong parameter';

print(BUGNUMBER + ": " + summary);

function test() {
  assertThrowsInstanceOf(() => "abc".normalize("NFE"), RangeError,
                         "String.prototype.normalize should raise RangeError on invalid form");

  assert.sameValue("".normalize(), "");
}

if ("normalize" in String.prototype) {
  // String.prototype.normalize is not enabled in all builds.
  test();
}

