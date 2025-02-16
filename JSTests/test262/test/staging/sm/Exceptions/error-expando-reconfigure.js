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
var gTestfile = "error-expando-reconfigure.js"
//-----------------------------------------------------------------------------
var BUGNUMBER = 961494;
var summary =
  "Reconfiguring the first expando property added to an Error object " +
  "shouldn't assert";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var err = new Error(); // no message argument => no err.message property
err.expando = 17;
Object.defineProperty(err, "expando", { configurable: false });

/******************************************************************************/

print("Tests complete");
