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
var expect = 42;
var actual = (function({
    x: [w]
},
x) {
    with({}) {return w;}
})({x:[42]});

assert.sameValue(expect, actual, "ok");
