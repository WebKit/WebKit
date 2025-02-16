/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-Date-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 771946;
var summary = "Fractional days, months, years shouldn't trigger asserts";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

new Date(0).setFullYear(1.5);
new Date(0).setUTCDate(1.5);
new Date(0).setMonth(1.5);

/******************************************************************************/

print("Tests complete");
