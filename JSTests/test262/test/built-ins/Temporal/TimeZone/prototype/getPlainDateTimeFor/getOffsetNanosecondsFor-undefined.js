// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getplaindatetimefor
description: Fall back to calling intrinsic getOffsetNanosecondsFor() when method is deleted
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instant = Temporal.Instant.from("1975-02-02T14:25:36.123456789Z");
const timeZone = Temporal.TimeZone.from("Europe/Madrid");
timeZone.getOffsetNanosecondsFor = undefined;
const result = timeZone.getPlainDateTimeFor(instant);
TemporalHelpers.assertPlainDateTime(result, 1975, 2, "M02", 2, 15, 25, 36, 123, 456, 789);
