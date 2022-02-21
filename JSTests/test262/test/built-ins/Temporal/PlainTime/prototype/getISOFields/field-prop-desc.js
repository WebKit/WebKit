// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.getisofields
description: Properties on the returned object have the correct descriptor
includes: [propertyHelper.js]
features: [Temporal]
---*/

const expected = [
  "calendar",
  "isoHour",
  "isoMicrosecond",
  "isoMillisecond",
  "isoMinute",
  "isoNanosecond",
  "isoSecond",
];

const time = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);
const result = time.getISOFields();

for (const property of expected) {
  verifyProperty(result, property, {
    writable: true,
    enumerable: true,
    configurable: true,
  });
}
