// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Japanese eras
includes: [temporalHelpers.js]
features: [Temporal]
---*/


// Reiwa (2019-)
var date = Temporal.PlainDate.from({
  era: "reiwa",
  eraYear: 2,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "2020-01-01[u-ca=japanese]");

// Heisei (1989-2019)
var date = Temporal.PlainDate.from({
  era: "heisei",
  eraYear: 2,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "1990-01-01[u-ca=japanese]");

// Showa (1926-1989)
var date = Temporal.PlainDate.from({
  era: "showa",
  eraYear: 2,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "1927-01-01[u-ca=japanese]");

// Taisho (1912-1926)
var date = Temporal.PlainDate.from({
  era: "taisho",
  eraYear: 2,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "1913-01-01[u-ca=japanese]");

// Meiji (1868-1912)
var date = Temporal.PlainDate.from({
  era: "meiji",
  eraYear: 2,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "1869-01-01[u-ca=japanese]");

// Verify that CE and BCE eras (before Meiji) are recognized
date = Temporal.PlainDate.from({
  era: "ce",
  eraYear: 1000,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${date}`, "1000-01-01[u-ca=japanese]");
assert.sameValue(
  TemporalHelpers.canonicalizeCalendarEra(date.calendarId, date.era),
  TemporalHelpers.canonicalizeCalendarEra(date.calendarId, "ce"),
);
assert.sameValue(date.eraYear, 1000);

date = Temporal.PlainDate.from({
  era: "bce",
  eraYear: 1,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${date}`, "0000-01-01[u-ca=japanese]");
assert.sameValue(
  TemporalHelpers.canonicalizeCalendarEra(date.calendarId, date.era),
  TemporalHelpers.canonicalizeCalendarEra(date.calendarId, "bce"),
);
assert.sameValue(date.eraYear, 1);
