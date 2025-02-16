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
var BUGNUMBER = 738511;
var summary =
  "new Date(value) should call ToPrimitive on value before testing for " +
  "string-ness";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assert.sameValue(new Date(new String("2012-01-31T00:00:00.000Z")).valueOf(),
         1327968000000,
         "Date constructor passed a String object should parse it");

/******************************************************************************/

print("Tests complete");
