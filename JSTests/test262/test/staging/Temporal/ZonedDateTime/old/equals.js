// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.equals()
features: [Temporal]
---*/

var zdt = new Temporal.ZonedDateTime(0n, "-05:00", "iso8601");

// constructed from equivalent parameters are equal
var zdt2 = Temporal.ZonedDateTime.from({
  year: 1969,
  month: 12,
  day: 31,
  hour: 19,
  timeZone: "-05:00",
  calendar: "iso8601",
});
assert(zdt.equals(zdt2));
assert(zdt2.equals(zdt));

// different instant not equal
var zdt2 = new Temporal.ZonedDateTime(1n, "-05:00", "iso8601");
assert(!zdt.equals(zdt2));

// different time zone not equal
var zdt2 = new Temporal.ZonedDateTime(0n, "UTC", "iso8601");
assert(!zdt.equals(zdt2));

// casts its argument
var instance = new Temporal.ZonedDateTime(0n, "UTC", "iso8601");
assert(instance.equals("1970-01-01T00:00+00:00[UTC][u-ca=iso8601]"));
assert(instance.equals({
  year: 1970,
  month: 1,
  day: 1,
  timeZone: "UTC",
  calendar: "iso8601",
}));

// at least the required properties must be present
assert(!zdt.equals({
  year: 1969,
  month: 12,
  day: 31,
  timeZone: "-05:00"
}));
assert.throws(TypeError, () => zdt.equals({
  month: 12,
  day: 31,
  timeZone: "-05:00"
}));
assert.throws(TypeError, () => zdt.equals({
  year: 1969,
  day: 31,
  timeZone: "-05:00"
}));
assert.throws(TypeError, () => zdt.equals({
  year: 1969,
  month: 12,
  timeZone: "-05:00"
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
  timeZone: "-05:00",
  calendarName: "iso8601"
}));
