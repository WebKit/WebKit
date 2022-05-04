// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getplaindatetimefor
description: Leap second is a valid ISO string for Instant
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

const arg = "2016-12-31T23:59:60Z";
const result = instance.getPlainDateTimeFor(arg);
TemporalHelpers.assertPlainDateTime(
  result,
  2016, 12, "M12", 31, 23, 59, 59, 0, 0, 0,
  "leap second is a valid ISO string for Instant"
);
