// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.getisofields
description: Properties added in correct order to object returned from getISOFields
includes: [compareArray.js]
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

assert.compareArray(Object.keys(result), expected);
