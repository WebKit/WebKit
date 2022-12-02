// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearOfWeek
description: yearOfWeek() where the result is different from the calendar year
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
assert.sameValue(iso.yearOfWeek(Temporal.PlainDate.from("2019-12-31")), 2020, "next year");
assert.sameValue(iso.yearOfWeek(Temporal.PlainDate.from("2021-01-01")), 2020, "previous year");
