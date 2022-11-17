// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: >
  A Temporal.Calendar instance passed to since() in a property bag does
  not have its 'calendar' property observably checked
features: [Temporal]
---*/

const instance = new Temporal.PlainYearMonth(2019, 6);

const calendar = new Temporal.Calendar("iso8601");
Object.defineProperty(calendar, "calendar", {
  get() {
    throw new Test262Error("calendar.calendar should not be accessed");
  },
});

let arg = { year: 2019, monthCode: "M06", calendar };
instance.since(arg);

arg = { year: 2019, monthCode: "M06", calendar: { calendar } };
instance.since(arg);
