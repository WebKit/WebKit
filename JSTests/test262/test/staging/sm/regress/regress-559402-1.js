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
var expect = "No error";
var actual = expect;

try {
    eval('function foo() { "use strict"; }');
} catch (e) {
    actual = '' + e;
}

assert.sameValue(expect, actual, "ok");
