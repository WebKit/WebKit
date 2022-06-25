// Copyright (C) 2021 Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.NumberFormat.prototype.resolvedOptions
description: >
  Tests that Intl.NumberFormat.prototype.resolvedOptions calls
  OrdinaryHasInstance instead of the instanceof operator which includes a
  Symbol.hasInstance lookup and call among other things.
---*/

const nf = new Intl.NumberFormat();

Object.defineProperty(Intl.NumberFormat, Symbol.hasInstance, {
    get() { throw new Test262Error(); }
});

nf.resolvedOptions();
