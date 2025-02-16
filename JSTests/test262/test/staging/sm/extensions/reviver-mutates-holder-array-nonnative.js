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
var BUGNUMBER = 901380;
var summary = "Behavior when JSON.parse walks over a non-native object";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var typedArray = null;

var observedTypedArrayElementCount = 0;

var arr = JSON.parse('[0, 1]', function(prop, v) {
  if (prop === "0" && Array.isArray(this)) // exclude typedArray[0]
  {
    typedArray = new Int8Array(1);
    this[1] = typedArray;
  }
  if (this instanceof Int8Array)
  {
    assert.sameValue(prop, "0");
    observedTypedArrayElementCount++;
  }
  return v;
});

assert.sameValue(arr[0], 0);
assert.sameValue(arr[1], typedArray);

assert.sameValue(observedTypedArrayElementCount, 1);

/******************************************************************************/

print("Tests complete");
