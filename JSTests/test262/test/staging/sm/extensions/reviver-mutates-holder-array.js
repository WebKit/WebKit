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
//-----------------------------------------------------------------------------
var BUGNUMBER = 901351;
var summary = "Behavior when the JSON.parse reviver mutates the holder array";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var proxyObj = null;

var arr = JSON.parse('[0, 1]', function(prop, v) {
  if (prop === "0")
  {
    proxyObj = new Proxy({ c: 17, d: 42 }, {});
    this[1] = proxyObj;
  }
  return v;
});

assert.sameValue(arr[0], 0);
assert.sameValue(arr[1], proxyObj);
assert.sameValue(arr[1].c, 17);
assert.sameValue(arr[1].d, 42);

/******************************************************************************/

print("Tests complete");
