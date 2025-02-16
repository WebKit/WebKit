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
function make() {
  var r = {};
  r.desc = {get: function() {}};
  r.a = Object.defineProperty({}, "prop", r.desc);
  r.info = Object.getOwnPropertyDescriptor(r.a, "prop");
  return r;
}

r1 = make();
assert.sameValue(r1.desc.get, r1.info.get);

// Distinct evaluations of an object literal make distinct methods.
r2 = make();
assert.sameValue(r1.desc.get === r2.desc.get, false);

r1.info.get.foo = 42;

assert.sameValue(r1.desc.get.hasOwnProperty('foo'), !r2.desc.get.hasOwnProperty('foo'));
assert.sameValue(r1.info.get.hasOwnProperty('foo'), !r2.info.get.hasOwnProperty('foo'));

