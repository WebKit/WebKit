// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-RegExp-shell.js, compareArray.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- ignoreCase flag with negated character class.";

print(BUGNUMBER + ": " + summary);

assert.sameValue(/[^A]/iu.exec("A"),
         null);
assert.sameValue(/[^a]/iu.exec("A"),
         null);
assert.sameValue(/[^A]/iu.exec("a"),
         null);
assert.sameValue(/[^a]/iu.exec("a"),
         null);

assert.compareArray(/[^A]/iu.exec("b"),
              ["b"]);

