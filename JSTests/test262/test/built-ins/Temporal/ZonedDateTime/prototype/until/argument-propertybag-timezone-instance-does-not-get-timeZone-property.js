// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.until
description: >
  A Temporal.TimeZone instance passed to until() does not have its
  'timeZone' property observably checked
features: [Temporal]
---*/

const instance = new Temporal.ZonedDateTime(0n, new Temporal.TimeZone("UTC"));

const timeZone = new Temporal.TimeZone("UTC");
Object.defineProperty(timeZone, "timeZone", {
  get() {
    throw new Test262Error("timeZone.timeZone should not be accessed");
  },
});

instance.until({ year: 2020, month: 5, day: 2, timeZone });
instance.until({ year: 2020, month: 5, day: 2, timeZone: { timeZone } });
