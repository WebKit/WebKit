// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.from
description: >
  A Temporal.Calendar instance passed to from() in a property bag does
  not have its 'calendar' property observably checked
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
Object.defineProperty(calendar, "calendar", {
  get() {
    throw new Test262Error("calendar.calendar should not be accessed");
  },
});

let arg = { year: 1976, monthCode: "M11", day: 18, calendar };
Temporal.PlainDateTime.from(arg);

arg = { year: 1976, monthCode: "M11", day: 18, calendar: { calendar } };
Temporal.PlainDateTime.from(arg);
