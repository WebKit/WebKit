// Copyright 2012 Google Inc.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.3.1
description: >
    Tests that Intl.NumberFormat.prototype.constructor is the
    Intl.NumberFormat.
author: Roozbeh Pournader
---*/

assert.sameValue(Intl.NumberFormat.prototype.constructor, Intl.NumberFormat, "Intl.NumberFormat.prototype.constructor is not the same as Intl.NumberFormat");
