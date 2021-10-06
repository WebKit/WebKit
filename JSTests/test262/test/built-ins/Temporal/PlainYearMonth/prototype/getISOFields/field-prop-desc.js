// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.getisofields
description: Properties on the returned object have the correct descriptor
includes: [propertyHelper.js]
features: [Temporal]
---*/

const expected = [
  "calendar",
  "isoDay",
  "isoMonth",
  "isoYear",
];

const ym = new Temporal.PlainYearMonth(2000, 5);
const result = ym.getISOFields();

for (const property of expected) {
  verifyProperty(result, property, {
    writable: true,
    enumerable: true,
    configurable: true,
  });
}
