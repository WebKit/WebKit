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
if (typeof timeout == "function") {
    assert.sameValue(typeof timeout(), "number");
    assert.sameValue(typeof timeout(1), "undefined");
}

