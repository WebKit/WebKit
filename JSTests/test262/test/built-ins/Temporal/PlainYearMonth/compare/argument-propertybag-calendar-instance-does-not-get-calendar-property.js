// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.compare
description: >
  A Temporal.Calendar instance passed to compare() in a property bag does
  not have its 'calendar' property observably checked
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
Object.defineProperty(calendar, "calendar", {
  get() {
    throw new Test262Error("calendar.calendar should not be accessed");
  },
});

const arg = { year: 2019, monthCode: "M06", calendar };
Temporal.PlainYearMonth.compare(arg, arg);
