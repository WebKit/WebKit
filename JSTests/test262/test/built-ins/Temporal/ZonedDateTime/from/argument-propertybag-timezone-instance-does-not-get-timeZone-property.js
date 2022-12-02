// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: >
  A Temporal.TimeZone instance passed to from() does not have its
  'timeZone' property observably checked
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
Object.defineProperty(timeZone, "timeZone", {
  get() {
    throw new Test262Error("timeZone.timeZone should not be accessed");
  },
});

Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, timeZone });
Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, timeZone: { timeZone } });
