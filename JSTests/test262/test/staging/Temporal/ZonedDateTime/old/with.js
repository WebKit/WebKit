// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.with()
features: [Temporal]
---*/

var zdt = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789).toZonedDateTime("UTC");

// zdt.with({ year: 2019 } works
assert.sameValue(`${ zdt.with({ year: 2019 }) }`, "2019-11-18T15:23:30.123456789+00:00[UTC]");

// zdt.with({ month: 5 } works
assert.sameValue(`${ zdt.with({ month: 5 }) }`, "1976-05-18T15:23:30.123456789+00:00[UTC]");

// zdt.with({ monthCode: "M05" }) works
assert.sameValue(`${ zdt.with({ monthCode: "M05" }) }`, "1976-05-18T15:23:30.123456789+00:00[UTC]");

// month and monthCode must agree
assert.throws(RangeError, () => zdt.with({
  month: 5,
  monthCode: "M06"
}));

// zdt.with({ day: 5 } works
assert.sameValue(`${ zdt.with({ day: 5 }) }`, "1976-11-05T15:23:30.123456789+00:00[UTC]");

// zdt.with({ hour: 5 } works
assert.sameValue(`${ zdt.with({ hour: 5 }) }`, "1976-11-18T05:23:30.123456789+00:00[UTC]");

// zdt.with({ minute: 5 } works
assert.sameValue(`${ zdt.with({ minute: 5 }) }`, "1976-11-18T15:05:30.123456789+00:00[UTC]");

// zdt.with({ second: 5 } works
assert.sameValue(`${ zdt.with({ second: 5 }) }`, "1976-11-18T15:23:05.123456789+00:00[UTC]");

// zdt.with({ millisecond: 5 } works
assert.sameValue(`${ zdt.with({ millisecond: 5 }) }`, "1976-11-18T15:23:30.005456789+00:00[UTC]");

// zdt.with({ microsecond: 5 } works
assert.sameValue(`${ zdt.with({ microsecond: 5 }) }`, "1976-11-18T15:23:30.123005789+00:00[UTC]");

// zdt.with({ nanosecond: 5 } works
assert.sameValue(`${ zdt.with({ nanosecond: 5 }) }`, "1976-11-18T15:23:30.123456005+00:00[UTC]");

// zdt.with({ month: 5, second: 15 } works
assert.sameValue(`${ zdt.with({
  month: 5,
  second: 15
}) }`, "1976-05-18T15:23:15.123456789+00:00[UTC]");

// Overflow options
// constrain
var overflow = "constrain";
assert.sameValue(`${ zdt.with({ month: 29 }, { overflow }) }`, "1976-12-18T15:23:30.123456789+00:00[UTC]");
assert.sameValue(`${ zdt.with({ day: 31 }, { overflow }) }`, "1976-11-30T15:23:30.123456789+00:00[UTC]");
assert.sameValue(`${ zdt.with({ hour: 29 }, { overflow }) }`, "1976-11-18T23:23:30.123456789+00:00[UTC]");
assert.sameValue(`${ zdt.with({ nanosecond: 9000 }, { overflow }) }`, "1976-11-18T15:23:30.123456999+00:00[UTC]");

// reject
var overflow = "reject";
assert.throws(RangeError, () => zdt.with({ month: 29 }, { overflow }));
assert.throws(RangeError, () => zdt.with({ day: 31 }, { overflow }));
assert.throws(RangeError, () => zdt.with({ hour: 29 }, { overflow }));
assert.throws(RangeError, () => zdt.with({ nanosecond: 9000 }, { overflow }));

// invalid disambiguation
[
  "",
  "EARLIER",
  "balance"
].forEach(disambiguation => assert.throws(RangeError, () => zdt.with({ day: 5 }, { disambiguation })));

// invalid offset
[
  "",
  "PREFER",
  "balance"
].forEach(offset => assert.throws(RangeError, () => zdt.with({ day: 5 }, { offset })));

// object must contain at least one correctly-spelled property
assert.throws(TypeError, () => zdt.with({}));
assert.throws(TypeError, () => zdt.with({ months: 12 }));

// incorrectly-spelled properties are ignored
assert.sameValue(`${ zdt.with({
  month: 12,
  days: 15
}) }`, "1976-12-18T15:23:30.123456789+00:00[UTC]");

// throws if timeZone is included
assert.throws(TypeError, () => zdt.with({
  month: 2,
  timeZone: "UTC"
}));

// throws if given a Temporal object with a time zone
assert.throws(TypeError, () => zdt.with(zdt));

// throws if calendarName is included
assert.throws(TypeError, () => zdt.with({
  month: 2,
  calendar: "iso8601"
}));

// throws if given a Temporal object with a calendar
assert.throws(TypeError, () => zdt.with(Temporal.PlainDateTime.from("1976-11-18T12:00")));
assert.throws(TypeError, () => zdt.with(Temporal.PlainDate.from("1976-11-18")));
assert.throws(TypeError, () => zdt.with(Temporal.PlainTime.from("12:00")));
assert.throws(TypeError, () => zdt.with(Temporal.PlainYearMonth.from("1976-11")));
assert.throws(TypeError, () => zdt.with(Temporal.PlainMonthDay.from("11-18")));

// throws if given a string
assert.throws(TypeError, () => zdt.with("1976-11-18T12:00+00:00[UTC]"));
assert.throws(TypeError, () => zdt.with("1976-11-18"));
assert.throws(TypeError, () => zdt.with("12:00"));
assert.throws(TypeError, () => zdt.with("invalid"));

