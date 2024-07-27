// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.toString()
features: [Temporal]
---*/

var zdt1 = Temporal.ZonedDateTime.from("1976-11-18T15:23+00:00[UTC]");

// combinations of calendar, time zone, and offset
var zdt = zdt1.withCalendar("gregory");
assert.sameValue(zdt.toString({
  timeZoneName: "never",
  calendarName: "never"
}), "1976-11-18T15:23:00+00:00");
assert.sameValue(zdt.toString({
  offset: "never",
  calendarName: "never"
}), "1976-11-18T15:23:00[UTC]");
assert.sameValue(zdt.toString({
  offset: "never",
  timeZoneName: "never"
}), "1976-11-18T15:23:00[u-ca=gregory]");
assert.sameValue(zdt.toString({
  offset: "never",
  timeZoneName: "never",
  calendarName: "never"
}), "1976-11-18T15:23:00");

// rounding up to a nonexistent wall-clock time
var zdt5 = Temporal.PlainDateTime.from("2000-04-02T01:59:59.999999999").toZonedDateTime("America/Vancouver");
var roundedString = zdt5.toString({
  fractionalSecondDigits: 8,
  roundingMode: "halfExpand"
});
assert.sameValue(roundedString, "2000-04-02T03:00:00.00000000-07:00[America/Vancouver]");
var zdt6 = Temporal.Instant.from(roundedString);
assert.sameValue(zdt6.epochNanoseconds - zdt5.epochNanoseconds, 1n);
