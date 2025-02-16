/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = '523401';
var summary = 'numeric literal followed by an identifier';

var array = new Array();
assertThrowsInstanceOf(() => eval("array[0for]"), SyntaxError);
assertThrowsInstanceOf(() => eval("array[1yield]"), SyntaxError);
assertThrowsInstanceOf(() => eval("array[2in []]"), SyntaxError); // "2 in []" is valid.
assert.sameValue(array[2 in []], undefined);
assert.sameValue(2 in [], false);
assertThrowsInstanceOf(() => eval("array[3in]"), SyntaxError);
