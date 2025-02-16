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
var summary = "Implement RegExp unicode flag -- disallow extended patterns.";

print(BUGNUMBER + ": " + summary);

// IdentityEscape

assert.compareArray(/\^\$\\\.\*\+\?\(\)\[\]\{\}\|/u.exec("^$\\.*+?()[]{}|"),
              ["^$\\.*+?()[]{}|"]);
assertThrowsInstanceOf(() => eval(`/\\A/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\-/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\U{10}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\U0000/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\uD83D\\U0000/u`), SyntaxError);

assert.compareArray(/[\^\$\\\.\*\+\?\(\)\[\]\{\}\|]+/u.exec("^$\\.*+?()[]{}|"),
              ["^$\\.*+?()[]{}|"]);
assertThrowsInstanceOf(() => eval(`/[\\A]/u`), SyntaxError);
assert.compareArray(/[A\-Z]+/u.exec("a-zABC"),
              ["-"]);
assertThrowsInstanceOf(() => eval(`/[\\U{10}]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\U0000]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\uD83D\\U0000]/u`), SyntaxError);

// PatternCharacter
assertThrowsInstanceOf(() => eval(`/{}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/{/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/{0}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/{1,}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/{1,2}/u`), SyntaxError);

// QuantifiableAssertion
assert.compareArray(/.B(?=A)/u.exec("cBaCBA"),
              ["CB"]);
assert.compareArray(/.B(?!A)/u.exec("CBAcBa"),
              ["cB"]);
assert.compareArray(/.B(?:A)/u.exec("cBaCBA"),
              ["CBA"]);
assert.compareArray(/.B(A)/u.exec("cBaCBA"),
              ["CBA", "A"]);

assertThrowsInstanceOf(() => eval(`/.B(?=A)+/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/.B(?!A)+/u`), SyntaxError);
assert.compareArray(/.B(?:A)+/u.exec("cBaCBA"),
              ["CBA"]);
assert.compareArray(/.B(A)+/u.exec("cBaCBA"),
              ["CBA", "A"]);

// ControlLetter
assert.compareArray(/\cA/u.exec("\u0001"),
              ["\u0001"]);
assert.compareArray(/\cZ/u.exec("\u001a"),
              ["\u001a"]);
assert.compareArray(/\ca/u.exec("\u0001"),
              ["\u0001"]);
assert.compareArray(/\cz/u.exec("\u001a"),
              ["\u001a"]);

assert.compareArray(/[\cA]/u.exec("\u0001"),
              ["\u0001"]);
assert.compareArray(/[\cZ]/u.exec("\u001a"),
              ["\u001a"]);
assert.compareArray(/[\ca]/u.exec("\u0001"),
              ["\u0001"]);
assert.compareArray(/[\cz]/u.exec("\u001a"),
              ["\u001a"]);

assertThrowsInstanceOf(() => eval(`/\\c/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\c1/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\c_/u`), SyntaxError);

assertThrowsInstanceOf(() => eval(`/[\\c]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\c1]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\c_]/u`), SyntaxError);

// HexEscapeSequence
assertThrowsInstanceOf(() => eval(`/\\x/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\x0/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\x1/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\x1G/u`), SyntaxError);

assertThrowsInstanceOf(() => eval(`/[\\x]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\x0]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\x1]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\x1G]/u`), SyntaxError);

// LegacyOctalEscapeSequence
assertThrowsInstanceOf(() => eval(`/\\52/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\052/u`), SyntaxError);

assertThrowsInstanceOf(() => eval(`/[\\52]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\052]/u`), SyntaxError);

// DecimalEscape
assert.compareArray(/\0/u.exec("\0"),
              ["\0"]);
assert.compareArray(/[\0]/u.exec("\0"),
              ["\0"]);
assert.compareArray(/\0A/u.exec("\0A"),
              ["\0A"]);
assert.compareArray(/\0G/u.exec("\0G"),
              ["\0G"]);
assert.compareArray(/(A.)\1/u.exec("ABACABAB"),
              ["ABAB", "AB"]);
assert.compareArray(/(A.)(B.)(C.)(D.)(E.)(F.)(G.)(H.)(I.)(J.)(K.)\10/u.exec("A1B2C3D4E5F6G7H8I9JaKbJa"),
              ["A1B2C3D4E5F6G7H8I9JaKbJa", "A1", "B2", "C3", "D4", "E5", "F6", "G7", "H8", "I9", "Ja", "Kb"]);

assertThrowsInstanceOf(() => eval(`/\\00/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\01/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\09/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\1/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\2/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\3/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\4/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\5/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\6/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\7/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\8/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\9/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\10/u`), SyntaxError);

assertThrowsInstanceOf(() => eval(`/[\\00]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\01]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\09]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\1]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\2]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\3]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\4]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\5]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\6]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\7]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\8]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\9]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\10]/u`), SyntaxError);

