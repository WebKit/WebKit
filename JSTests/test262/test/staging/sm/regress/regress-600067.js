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
/*
 * NB: this test hardcodes for the value of PropertyTable::HASH_THRESHOLD (6).
 */

function s(e) {
  var a, b, c, d;
  function e() { }
}

assert.sameValue(0, 0, "don't crash");
