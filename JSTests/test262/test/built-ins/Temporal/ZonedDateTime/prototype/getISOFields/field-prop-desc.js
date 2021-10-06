// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.getisofields
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
  "offset",
  "timeZone",
];

const datetime = new Temporal.ZonedDateTime(1_000_086_400_987_654_321n, "UTC");
const result = datetime.getISOFields();

for (const property of expected) {
  verifyProperty(result, property, {
    writable: true,
    enumerable: true,
    configurable: true,
  });
}
