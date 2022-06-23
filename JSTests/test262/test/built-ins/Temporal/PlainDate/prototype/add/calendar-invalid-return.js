// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.add
description: Throw when the returned value from the calendar's dateAdd method is not a PlainDate.
features: [Temporal]
---*/

class CustomCalendar extends Temporal.Calendar {
  constructor(value) {
    super("iso8601");
    this.value = value;
  }
  dateAdd() {
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
  [Temporal.PlainDate, "Temporal.PlainDate"],
  [Temporal.PlainDate.prototype, "Temporal.PlainDate.prototype"],
];
for (const [test, description = typeof test] of tests) {
  const plainDate = new Temporal.PlainDate(2000, 5, 2, new CustomCalendar(test));
  assert.throws(TypeError, () => plainDate.add({ years: 1 }), `Expected error with ${description}`);
}
