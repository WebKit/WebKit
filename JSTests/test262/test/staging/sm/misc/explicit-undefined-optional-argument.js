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
var BUGNUMBER = 373118;
var summary =
  'Properly handle explicitly-undefined optional arguments to a bunch of ' +
  'functions';

print(BUGNUMBER + ": " + summary);

//-----------------------------------------------------------------------------

var a;

a = "abc".slice(0, undefined);
assert.sameValue(a, "abc");

a = "abc".substr(0, undefined);
assert.sameValue(a, "abc");

a = "abc".substring(0, undefined);
assert.sameValue(a, "abc");

a = [1, 2, 3].slice(0, undefined);
assert.sameValue(a.join(), '1,2,3');

a = [1, 2, 3].sort(undefined);
assert.sameValue(a.join(), '1,2,3');

assert.sameValue((20).toString(undefined), '20');

//-----------------------------------------------------------------------------

