// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-String-shell.js, compareArray.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- AdvanceStringIndex in global match and replace.";

print(BUGNUMBER + ": " + summary);

// ==== String.prototype.match ====

assert.compareArray("\uD83D\uDC38\uD83D\uDC39X\uD83D\uDC3A".match(/\uD83D|X|/gu),
              ["", "", "X", "", ""]);
assert.compareArray("\uD83D\uDC38\uD83D\uDC39X\uD83D\uDC3A".match(/\uDC38|X|/gu),
              ["", "", "X", "", ""]);
assert.compareArray("\uD83D\uDC38\uD83D\uDC39X\uD83D\uDC3A".match(/\uD83D\uDC38|X|/gu),
              ["\uD83D\uDC38", "", "X", "", ""]);

// ==== String.prototype.replace ====

// empty string replacement (optimized)
assert.compareArray("\uD83D\uDC38\uD83D\uDC39X\uD83D\uDC3A".replace(/\uD83D|X|/gu, ""),
              "\uD83D\uDC38\uD83D\uDC39\uD83D\uDC3A");
assert.compareArray("\uD83D\uDC38\uD83D\uDC39X\uD83D\uDC3A".replace(/\uDC38|X|/gu, ""),
              "\uD83D\uDC38\uD83D\uDC39\uD83D\uDC3A");
assert.compareArray("\uD83D\uDC38\uD83D\uDC39X\uD83D\uDC3A".replace(/\uD83D\uDC38|X|/gu, ""),
              "\uD83D\uDC39\uD83D\uDC3A");

// non-empty string replacement
assert.compareArray("\uD83D\uDC38\uD83D\uDC39X\uD83D\uDC3A".replace(/\uD83D|X|/gu, "x"),
              "x\uD83D\uDC38x\uD83D\uDC39xx\uD83D\uDC3Ax");
assert.compareArray("\uD83D\uDC38\uD83D\uDC39X\uD83D\uDC3A".replace(/\uDC38|X|/gu, "x"),
              "x\uD83D\uDC38x\uD83D\uDC39xx\uD83D\uDC3Ax");
assert.compareArray("\uD83D\uDC38\uD83D\uDC39X\uD83D\uDC3A".replace(/\uD83D\uDC38|X|/gu, "x"),
              "xx\uD83D\uDC39xx\uD83D\uDC3Ax");

// ==== String.prototype.split ====

assert.compareArray("\uD83D\uDC38\uD83D\uDC39X\uD83D\uDC3A".split(/\uD83D|X|/u),
              ["\uD83D\uDC38", "\uD83D\uDC39", "\uD83D\uDC3A"]);
assert.compareArray("\uD83D\uDC38\uD83D\uDC39X\uD83D\uDC3A".split(/\uDC38|X|/u),
              ["\uD83D\uDC38", "\uD83D\uDC39", "\uD83D\uDC3A"]);
assert.compareArray("\uD83D\uDC38\uD83D\uDC39X\uD83D\uDC3A".split(/\uD83D\uDC38|X|/u),
              ["", "\uD83D\uDC39", "\uD83D\uDC3A"]);

