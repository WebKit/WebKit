// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.until
description: The dateUntil() method on the calendar is called with a copy of the options bag
features: [Temporal]
---*/

const originalOptions = {
  largestUnit: "year",
  shouldBeCopied: {},
};
let called = false;

class Calendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }

  dateUntil(d1, d2, options) {
    called = true;
    assert.notSameValue(options, originalOptions, "options bag should be a copy");
    assert.sameValue(options.shouldBeCopied, originalOptions.shouldBeCopied, "options bag should be a shallow copy");
    return new Temporal.Duration();
  }
}
const calendar = new Calendar();
const earlier = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, calendar);
const later = new Temporal.PlainDateTime(2001, 6, 3, 13, 35, 57, 988, 655, 322, calendar);
earlier.until(later, originalOptions);
assert(called, "calendar.dateUntil must be called");
