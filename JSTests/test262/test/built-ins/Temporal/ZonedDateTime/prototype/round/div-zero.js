// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.round
description: RangeError thrown if the calculated day length is zero
features: [Temporal]
---*/

class Calendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  dateAdd(d) {
    return d;
  }
}

const zdt = new Temporal.ZonedDateTime(0n, "UTC", new Calendar());

const units = ["hour", "minute", "second", "millisecond", "microsecond", "nanosecond"];
for (const smallestUnit of units) {
  assert.throws(RangeError, () => zdt.round({ smallestUnit, roundingIncrement: 2 }));
}
