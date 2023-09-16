// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.zoneddatetime.prototype.with
description: If a calendar's fields() method returns duplicate field names, PrepareTemporalFields should throw a RangeError.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

for (const extra_fields of [['foo', 'foo'], ['monthCode'], ['day'], ['hour'], ['microsecond'], ['millisecond'], ['minute'], ['month'], ['nanosecond'], ['second'], ['year'], ['offset']]) {
    const calendar = TemporalHelpers.calendarWithExtraFields(extra_fields);
    const zoneddatetime = new Temporal.ZonedDateTime(1682892000000000000n, 'Europe/Madrid', calendar);

    assert.throws(RangeError, () => zoneddatetime.with({hour: 12}));
}
