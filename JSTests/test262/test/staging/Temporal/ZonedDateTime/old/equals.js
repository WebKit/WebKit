// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.equals()
features: [Temporal]
---*/

var tz = Temporal.TimeZone.from("America/New_York");
var cal = Temporal.Calendar.from("gregory");
var zdt = new Temporal.ZonedDateTime(0n, tz, cal);

// constructed from equivalent parameters are equal
var zdt2 = Temporal.ZonedDateTime.from("1969-12-31T19:00-05:00[America/New_York][u-ca=gregory]");
assert(zdt.equals(zdt2));
assert(zdt2.equals(zdt));

// different instant not equal
var zdt2 = new Temporal.ZonedDateTime(1n, tz, cal);
assert(!zdt.equals(zdt2));

// different time zone not equal
var zdt2 = new Temporal.ZonedDateTime(0n, "America/Chicago", cal);
assert(!zdt.equals(zdt2));

// different calendar not equal
var zdt2 = new Temporal.ZonedDateTime(0n, tz, "iso8601");
assert(!zdt.equals(zdt2));

// casts its argument
assert(zdt.equals("1969-12-31T19:00-05:00[America/New_York][u-ca=gregory]"));
assert(zdt.equals({
  year: 1969,
  month: 12,
  day: 31,
  hour: 19,
  timeZone: "America/New_York",
  calendar: "gregory"
}));

// at least the required properties must be present
assert(!zdt.equals({
  year: 1969,
  month: 12,
  day: 31,
  timeZone: "America/New_York"
}));
assert.throws(TypeError, () => zdt.equals({
  month: 12,
  day: 31,
  timeZone: "America/New_York"
}));
assert.throws(TypeError, () => zdt.equals({
  year: 1969,
  day: 31,
  timeZone: "America/New_York"
}));
assert.throws(TypeError, () => zdt.equals({
  year: 1969,
  month: 12,
  timeZone: "America/New_York"
}));
assert.throws(TypeError, () => zdt.equals({
  year: 1969,
  month: 12,
  day: 31
}));
assert.throws(TypeError, () => zdt.equals({
  years: 1969,
  months: 12,
  days: 31,
  timeZone: "America/New_York",
  calendarName: "gregory"
}));
