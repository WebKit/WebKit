// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.toplainmonthday
description: Throw when the returned value is not a PlainMonthDay.
features: [Temporal]
---*/

class CustomCalendar extends Temporal.Calendar {
  constructor(value) {
    super("iso8601");
    this.value = value;
  }
  monthDayFromFields() {
    return this.value;
  }
}

const tests = [
  [undefined],
  [null, "null"],
  [true],
  ["2000-05"],
  [Symbol()],
  [200005],
  [200005n],
  [{}, "plain object"],
  [() => {}, "lambda"],
  [Temporal.PlainMonthDay, "Temporal.PlainMonthDay"],
  [Temporal.PlainMonthDay.prototype, "Temporal.PlainMonthDay.prototype"],
];
for (const [test, description = typeof test] of tests) {
  const plainDate = new Temporal.PlainDate(2000, 5, 2, new CustomCalendar(test));
  assert.throws(TypeError, () => plainDate.toPlainMonthDay(), `Expected error with ${description}`);
}
