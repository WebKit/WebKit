// Copyright (C) 2021 Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DateTimeFormat.prototype.resolvedOptions
description: >
  Tests that Intl.DateTimeFormat.prototype.resolvedOptions calls
  OrdinaryHasInstance instead of the instanceof operator which includes a
  Symbol.hasInstance lookup and call among other things.
---*/

Object.defineProperty(Intl.DateTimeFormat, Symbol.hasInstance, {
    get() { throw new Test262Error(); }
});

Intl.DateTimeFormat();
