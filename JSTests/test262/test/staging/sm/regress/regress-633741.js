/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
info: |
  preventExtensions on global
description: |
  pending
esid: pending
---*/
Object.preventExtensions(this);
delete Function;

try {
    /* Don't assert. */
    Object.getOwnPropertyNames(this);
} catch(e) {
    assert.sameValue(true, false, "this shouldn't have thrown");
}
