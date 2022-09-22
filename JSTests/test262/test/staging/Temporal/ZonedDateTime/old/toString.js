// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.toString()
features: [Temporal]
---*/

var zdt1 = Temporal.ZonedDateTime.from("1976-11-18T15:23+01:00[Europe/Vienna]");

// shows only non-ISO calendar if calendarName = auto
assert.sameValue(zdt1.toString({ calendarName: "auto" }), "1976-11-18T15:23:00+01:00[Europe/Vienna]");
assert.sameValue(zdt1.withCalendar("gregory").toString({ calendarName: "auto" }), "1976-11-18T15:23:00+01:00[Europe/Vienna][u-ca=gregory]");

// shows ISO calendar if calendarName = always
assert.sameValue(zdt1.toString({ calendarName: "always" }), "1976-11-18T15:23:00+01:00[Europe/Vienna][u-ca=iso8601]");

// omits non-ISO calendar if calendarName = never
assert.sameValue(zdt1.withCalendar("gregory").toString({ calendarName: "never" }), "1976-11-18T15:23:00+01:00[Europe/Vienna]");

// throws on invalid calendar
[
  "ALWAYS",
  "sometimes",
  false,
].forEach(calendarName => {
  assert.throws(RangeError, () => zdt1.toString({ calendarName }));
});

// shows time zone if timeZoneName = auto
assert.sameValue(zdt1.toString({ timeZoneName: "auto" }), "1976-11-18T15:23:00+01:00[Europe/Vienna]");

// omits time zone if timeZoneName = never
assert.sameValue(zdt1.toString({ timeZoneName: "never" }), "1976-11-18T15:23:00+01:00");

// shows offset if offset = auto
assert.sameValue(zdt1.toString({ offset: "auto" }), "1976-11-18T15:23:00+01:00[Europe/Vienna]");

// omits offset if offset = never
assert.sameValue(zdt1.toString({ offset: "never" }), "1976-11-18T15:23:00[Europe/Vienna]");

// combinations of calendar, time zone, and offset
var zdt = zdt1.withCalendar("gregory");
assert.sameValue(zdt.toString({
  timeZoneName: "never",
  calendarName: "never"
}), "1976-11-18T15:23:00+01:00");
assert.sameValue(zdt.toString({
  offset: "never",
  calendarName: "never"
}), "1976-11-18T15:23:00[Europe/Vienna]");
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
var zdt5 = Temporal.ZonedDateTime.from("2018-11-03T23:59:59.999999999-03:00[America/Sao_Paulo]");
var roundedString = zdt5.toString({
  fractionalSecondDigits: 8,
  roundingMode: "halfExpand"
});
assert.sameValue(roundedString, "2018-11-04T01:00:00.00000000-02:00[America/Sao_Paulo]");
var zdt6 = Temporal.ZonedDateTime.from(roundedString);
assert.sameValue(zdt6.epochNanoseconds - zdt5.epochNanoseconds, 1n);
