// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
  A Temporal.TimeZone instance passed to round() does not have its
  'timeZone' property observably checked
features: [Temporal]
---*/

const instance = new Temporal.Duration(1);

const timeZone = new Temporal.TimeZone("UTC");
Object.defineProperty(timeZone, "timeZone", {
  get() {
    throw new Test262Error("timeZone.timeZone should not be accessed");
  },
});

instance.round({ largestUnit: "months", relativeTo: { year: 2000, month: 5, day: 2, timeZone } });
instance.round({ largestUnit: "months", relativeTo: { year: 2000, month: 5, day: 2, timeZone: { timeZone } } });
