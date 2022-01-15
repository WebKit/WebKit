// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.protoype.equals
description: test if the calendar is compared
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

assert.sameValue(date1.equals(date2), true, "same calendar id");
assert.sameValue(calendar1.calls, 1, "calendar1 toString() calls");
assert.sameValue(calendar2.calls, 1, "calendar2 toString() calls");
