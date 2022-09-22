// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.withTimeZone()
features: [Temporal]
---*/

var tz = new Temporal.TimeZone("America/Los_Angeles");
var instant = Temporal.Instant.from("2019-11-18T15:23:30.123456789-08:00[America/Los_Angeles]");
var zdt = instant.toZonedDateTimeISO("UTC");

// keeps instant and calendar the same
var zdt = Temporal.ZonedDateTime.from("2019-11-18T15:23:30.123456789+01:00[Europe/Madrid][u-ca=gregory]");
var zdt2 = zdt.withTimeZone("America/Vancouver");
assert.sameValue(zdt.epochNanoseconds, zdt2.epochNanoseconds);
assert.sameValue(zdt2.calendar.id, "gregory");
assert.sameValue(zdt2.timeZone.id, "America/Vancouver");
assert.notSameValue(`${ zdt.toPlainDateTime() }`, `${ zdt2.toPlainDateTime() }`);
