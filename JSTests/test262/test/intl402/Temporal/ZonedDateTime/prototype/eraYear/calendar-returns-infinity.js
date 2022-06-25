// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.erayear
description: Getter throws if the calendar returns ±∞ from its eraYear method
features: [Temporal]
---*/

class InfinityCalendar extends Temporal.Calendar {
  constructor(positive) {
    super("iso8601");
    this.positive = positive;
  }

  eraYear() {
    return this.positive ? Infinity : -Infinity;
  }
}

const pos = new InfinityCalendar(true);
const instance1 = new Temporal.ZonedDateTime(0n, "UTC", pos);
assert.throws(RangeError, () => instance1.eraYear);

const neg = new InfinityCalendar(false);
const instance2 = new Temporal.ZonedDateTime(0n, "UTC", neg);
assert.throws(RangeError, () => instance2.eraYear);
