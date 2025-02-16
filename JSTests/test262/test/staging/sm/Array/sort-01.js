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
//-----------------------------------------------------------------------------
var BUGNUMBER = 604971;
var summary = 'array.sort compare-function gets incorrect this';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

[1, 2, 3].sort(function() { "use strict"; assert.sameValue(this, undefined); });

/******************************************************************************/

print("All tests passed!");
