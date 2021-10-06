// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaindate.prototype.year
description: Getter throws if the calendar returns ±∞ from its year method
features: [Temporal]
---*/

class InfinityCalendar extends Temporal.Calendar {
  constructor(positive) {
    super("iso8601");
    this.positive = positive;
  }

  year() {
    return this.positive ? Infinity : -Infinity;
  }
}

const pos = new InfinityCalendar(true);
const instance1 = new Temporal.PlainDate(2000, 5, 2, pos);
assert.throws(RangeError, () => instance1.year);

const neg = new InfinityCalendar(false);
const instance2 = new Temporal.PlainDate(2000, 5, 2, neg);
assert.throws(RangeError, () => instance2.year);
