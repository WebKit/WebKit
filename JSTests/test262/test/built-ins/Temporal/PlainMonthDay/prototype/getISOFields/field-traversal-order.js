// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.getisofields
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

const md = new Temporal.PlainMonthDay(5, 2);
const result = md.getISOFields();

assert.compareArray(Object.keys(result), expected);
