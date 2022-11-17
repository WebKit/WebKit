// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: .withPlainDate manipulation
includes: [temporalHelpers.js]
features: [Temporal]
---*/

var dst = TemporalHelpers.springForwardFallBackTimeZone();
var zdt = Temporal.PlainDateTime.from("1995-12-07T03:24:30").toZonedDateTime(dst);

// withPlainDate({ year: 2000, month: 6, day: 1 }) works
// and keeps wall time constant despite the UTC offset change
assert.sameValue(`${ zdt.withPlainDate({
  year: 2000,
  month: 6,
  day: 1
}) }`, "2000-06-01T03:24:30-07:00[Custom/Spring_Fall]");

// withPlainDate(plainDate) works
var date = Temporal.PlainDate.from("2020-01-23");
assert.sameValue(`${ zdt.withPlainDate(date) }`, "2020-01-23T03:24:30-08:00[Custom/Spring_Fall]");

// withPlainDate('2018-09-15') works
assert.sameValue(`${ zdt.withPlainDate("2018-09-15") }`, "2018-09-15T03:24:30-08:00[Custom/Spring_Fall]");

// result contains a non-ISO calendar if present in the input
var fakeJapanese = { toString() { return "japanese"; }};
assert.sameValue(`${ zdt.withCalendar(fakeJapanese).withPlainDate("2008-09-06") }`, "2008-09-06T03:24:30-08:00[Custom/Spring_Fall][u-ca=japanese]");

// calendar is unchanged if input has ISO calendar
var date = new Temporal.PlainDate(2008, 9, 6, fakeJapanese);
assert.sameValue(`${ zdt.withPlainDate(date) }`, "2008-09-06T03:24:30-08:00[Custom/Spring_Fall][u-ca=japanese]");

// throws if both `this` and `other` have a non-ISO calendar
var fakeGregorian = { toString() { return "gregory"; }};
assert.throws(RangeError, () => zdt.withCalendar(fakeGregorian).withPlainDate(date));

// object must contain at least one correctly-spelled property
assert.throws(TypeError, () => zdt.withPlainDate({}));
assert.throws(TypeError, () => zdt.withPlainDate({ months: 12 }));

// incorrectly-spelled properties are ignored
assert.sameValue(`${ zdt.withPlainDate({
  year: 2000,
  month: 6,
  day: 1,
  months: 123
}) }`, "2000-06-01T03:24:30-07:00[Custom/Spring_Fall]");
