// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaindatetime.prototype.month
description: Getter throws if the calendar returns ±∞ from its month method
features: [Temporal]
---*/

class InfinityCalendar extends Temporal.Calendar {
  constructor(positive) {
    super("iso8601");
    this.positive = positive;
  }

  month() {
    return this.positive ? Infinity : -Infinity;
  }
}

const pos = new InfinityCalendar(true);
const instance1 = new Temporal.PlainDateTime(2000, 5, 2, 15, 30, 45, 987, 654, 321, pos);
assert.throws(RangeError, () => instance1.month);

const neg = new InfinityCalendar(false);
const instance2 = new Temporal.PlainDateTime(2000, 5, 2, 15, 30, 45, 987, 654, 321, neg);
assert.throws(RangeError, () => instance2.month);
