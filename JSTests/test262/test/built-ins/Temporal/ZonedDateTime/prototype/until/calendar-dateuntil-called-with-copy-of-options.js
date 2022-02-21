// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.until
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
    return new Temporal.Duration(1);
  }
}
const calendar = new Calendar();
const earlier = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", calendar);
// exactly one year later; avoids NanosecondsToDays path
const later = new Temporal.ZonedDateTime(1_031_536_000_000_000_000n, "UTC", calendar);
earlier.until(later, originalOptions);
assert(called, "calendar.dateUntil must be called");
