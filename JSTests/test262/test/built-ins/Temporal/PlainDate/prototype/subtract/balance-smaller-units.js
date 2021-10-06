// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Durations with units smaller than days are balanced before adding, in the calendar
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];

class DateAddCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }

  dateAdd(date, duration, options) {
    actual.push(duration);
    return super.dateAdd(date, duration, options);
  }
}

const calendar = new DateAddCalendar();
const date = new Temporal.PlainDate(2000, 5, 2, calendar);
const duration = new Temporal.Duration(0, 0, 0, 1, 24, 1440, 86400, 86400_000, 86400_000_000, 86400_000_000_000);

const result = date.subtract(duration);
TemporalHelpers.assertPlainDate(result, 2000, 4, "M04", 25, "units smaller than days are balanced");

assert.sameValue(actual.length, 1, "calendar.dateAdd called exactly once");
TemporalHelpers.assertDuration(actual[0], 0, 0, 0, -1, -24, -1440, -86400, -86400_000, -86400_000_000, -86400_000_000_000, "the duration is negated but not balanced before passing to the calendar");
assert.notSameValue(actual[0], duration, "the duration is not passed directly to the calendar");

const resultString = date.subtract("P1DT24H1440M86400S");
TemporalHelpers.assertPlainDate(resultString, 2000, 4, "M04", 28, "units smaller than days are balanced");

assert.sameValue(actual.length, 2, "calendar.dateAdd called exactly once");
TemporalHelpers.assertDuration(actual[1], 0, 0, 0, -1, -24, -1440, -86400, 0, 0, 0, "the duration is negated but not balanced before passing to the calendar");

const resultPropBag = date.subtract({ days: 1, hours: 24, minutes: 1440, seconds: 86400, milliseconds: 86400_000, microseconds: 86400_000_000, nanoseconds: 86400_000_000_000 });
TemporalHelpers.assertPlainDate(resultPropBag, 2000, 4, "M04", 25, "units smaller than days are balanced");

assert.sameValue(actual.length, 3, "calendar.dateAdd called exactly once");
TemporalHelpers.assertDuration(actual[2], 0, 0, 0, -1, -24, -1440, -86400, -86400_000, -86400_000_000, -86400_000_000_000, "the duration is not balanced before passing to the calendar");

const negativeDuration = new Temporal.Duration(0, 0, 0, -1, -24, -1440, -86400, -86400_000, -86400_000_000, -86400_000_000_000);
const resultNegative = date.subtract(negativeDuration);
TemporalHelpers.assertPlainDate(resultNegative, 2000, 5, "M05", 9, "units smaller than days are balanced");

assert.sameValue(actual.length, 4, "calendar.dateAdd called exactly once");
TemporalHelpers.assertDuration(actual[3], 0, 0, 0, 1, 24, 1440, 86400, 86400_000, 86400_000_000, 86400_000_000_000, "the duration is negated but not balanced before passing to the calendar");
assert.notSameValue(actual[3], negativeDuration, "the duration is not passed directly to the calendar");
