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
var BUGNUMBER = 620130;
var summary =
  "Calls to eval with same code + varying strict mode of script containing " +
  "eval == fail";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function t(code) { return eval(code); }

assert.sameValue(t("'use strict'; try { eval('with (5) 17'); } catch (e) { 'threw'; }"),
         "threw");
assert.sameValue(t("try { eval('with (5) 17'); } catch (e) { 'threw'; }"),
         17);
assert.sameValue(t("'use strict'; try { eval('with (5) 17'); } catch (e) { 'threw'; }"),
         "threw");

/******************************************************************************/

print("All tests passed!");
