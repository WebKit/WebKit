// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.getisofields
description: Correct field names on the object returned from getISOFields
features: [Temporal]
---*/

const datetime = new Temporal.ZonedDateTime(1_000_086_400_987_654_321n, "UTC");

const result = datetime.getISOFields();
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
assert.sameValue(result.calendar.id, "iso8601", "calendar result");
assert.sameValue(result.timeZone.id, "UTC", "timeZone result");
