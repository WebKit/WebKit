// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.getisofields
description: getISOFields does not call into user code.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarThrowEverything();
const instance = new Temporal.ZonedDateTime(1_000_086_400_987_654_321n, "UTC", calendar);
const result = instance.getISOFields();

assert.sameValue(result.isoYear, 2001, "isoYear result");
assert.sameValue(result.isoMonth, 9, "isoMonth result");
assert.sameValue(result.isoDay, 10, "isoDay result");
assert.sameValue(result.isoHour, 1, "isoHour result");
assert.sameValue(result.isoMinute, 46, "isoMinute result");
assert.sameValue(result.isoSecond, 40, "isoSecond result");
assert.sameValue(result.isoMillisecond, 987, "isoMillisecond result");
assert.sameValue(result.isoMicrosecond, 654, "isoMicrosecond result");
assert.sameValue(result.isoNanosecond, 321, "isoNanosecond result");
assert.sameValue(result.offset, "+00:00", "offset result");
assert.sameValue(result.calendar, calendar, "calendar result");
assert.sameValue(result.timeZone, "UTC", "timeZone result");
