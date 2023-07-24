// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: Addition of a negative duration to a PlainYearMonth is not influenced by the implementation of daysInMonth()
includes: [temporalHelpers.js]
features: [Temporal]
---*/

class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  daysInMonth(ym, ...args) {
    return 15;
  }
}

const customCalendar = new CustomCalendar();
const instance = new Temporal.PlainYearMonth(2023, 3, customCalendar);

TemporalHelpers.assertPlainYearMonth(instance.add({days: -30}), 2023, 3, 'M03', "Adding -30 days from calendar reimplementing daysinMonth()")
