// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.now
description: Date.now must exist as a function
---*/

var fun = Date.now;

assert.sameValue(typeof(fun), "function", 'typeof (fun)');
