// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: era and eraYear are ignored (for calendars not using eras)
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const result = Temporal.PlainYearMonth.from({
  era: "foobar",
  eraYear: 1,
  year: 1970,
  monthCode: "M01",
  calendar: "iso8601",
});
TemporalHelpers.assertPlainYearMonth(result, 1970, 1, "M01",
  "era and eraYear are ignored for calendar not using eras (iso8601)");

assert.throws(TypeError, () => Temporal.PlainYearMonth.from({
  era: "foobar",
  eraYear: 1,
  monthCode: "M01",
  calendar: "iso8601",
}), "era and eraYear cannot replace year for calendar not using eras (iso8601)");

const resultHebrew = Temporal.PlainYearMonth.from({
  era: "foobar",
  eraYear: 1,
  year: 5780,
  monthCode: "M01",
  calendar: "hebrew",
});
TemporalHelpers.assertPlainYearMonth(resultHebrew, 5780, 1, "M01",
  "era and eraYear are ignored for calendar not using eras (Hebrew)",
  undefined, undefined, 30);
assert.sameValue(resultHebrew.calendarId, "hebrew");

assert.throws(TypeError, () => Temporal.PlainYearMonth.from({
  era: "foobar",
  eraYear: 1,
  monthCode: "M01",
  day: 1,
  calendar: "hebrew",
}), "era and eraYear cannot replace year for calendar not using eras (Hebrew)");
