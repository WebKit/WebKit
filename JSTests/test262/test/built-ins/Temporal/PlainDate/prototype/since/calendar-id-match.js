// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.since
description: Calculation is performed if calendars' toString results match
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

class Calendar1 extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  toString() {
    return "A";
  }
}
class Calendar2 extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  toString() {
    return "A";
  }
}

const plainDate1 = new Temporal.PlainDate(2000, 1, 1, new Calendar1());
const plainDate2 = new Temporal.PlainDate(2000, 1, 2, new Calendar2());
TemporalHelpers.assertDuration(plainDate2.since(plainDate1), 0, 0, 0, /* days = */ 1, 0, 0, 0, 0, 0, 0);
