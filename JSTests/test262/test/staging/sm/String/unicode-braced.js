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
var BUGNUMBER = 320500;
var summary = 'Add \\u{xxxxxx} string literals';

print(BUGNUMBER + ": " + summary);

assert.sameValue("\u{0}", String.fromCodePoint(0x0));
assert.sameValue("\u{1}", String.fromCodePoint(0x1));
assert.sameValue("\u{10}", String.fromCodePoint(0x10));
assert.sameValue("\u{100}", String.fromCodePoint(0x100));
assert.sameValue("\u{1000}", String.fromCodePoint(0x1000));
assert.sameValue("\u{D7FF}", String.fromCodePoint(0xD7FF));
assert.sameValue("\u{D800}", String.fromCodePoint(0xD800));
assert.sameValue("\u{DBFF}", String.fromCodePoint(0xDBFF));
assert.sameValue("\u{DC00}", String.fromCodePoint(0xDC00));
assert.sameValue("\u{DFFF}", String.fromCodePoint(0xDFFF));
assert.sameValue("\u{E000}", String.fromCodePoint(0xE000));
assert.sameValue("\u{10000}", String.fromCodePoint(0x10000));
assert.sameValue("\u{100000}", String.fromCodePoint(0x100000));
assert.sameValue("\u{10FFFF}", String.fromCodePoint(0x10FFFF));
assert.sameValue("\u{10ffff}", String.fromCodePoint(0x10FFFF));

assert.sameValue("A\u{1}\u{10}B\u{100}\u{1000}\u{10000}C\u{100000}",
         "A" +
         String.fromCodePoint(0x1) +
         String.fromCodePoint(0x10) +
         "B" +
         String.fromCodePoint(0x100) +
         String.fromCodePoint(0x1000) +
         String.fromCodePoint(0x10000) +
         "C" +
         String.fromCodePoint(0x100000));

assert.sameValue('\u{10ffff}', String.fromCodePoint(0x10FFFF));
assert.sameValue(`\u{10ffff}`, String.fromCodePoint(0x10FFFF));
assert.sameValue(`\u{10ffff}${""}`, String.fromCodePoint(0x10FFFF));
assert.sameValue(`${""}\u{10ffff}`, String.fromCodePoint(0x10FFFF));
assert.sameValue(`${""}\u{10ffff}${""}`, String.fromCodePoint(0x10FFFF));

assert.sameValue("\u{00}", String.fromCodePoint(0x0));
assert.sameValue("\u{00000000000000000}", String.fromCodePoint(0x0));
assert.sameValue("\u{00000000000001000}", String.fromCodePoint(0x1000));

assert.sameValue(eval(`"\\u{${"0".repeat(Math.pow(2, 24)) + "1234"}}"`), String.fromCodePoint(0x1234));

assert.sameValue("\U{0}", "U{0}");

assertThrowsInstanceOf(() => eval(`"\\u{-1}"`), SyntaxError);
assertThrowsInstanceOf(() => eval(`"\\u{0.0}"`), SyntaxError);
assertThrowsInstanceOf(() => eval(`"\\u{G}"`), SyntaxError);
assertThrowsInstanceOf(() => eval(`"\\u{}"`), SyntaxError);
assertThrowsInstanceOf(() => eval(`"\\u{{"`), SyntaxError);
assertThrowsInstanceOf(() => eval(`"\\u{"`), SyntaxError);
assertThrowsInstanceOf(() => eval(`"\\u{110000}"`), SyntaxError);
assertThrowsInstanceOf(() => eval(`"\\u{00110000}"`), SyntaxError);
assertThrowsInstanceOf(() => eval(`"\\u{100000000000000000000000000000}"`), SyntaxError);
assertThrowsInstanceOf(() => eval(`"\\u{FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF}"`), SyntaxError);
assertThrowsInstanceOf(() => eval(`"\\u{   FFFF}"`), SyntaxError);
assertThrowsInstanceOf(() => eval(`"\\u{FFFF   }"`), SyntaxError);
assertThrowsInstanceOf(() => eval(`"\\u{FF   FF}"`), SyntaxError);
assertThrowsInstanceOf(() => eval(`"\\u{F F F F}"`), SyntaxError);
assertThrowsInstanceOf(() => eval(`"\\u{100000001}"`), SyntaxError);

