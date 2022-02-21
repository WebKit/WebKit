// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.compare
description: basic tests
features: [Temporal]
---*/

class CalendarTraceToString extends Temporal.Calendar {
  constructor(id) {
    super("iso8601");
    this.id_ = id;
    this.calls = 0;
  }
  toString() {
    ++this.calls;
    return this.id_;
  }
};

const calendar1 = new CalendarTraceToString("a");
const date1 = new Temporal.PlainDate(1914, 2, 23, calendar1);

const calendar2 = new CalendarTraceToString("a");
const date2 = new Temporal.PlainDate(1914, 2, 23, calendar2);

const calendar3 = new CalendarTraceToString("b");
const date3 = new Temporal.PlainDate(1914, 2, 23, calendar3);

assert.sameValue(Temporal.PlainDate.compare(date1, date1), 0, "same object");
assert.sameValue(Temporal.PlainDate.compare(date1, date2), 0, "same date");
assert.sameValue(Temporal.PlainDate.compare(date1, date3), 0, "same date, different calendar");

assert.sameValue(calendar1.calls, 0, "calendar1 toString() calls");
assert.sameValue(calendar2.calls, 0, "calendar2 toString() calls");
assert.sameValue(calendar3.calls, 0, "calendar3 toString() calls");
