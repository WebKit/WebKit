// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If Z is empty, return NaN
esid: sec-parseint-string-radix
description: x is not a radix-R digit
---*/

assert.sameValue(parseInt("$0x"), NaN, "$0x");
assert.sameValue(parseInt("$0X"), NaN, "$0X");
assert.sameValue(parseInt("$$$"), NaN, "$$$");
assert.sameValue(parseInt(""), NaN, "the empty string");
assert.sameValue(parseInt(" "), NaN, "a string with a single space");
