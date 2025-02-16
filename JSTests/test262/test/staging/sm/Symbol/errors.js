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
// Section numbers cite ES6 rev 24 (2014 April 27).

var sym = Symbol();

// 7.2.2 IsCallable
assertThrowsInstanceOf(() => sym(), TypeError);
assertThrowsInstanceOf(() => Function.prototype.call.call(sym), TypeError);

// 7.2.5 IsConstructor
assertThrowsInstanceOf(() => new sym(), TypeError);
assertThrowsInstanceOf(() => new Symbol(), TypeError);

