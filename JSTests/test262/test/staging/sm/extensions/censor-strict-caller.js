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

function nonstrict() { return nonstrict.caller; }
function strict() { "use strict"; return nonstrict(); }

assert.sameValue(strict(), null);

/******************************************************************************/

print("All tests passed!");
