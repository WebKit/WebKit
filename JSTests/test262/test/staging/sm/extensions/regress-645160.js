/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
function potatoMasher(obj, arg) { this.eval(arg); }
potatoMasher(this, "var s = Error().stack");
assert.sameValue(/potatoMasher/.exec(s) instanceof Array, true);

