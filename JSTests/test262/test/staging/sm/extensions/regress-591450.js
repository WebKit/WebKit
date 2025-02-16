/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-extensions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
/*
 * This was causing the parser to assert at one point. Now it's not. Yay!
 */
function f(a,[x,y],b,[w,z],c) { function b() { } }

assert.sameValue(0, 0, "don't crash");
