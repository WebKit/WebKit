/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-JSON-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
testJSON('-', true);
testJSON('+', true);
testJSON('-f', true);
testJSON('+f', true);
testJSON('00', true);
testJSON('01', true);
testJSON('1.', true);
testJSON('1.0e', true);
testJSON('1.0e+', true);
testJSON('1.0e-', true);
testJSON('1.0e+z', true);
testJSON('1.0e-z', true);
testJSON('1.0ee', true);
testJSON('1.e1', true);
testJSON('1.e+1', true);
testJSON('1.e-1', true);
testJSON('.', true);
testJSON('.1', true);
testJSON('.1e', true);
testJSON('.1e1', true);
testJSON('.1e+1', true);
testJSON('.1e-1', true);

/******************************************************************************/

print("Tests complete");
