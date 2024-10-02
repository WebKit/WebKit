// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
description: era and eraYear are ignored (for calendars not using eras)
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const result = Temporal.PlainDate.from({
  era: "foobar",
  eraYear: 1,
  year: 1970,
  monthCode: "M01",
  day: 1,
  calendar: "iso8601",
});
TemporalHelpers.assertPlainDate(result, 1970, 1, "M01", 1,
  "era and eraYear are ignored for calendar not using eras (iso8601)");

assert.throws(TypeError, () => Temporal.PlainDate.from({
  era: "foobar",
  eraYear: 1,
  monthCode: "M01",
  day: 1,
  calendar: "iso8601",
}), "era and eraYear cannot replace year for calendar not using eras (iso8601)");

const resultHebrew = Temporal.PlainDate.from({
  era: "foobar",
  eraYear: 1,
  year: 5780,
  monthCode: "M01",
  day: 1,
  calendar: "hebrew",
});
TemporalHelpers.assertPlainDate(resultHebrew, 5780, 1, "M01", 1,
  "era and eraYear are ignored for calendar not using eras (Hebrew)");
assert.sameValue(resultHebrew.calendarId, "hebrew");

assert.throws(TypeError, () => Temporal.PlainDate.from({
  era: "foobar",
  eraYear: 1,
  monthCode: "M01",
  day: 1,
  calendar: "hebrew",
}), "era and eraYear cannot replace year for calendar not using eras (Hebrew)");
