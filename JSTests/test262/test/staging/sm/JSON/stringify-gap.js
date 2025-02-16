/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-JSON-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var gTestfile = 'stringify-gap.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 584909;
var summary =
  "JSON.stringify(_1, _2, numberGreaterThanOne) produces wrong output";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var LF = "\n";
var GAP = "   ";

var obj = { a: { b: [1, 2], c: { d: 3, e: 4 }, f: [], g: {}, h: [5], i: { j: 6 } } };

var expected =
  '{\n' +
  '   "a": {\n' +
  '      "b": [\n' +
  '         1,\n' +
  '         2\n' +
  '      ],\n' +
  '      "c": {\n' +
  '         "d": 3,\n' +
  '         "e": 4\n' +
  '      },\n' +
  '      "f": [],\n' +
  '      "g": {},\n' +
  '      "h": [\n' +
  '         5\n' +
  '      ],\n' +
  '      "i": {\n' +
  '         "j": 6\n' +
  '      }\n' +
  '   }\n' +
  '}';

assert.sameValue(JSON.stringify(obj, null, 3), expected);
assert.sameValue(JSON.stringify(obj, null, "   "), expected);

obj = [1, 2, 3];

String.prototype.toString = function() { return "--"; };

assert.sameValue(JSON.stringify(obj, null, new String("  ")), "[\n--1,\n--2,\n--3\n]");

Number.prototype.valueOf = function() { return 0; };

assert.sameValue(JSON.stringify(obj, null, new Number(3)), "[1,2,3]");

/******************************************************************************/

print("All tests passed!");
