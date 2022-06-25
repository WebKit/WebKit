// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withplaindate
description: >
    Calendar.dateFromFields method is called with undefined as the options value
    when call originates internally
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarFromFieldsUndefinedOptions();
const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", calendar);
instance.withPlainDate({ year: 2000, month: 5, day: 3, calendar });
assert.sameValue(calendar.dateFromFieldsCallCount, 1);
