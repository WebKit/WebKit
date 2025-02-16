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
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Author: Emilio Cobos Álvarez <ecoal95@gmail.com>
 */
var BUGNUMBER = 1312948;
var summary = "Freezing a dictionary mode object with a length property should make Object.isFrozen report true";

print(BUGNUMBER + ": " + summary);

/* Convert to dictionary mode */
delete Array.prototype.slice;

Object.freeze(Array.prototype);
assert.sameValue(Object.isFrozen(Array.prototype), true);

