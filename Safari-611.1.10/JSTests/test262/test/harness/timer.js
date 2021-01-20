// Copyright (c) 2017 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    Including timer.js will expose:

        setTimeout()

includes: [timer.js,fnGlobalObject.js]
---*/

var gO = fnGlobalObject();

assert(typeof setTimeout === "function");
assert(typeof gO.setTimeout === "function");
assert.sameValue(gO.setTimeout, setTimeout);

// TODO: assert semantics
