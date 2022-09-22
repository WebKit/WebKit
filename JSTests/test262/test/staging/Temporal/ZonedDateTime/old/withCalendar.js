// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.withCalendar()
features: [Temporal]
---*/

var zdt = Temporal.ZonedDateTime.from("2019-11-18T15:23:30.123456789-08:00[America/Los_Angeles]");

// zonedDateTime.withCalendar(japanese) works
var cal = Temporal.Calendar.from("japanese");
assert.sameValue(`${ zdt.withCalendar(cal) }`, "2019-11-18T15:23:30.123456789-08:00[America/Los_Angeles][u-ca=japanese]");

// keeps instant and time zone the same
var zdt = Temporal.ZonedDateTime.from("2019-11-18T15:23:30.123456789+01:00[Europe/Madrid][u-ca=gregory]");
var zdt2 = zdt.withCalendar("japanese");
assert.sameValue(zdt.epochNanoseconds, zdt2.epochNanoseconds);
assert.sameValue(zdt2.calendar.id, "japanese");
assert.sameValue(zdt2.timeZone.id, "Europe/Madrid");
