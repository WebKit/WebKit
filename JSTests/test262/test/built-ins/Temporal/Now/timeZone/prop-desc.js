// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.timezone
description: The "timeZone" property of Temporal.Now
info: |
  Section 17: Every other data property described in clauses 18 through 26
  and in Annex B.2 has the attributes { [[Writable]]: true,
  [[Enumerable]]: false, [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
features: [Temporal]
---*/

verifyProperty(Temporal.Now, 'timeZone', {
  enumerable: false,
  writable: true,
  configurable: true
});
