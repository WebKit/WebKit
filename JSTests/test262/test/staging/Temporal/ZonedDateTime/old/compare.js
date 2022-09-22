// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.compare()
features: [Temporal]
---*/

var zdt1 = Temporal.ZonedDateTime.from("1976-11-18T15:23:30.123456789+01:00[Europe/Vienna]");
var zdt2 = Temporal.ZonedDateTime.from("2019-10-29T10:46:38.271986102+01:00[Europe/Vienna]");

// equal
assert.sameValue(Temporal.ZonedDateTime.compare(zdt1, zdt1), 0)

// smaller/larger
assert.sameValue(Temporal.ZonedDateTime.compare(zdt1, zdt2), -1)

// larger/smaller
assert.sameValue(Temporal.ZonedDateTime.compare(zdt2, zdt1), 1)

// casts first argument
assert.sameValue(Temporal.ZonedDateTime.compare({
  year: 1976,
  month: 11,
  day: 18,
  hour: 15,
  timeZone: "Europe/Vienna"
}, zdt2), -1);
assert.sameValue(Temporal.ZonedDateTime.compare("1976-11-18T15:23:30.123456789+01:00[Europe/Vienna]", zdt2), -1);

// casts second argument
assert.sameValue(Temporal.ZonedDateTime.compare(zdt1, {
  year: 2019,
  month: 10,
  day: 29,
  hour: 10,
  timeZone: "Europe/Vienna"
}), -1);
assert.sameValue(Temporal.ZonedDateTime.compare(zdt1, "2019-10-29T10:46:38.271986102+01:00[Europe/Vienna]"), -1);

// object must contain at least the required properties
assert.sameValue(Temporal.ZonedDateTime.compare({
  year: 1976,
  month: 11,
  day: 18,
  timeZone: "Europe/Vienna"
}, zdt2), -1);
assert.throws(TypeError, () => Temporal.ZonedDateTime.compare({
  month: 11,
  day: 18,
  timeZone: "Europe/Vienna"
}, zdt2));
assert.throws(TypeError, () => Temporal.ZonedDateTime.compare({
  year: 1976,
  day: 18,
  timeZone: "Europe/Vienna"
}, zdt2));
assert.throws(TypeError, () => Temporal.ZonedDateTime.compare({
  year: 1976,
  month: 11,
  timeZone: "Europe/Vienna"
}, zdt2));
assert.throws(TypeError, () => Temporal.ZonedDateTime.compare({
  year: 1976,
  month: 11,
  day: 18
}, zdt2));
assert.throws(TypeError, () => Temporal.ZonedDateTime.compare({
  years: 1976,
  months: 11,
  days: 19,
  hours: 15,
  timeZone: "Europe/Vienna"
}, zdt2));
assert.sameValue(Temporal.ZonedDateTime.compare(zdt1, {
  year: 2019,
  month: 10,
  day: 29,
  timeZone: "Europe/Vienna"
}), -1);
assert.throws(TypeError, () => Temporal.ZonedDateTime.compare(zdt1, {
  month: 10,
  day: 29,
  timeZone: "Europe/Vienna"
}));
assert.throws(TypeError, () => Temporal.ZonedDateTime.compare(zdt1, {
  year: 2019,
  day: 29,
  timeZone: "Europe/Vienna"
}));
assert.throws(TypeError, () => Temporal.ZonedDateTime.compare(zdt1, {
  year: 2019,
  month: 10,
  timeZone: "Europe/Vienna"
}));
assert.throws(TypeError, () => Temporal.ZonedDateTime.compare(zdt1, {
  year: 2019,
  month: 10,
  day: 29
}));
assert.throws(TypeError, () => Temporal.ZonedDateTime.compare(zdt1, {
  years: 2019,
  months: 10,
  days: 29,
  hours: 10,
  timeZone: "Europe/Vienna"
}));

// disregards time zone IDs if exact times are equal
assert.sameValue(Temporal.ZonedDateTime.compare(zdt1, zdt1.withTimeZone("Asia/Kolkata")), 0);

// disregards calendar IDs if exact times and time zones are equal
assert.sameValue(Temporal.ZonedDateTime.compare(zdt1, zdt1.withCalendar("japanese")), 0);

// compares exact time, not clock time
var clockBefore = Temporal.ZonedDateTime.from("1999-12-31T23:30-08:00[America/Vancouver]");
var clockAfter = Temporal.ZonedDateTime.from("2000-01-01T01:30-04:00[America/Halifax]");
assert.sameValue(Temporal.ZonedDateTime.compare(clockBefore, clockAfter), 1);
assert.sameValue(Temporal.PlainDateTime.compare(clockBefore.toPlainDateTime(), clockAfter.toPlainDateTime()), -1);
