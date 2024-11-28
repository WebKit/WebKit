// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: era and eraYear are ignored (for calendars not using eras)
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const result = Temporal.PlainMonthDay.from({
  era: "foobar",
  eraYear: 1,
  monthCode: "M01",
  day: 1,
  calendar: "iso8601",
});
TemporalHelpers.assertPlainMonthDay(result, "M01", 1,
  "era and eraYear are ignored for calendar not using eras (iso8601)");

const resultHebrew = Temporal.PlainMonthDay.from({
  era: "foobar",
  eraYear: 1,
  monthCode: "M01",
  day: 1,
  calendar: "hebrew",
});
TemporalHelpers.assertPlainMonthDay(resultHebrew, "M01", 1,
  "era and eraYear are ignored for calendar not using eras (Hebrew)");
assert.sameValue(resultHebrew.calendarId, "hebrew");
