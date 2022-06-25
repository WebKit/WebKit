// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.lastindexof
description: >
    Array.prototype.lastIndexOf - array element is +0 and search
    element is -0
---*/

assert.sameValue([+0].lastIndexOf(-0), 0, '[+0].lastIndexOf(-0)');
