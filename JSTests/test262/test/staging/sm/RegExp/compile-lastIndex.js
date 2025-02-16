/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-RegExp-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1253099;
var summary =
  "RegExp.prototype.compile must perform all its steps *except* setting " +
  ".lastIndex, then throw, when provided a RegExp whose .lastIndex has been " +
  "made non-writable";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var regex = /foo/i;

// Aside from making .lastIndex non-writable, this has one incidental effect
// ubiquitously tested through the remainder of this test:
//
//   * RegExp.prototype.compile will do everything it ordinarily does, BUT it
//     will throw a TypeError when attempting to zero .lastIndex immediately
//     before succeeding overall.
//
// Ain't it great?
Object.defineProperty(regex, "lastIndex", { value: 42, writable: false });

assert.sameValue(regex.global, false);
assert.sameValue(regex.ignoreCase, true);
assert.sameValue(regex.multiline, false);
assert.sameValue(regex.unicode, false);
assert.sameValue(regex.sticky, false);
assert.sameValue(Object.getOwnPropertyDescriptor(regex, "lastIndex").writable, false);
assert.sameValue(regex.lastIndex, 42);

assert.sameValue(regex.test("foo"), true);
assert.sameValue(regex.test("FOO"), true);
assert.sameValue(regex.test("bar"), false);
assert.sameValue(regex.test("BAR"), false);

assertThrowsInstanceOf(() => regex.compile("bar"), TypeError);

assert.sameValue(regex.global, false);
assert.sameValue(regex.ignoreCase, false);
assert.sameValue(regex.multiline, false);
assert.sameValue(regex.unicode, false);
assert.sameValue(regex.sticky, false);
assert.sameValue(Object.getOwnPropertyDescriptor(regex, "lastIndex").writable, false);
assert.sameValue(regex.lastIndex, 42);
assert.sameValue(regex.test("foo"), false);
assert.sameValue(regex.test("FOO"), false);
assert.sameValue(regex.test("bar"), true);
assert.sameValue(regex.test("BAR"), false);

assertThrowsInstanceOf(() => regex.compile("^baz", "m"), TypeError);

assert.sameValue(regex.global, false);
assert.sameValue(regex.ignoreCase, false);
assert.sameValue(regex.multiline, true);
assert.sameValue(regex.unicode, false);
assert.sameValue(regex.sticky, false);
assert.sameValue(Object.getOwnPropertyDescriptor(regex, "lastIndex").writable, false);
assert.sameValue(regex.lastIndex, 42);
assert.sameValue(regex.test("foo"), false);
assert.sameValue(regex.test("FOO"), false);
assert.sameValue(regex.test("bar"), false);
assert.sameValue(regex.test("BAR"), false);
assert.sameValue(regex.test("baz"), true);
assert.sameValue(regex.test("BAZ"), false);
assert.sameValue(regex.test("012345678901234567890123456789012345678901baz"), false);
assert.sameValue(regex.test("012345678901234567890123456789012345678901\nbaz"), true);
assert.sameValue(regex.test("012345678901234567890123456789012345678901BAZ"), false);
assert.sameValue(regex.test("012345678901234567890123456789012345678901\nBAZ"), false);

/******************************************************************************/

print("Tests complete");
