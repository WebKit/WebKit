// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.withTimeZone()
features: [Temporal]
---*/

// keeps instant and calendar the same
var fakeGregorian = { toString() { return "gregory"; }};
var zdt = Temporal.ZonedDateTime.from("2019-11-18T15:23:30.123456789+01:00[+01:00]").withCalendar(fakeGregorian);
var zdt2 = zdt.withTimeZone("-08:00");
assert.sameValue(zdt.epochNanoseconds, zdt2.epochNanoseconds);
assert.sameValue(zdt2.calendar, fakeGregorian);
assert.sameValue(zdt2.timeZone.id, "-08:00");
assert.notSameValue(`${ zdt.toPlainDateTime() }`, `${ zdt2.toPlainDateTime() }`);
