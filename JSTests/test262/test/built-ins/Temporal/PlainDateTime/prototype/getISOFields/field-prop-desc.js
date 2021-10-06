// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.getisofields
description: Properties on the returned object have the correct descriptor
includes: [propertyHelper.js]
features: [Temporal]
---*/

const expected = [
  "calendar",
  "isoDay",
  "isoHour",
  "isoMicrosecond",
  "isoMillisecond",
  "isoMinute",
  "isoMonth",
  "isoNanosecond",
  "isoSecond",
  "isoYear",
];

const datetime = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321);
const result = datetime.getISOFields();

for (const property of expected) {
  verifyProperty(result, property, {
    writable: true,
    enumerable: true,
    configurable: true,
  });
}
