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
var summary = "Implement RegExp unicode flag -- raw unicode.";

print(BUGNUMBER + ": " + summary);

// ==== standalone ====

assert.compareArray(eval(`/[\uD83D\uDC38]/u`).exec("\u{1F438}"),
              ["\u{1F438}"]);

// no unicode flag
assert.compareArray(eval(`/[\uD83D\uDC38]/`).exec("\u{1F438}"),
              ["\uD83D"]);

// escaped (lead)
assert.sameValue(eval(`/[\\uD83D\uDC38]/u`).exec("\u{1F438}"),
         null);
assert.sameValue(eval(`/[\\u{D83D}\uDC38]/u`).exec("\u{1F438}"),
         null);

// escaped (trail)
assert.sameValue(eval(`/[\uD83D\\uDC38]/u`).exec("\u{1F438}"),
         null);
assert.sameValue(eval(`/[\uD83D\\u{DC38}]/u`).exec("\u{1F438}"),
         null);

// escaped (lead), no unicode flag
assert.compareArray(eval(`/[\\uD83D\uDC38]/`).exec("\u{1F438}"),
              ["\uD83D"]);

// escaped (trail), no unicode flag
assert.compareArray(eval(`/[\uD83D\\uDC38]/`).exec("\u{1F438}"),
              ["\uD83D"]);

// ==== RegExp constructor ====

assert.compareArray(new RegExp("[\uD83D\uDC38]", "u").exec("\u{1F438}"),
              ["\u{1F438}"]);

// no unicode flag
assert.compareArray(new RegExp("[\uD83D\uDC38]", "").exec("\u{1F438}"),
              ["\uD83D"]);

// escaped(lead)
assert.sameValue(new RegExp("[\\uD83D\uDC38]", "u").exec("\u{1F438}"),
         null);
assert.sameValue(new RegExp("[\\u{D83D}\uDC38]", "u").exec("\u{1F438}"),
         null);

// escaped(trail)
assert.sameValue(new RegExp("[\uD83D\\uDC38]", "u").exec("\u{1F438}"),
         null);
assert.sameValue(new RegExp("[\uD83D\\u{DC38}]", "u").exec("\u{1F438}"),
         null);

// escaped(lead), no unicode flag
assert.compareArray(new RegExp("[\\uD83D\uDC38]", "").exec("\u{1F438}"),
              ["\uD83D"]);

// escaped(trail), no unicode flag
assert.compareArray(new RegExp("[\uD83D\\uDC38]", "").exec("\u{1F438}"),
              ["\uD83D"]);

