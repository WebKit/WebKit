// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: era and eraYear are ignored (for calendars not using eras)
features: [BigInt, Temporal]
---*/

const result = Temporal.ZonedDateTime.from({
  era: "foobar",
  eraYear: 1,
  year: 1970,
  monthCode: "M01",
  day: 1,
  timeZone: "UTC",
  calendar: "iso8601",
});
assert.sameValue(result.epochNanoseconds, 0n,
  "era and eraYear are ignored for calendar not using eras (iso8601)");

assert.throws(TypeError, () => Temporal.ZonedDateTime.from({
  era: "foobar",
  eraYear: 1,
  monthCode: "M01",
  day: 1,
  timeZone: "UTC",
  calendar: "iso8601",
}), "era and eraYear cannot replace year for calendar not using eras (iso8601)");

const resultHebrew = Temporal.ZonedDateTime.from({
  era: "foobar",
  eraYear: 1,
  year: 5730,
  monthCode: "M04",
  day: 23,
  timeZone: "UTC",
  calendar: "hebrew",
});
assert.sameValue(resultHebrew.epochNanoseconds, 0n,
  "era and eraYear are ignored for calendar not using eras (Hebrew)");
assert.sameValue(resultHebrew.calendarId, "hebrew");

assert.throws(TypeError, () => Temporal.ZonedDateTime.from({
  era: "foobar",
  eraYear: 1,
  monthCode: "M01",
  day: 1,
  timeZone: "UTC",
  calendar: "hebrew",
}), "era and eraYear cannot replace year for calendar not using eras (Hebrew)");
