// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.compare
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

const timeZone = new Temporal.TimeZone("UTC");
const arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar };
Temporal.ZonedDateTime.compare(arg, arg);
