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
var BUGNUMBER = 1120169;
var summary = "Implement RegExp.prototype.{global, ignoreCase, multiline, sticky, unicode} - property descriptor";

print(BUGNUMBER + ": " + summary);

var getters = [
  "flags",
  "global",
  "ignoreCase",
  "multiline",
  "source",
  "sticky",
  "unicode",
];

for (var name of getters) {
  var desc = Object.getOwnPropertyDescriptor(RegExp.prototype, name);
  assert.sameValue(desc.configurable, true);
  assert.sameValue(desc.enumerable, false);
  assert.sameValue("writable" in desc, false);
  assert.sameValue("get" in desc, true);
}

