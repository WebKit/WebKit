// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: .withPlainDate manipulation
features: [Temporal]
---*/

var zdt = Temporal.ZonedDateTime.from("1995-12-07T03:24:30[America/Los_Angeles]");

// withPlainDate({ year: 2000, month: 6, day: 1 }) works
assert.sameValue(`${ zdt.withPlainDate({
  year: 2000,
  month: 6,
  day: 1
}) }`, "2000-06-01T03:24:30-07:00[America/Los_Angeles]");

// withPlainDate(plainDate) works
var date = Temporal.PlainDate.from("2020-01-23");
assert.sameValue(`${ zdt.withPlainDate(date) }`, "2020-01-23T03:24:30-08:00[America/Los_Angeles]");

// withPlainDate('2018-09-15') works
assert.sameValue(`${ zdt.withPlainDate("2018-09-15") }`, "2018-09-15T03:24:30-07:00[America/Los_Angeles]");

// result contains a non-ISO calendar if present in the input
assert.sameValue(`${ zdt.withCalendar("japanese").withPlainDate("2008-09-06") }`, "2008-09-06T03:24:30-07:00[America/Los_Angeles][u-ca=japanese]");

// calendar is unchanged if input has ISO calendar
assert.sameValue(`${ zdt.withPlainDate("2008-09-06[u-ca=japanese]") }`, "2008-09-06T03:24:30-07:00[America/Los_Angeles][u-ca=japanese]");

// throws if both `this` and `other` have a non-ISO calendar
assert.throws(RangeError, () => zdt.withCalendar("gregory").withPlainDate("2008-09-06-07:00[America/Los_Angeles][u-ca=japanese]"));

// object must contain at least one correctly-spelled property
assert.throws(TypeError, () => zdt.withPlainDate({}));
assert.throws(TypeError, () => zdt.withPlainDate({ months: 12 }));

// incorrectly-spelled properties are ignored
assert.sameValue(`${ zdt.withPlainDate({
  year: 2000,
  month: 6,
  day: 1,
  months: 123
}) }`, "2000-06-01T03:24:30-07:00[America/Los_Angeles]");
