// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
function bogusConstruct(target) { return 4; }
function bogusConstructUndefined(target) { }

var handler = { construct: bogusConstruct }

function callable() {}

var p = new Proxy(callable, handler);

assertThrowsInstanceOf(function () { new p(); }, TypeError,
                       "[[Construct must throw if an object is not returned.");

handler.construct = bogusConstructUndefined;
assertThrowsInstanceOf(function () { new p(); }, TypeError,
                       "[[Construct must throw if an object is not returned.");

