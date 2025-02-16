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
var gTestfile = 'array-pop-proxy.js';
var BUGNUMBER = 858381;
var summary = "Behavior of [].pop on proxies";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var p = new Proxy([0, 1, 2], {});
Array.prototype.pop.call(p);

/******************************************************************************/

print("Tests complete");
