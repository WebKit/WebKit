// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.compare
description: >
  A Temporal.TimeZone instance passed to compare() does not have its
  'timeZone' property observably checked
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
Object.defineProperty(timeZone, "timeZone", {
  get() {
    throw new Test262Error("timeZone.timeZone should not be accessed");
  },
});

const arg1 = { year: 2020, month: 5, day: 2, timeZone };
Temporal.ZonedDateTime.compare(arg1, arg1);

const arg2 = { year: 2020, month: 5, day: 2, timeZone: { timeZone } };
Temporal.ZonedDateTime.compare(arg2, arg2);
