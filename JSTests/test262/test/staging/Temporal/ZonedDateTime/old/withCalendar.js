// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.withCalendar()
features: [Temporal]
---*/

var zdt = Temporal.ZonedDateTime.from("2019-11-18T15:23:30.123456789-08:00[-08:00]");

// zonedDateTime.withCalendar(japanese) works
var cal = { toString() { return "japanese"; }};
assert.sameValue(`${ zdt.withCalendar(cal) }`, "2019-11-18T15:23:30.123456789-08:00[-08:00][u-ca=japanese]");

// keeps instant and time zone the same
var zdt = Temporal.ZonedDateTime.from("2019-11-18T15:23:30.123456789+01:00[+01:00][u-ca=iso8601]");
var zdt2 = zdt.withCalendar(cal);
assert.sameValue(zdt.epochNanoseconds, zdt2.epochNanoseconds);
assert.sameValue(zdt2.calendar, cal);
assert.sameValue(zdt2.timeZone.id, "+01:00");
