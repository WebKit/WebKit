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
var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- disallow range with CharacterClassEscape.";

print(BUGNUMBER + ": " + summary);

assertThrowsInstanceOf(() => eval(`/[\\w-\\uFFFF]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\W-\\uFFFF]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\d-\\uFFFF]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\D-\\uFFFF]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\s-\\uFFFF]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\S-\\uFFFF]/u`), SyntaxError);

assertThrowsInstanceOf(() => eval(`/[\\uFFFF-\\w]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\uFFFF-\\W]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\uFFFF-\\d]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\uFFFF-\\D]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\uFFFF-\\s]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\uFFFF-\\S]/u`), SyntaxError);

assertThrowsInstanceOf(() => eval(`/[\\w-\\w]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\W-\\W]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\d-\\d]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\D-\\D]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\s-\\s]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\S-\\S]/u`), SyntaxError);

