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
var gTestfile = 'ownkeys-trap-duplicates.js';
var BUGNUMBER = 1293995;
var summary =
  "Scripted proxies' [[OwnPropertyKeys]] should not throw if the trap " +
  "implementation returns duplicate properties and the object is " +
  "non-extensible or has non-configurable properties." +
  "Revised (bug 1389752): Throw TypeError for duplicate properties.";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var target = Object.preventExtensions({ a: 1 });
var proxy = new Proxy(target, { ownKeys(t) { return ["a", "a"]; } });
assertThrowsInstanceOf(() => Object.getOwnPropertyNames(proxy), TypeError);

target = Object.freeze({ a: 1 });
proxy = new Proxy(target, { ownKeys(t) { return ["a", "a"]; } });
assertThrowsInstanceOf(() => Object.getOwnPropertyNames(proxy), TypeError);

/******************************************************************************/

print("Tests complete");
