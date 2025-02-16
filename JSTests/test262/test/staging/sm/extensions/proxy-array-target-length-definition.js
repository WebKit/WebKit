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
var gTestfile = 'proxy-array-target-length-definition.js';
var BUGNUMBER = 905947;
var summary =
  "Redefining an array's |length| property when redefining the |length| " +
  "property on a proxy with an array as target";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var arr = [];
var p = new Proxy(arr, {});

function assertThrowsTypeError(f)
{
    try {
        f();
        assert.sameValue(false, true, "Must have thrown");
    } catch (e) {
        assert.sameValue(e instanceof TypeError, true, "Must have thrown TypeError");
    }
}

// Redefining non-configurable length should throw a TypeError
assertThrowsTypeError(function () { Object.defineProperty(p, "length", { value: 17, configurable: true }); });

// Same here.
assertThrowsTypeError(function () { Object.defineProperty(p, "length", { value: 42, enumerable: true }); });

// Check the property went unchanged.
var pd = Object.getOwnPropertyDescriptor(p, "length");
assert.sameValue(pd.value, 0);
assert.sameValue(pd.writable, true);
assert.sameValue(pd.enumerable, false);
assert.sameValue(pd.configurable, false);

var ad = Object.getOwnPropertyDescriptor(arr, "length");
assert.sameValue(ad.value, 0);
assert.sameValue(ad.writable, true);
assert.sameValue(ad.enumerable, false);
assert.sameValue(ad.configurable, false);

/******************************************************************************/

print("Tests complete");
