// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.zoneddatetime.prototype.since
description: If a calendar's fields() method returns duplicate field names, PrepareTemporalFields should throw a RangeError.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

for (const extra_fields of [['foo', 'foo'], ['day'], ['hour'], ['microsecond'], ['millisecond'], ['minute'], ['month'], ['monthCode'], ['nanosecond'], ['second'], ['year'], ['timeZone'], ['offset']]) {
    const calendar = TemporalHelpers.calendarWithExtraFields(extra_fields);
    const timeZone = 'Europe/Paris'
    const arg = { year: 2023, month: 5, monthCode: 'M05', day: 1, calendar: calendar, timeZone: timeZone };
    const instance = new Temporal.ZonedDateTime(0n, timeZone);

    assert.throws(RangeError, () => instance.since(arg));
}
