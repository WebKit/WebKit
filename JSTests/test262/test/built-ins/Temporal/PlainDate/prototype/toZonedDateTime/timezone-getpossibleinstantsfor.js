// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tozoneddatetime
description: Calendar of the receiver is used
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
class CustomTimeZone extends Temporal.TimeZone {
  constructor() {
    super("UTC");
  }
  getPossibleInstantsFor(plainDateTime) {
    assert.sameValue(plainDateTime.calendar, calendar);
    return [new Temporal.Instant(987654321_000_000_000n)];
  }
}
const timeZone = new CustomTimeZone();
const plainDate = new Temporal.PlainDate(2000, 5, 2, calendar);
const result = plainDate.toZonedDateTime({
  timeZone,
  plainTime: { hour: 12, minute: 30 },
});
assert.sameValue(result.epochNanoseconds, 987654321_000_000_000n);
assert.sameValue(result.timeZone, timeZone);
assert.sameValue(result.calendar, calendar);
