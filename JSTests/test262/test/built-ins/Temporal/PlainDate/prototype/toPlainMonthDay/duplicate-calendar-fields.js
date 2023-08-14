// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.plaindate.prototype.toplainmonthday
description: If a calendar's fields() method returns duplicate field names, PrepareTemporalFields should throw a RangeError.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

for (const extra_fields of [['foo', 'foo'], ['monthCode'], ['day']]) {
    const calendar = TemporalHelpers.calendarWithExtraFields(extra_fields);
    const date = new Temporal.PlainDate(2023, 5, 1, calendar);

    assert.throws(RangeError, () => date.toPlainMonthDay());
}
