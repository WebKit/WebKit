// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.equals
description: >
  Time zone's getPossibleInstantsFor is called with a PlainDateTime with the
  built-in ISO 8601 calendar
features: [Temporal]
info: |
  DisambiguatePossibleInstants:
  2. Let _n_ be _possibleInstants_'s length.
  ...
  5. Assert: _n_ = 0.
  ...
  19. If _disambiguation_ is *"earlier"*, then
    ...
    c. Let _earlierDateTime_ be ! CreateTemporalDateTime(..., *"iso8601"*).
    d. Set _possibleInstants_ to ? GetPossibleInstantsFor(_timeZone_, _earlierDateTime_).
    ...
  20. Assert: _disambiguation_ is *"compatible"* or *"later"*.
  ...
  23. Let _laterDateTime_ be ! CreateTemporalDateTime(..., *"iso8601"*).
  24. Set _possibleInstants_ to ? GetPossibleInstantsFor(_timeZone_, _laterDateTime_).
---*/

class SkippedDateTime extends Temporal.TimeZone {
  constructor() {
    super("UTC");
    this.calls = 0;
  }

  getPossibleInstantsFor(dateTime) {
    // Calls occur in pairs. For the first one return no possible instants so
    // that DisambiguatePossibleInstants will call it again
    if (this.calls++ % 2 == 0) {
      return [];
    }

    assert.sameValue(
      dateTime.getISOFields().calendar,
      "iso8601",
      "getPossibleInstantsFor called with dateTime with built-in ISO 8601 calendar"
    );
    return super.getPossibleInstantsFor(dateTime);
  }
}

const nonBuiltinISOCalendar = new Temporal.Calendar("iso8601");
const timeZone = new SkippedDateTime();
const arg = { year: 2000, month: 5, day: 2, timeZone, calendar: nonBuiltinISOCalendar };

const instance = new Temporal.ZonedDateTime(0n, timeZone);
instance.equals(arg);

assert.sameValue(timeZone.calls, 2, "getPossibleInstantsFor should have been called 2 times");
