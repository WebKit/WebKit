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
var summary = "Implement RegExp unicode flag -- empty class should not match anything.";

print(BUGNUMBER + ": " + summary);

assert.sameValue(/[]/u.exec("A"),
         null);
assert.sameValue(/[]/u.exec("\uD83D"),
         null);
assert.sameValue(/[]/u.exec("\uDC38"),
         null);
assert.sameValue(/[]/u.exec("\uD83D\uDC38"),
         null);

assert.compareArray(/[^]/u.exec("A"),
              ["A"]);
assert.compareArray(/[^]/u.exec("\uD83D"),
              ["\uD83D"]);
assert.compareArray(/[^]/u.exec("\uDC38"),
              ["\uDC38"]);
assert.compareArray(/[^]/u.exec("\uD83D\uDC38"),
              ["\uD83D\uDC38"]);

