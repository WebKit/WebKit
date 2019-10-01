// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator use ToString
esid: sec-parseint-string-radix
description: Checking for Boolean object
---*/

assert.sameValue(parseInt(new Boolean(true)), NaN, "new Boolean(true)");
assert.sameValue(parseInt(new Boolean(false)), NaN, "new Boolean(false)");
