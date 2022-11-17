// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.with()
includes: [temporalHelpers.js]
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

var dst = TemporalHelpers.springForwardFallBackTimeZone();
var dstStartDay = Temporal.PlainDateTime.from("2000-04-02T12:00:01").toZonedDateTime(dst);
var dstEndDay = Temporal.PlainDateTime.from("2000-10-29T12:00:01").toZonedDateTime(dst);
var oneThirty = {
hour: 1,
minute: 30
};
var twoThirty = {
hour: 2,
minute: 30
};

// Disambiguation options
var offset = "ignore";
// compatible, skipped wall time
assert.sameValue(`${ dstStartDay.with(twoThirty, {
  offset,
  disambiguation: "compatible"
}) }`, "2000-04-02T03:30:01-07:00[Custom/Spring_Fall]");

// earlier, skipped wall time
assert.sameValue(`${ dstStartDay.with(twoThirty, {
  offset,
  disambiguation: "earlier"
}) }`, "2000-04-02T01:30:01-08:00[Custom/Spring_Fall]");

// later, skipped wall time
assert.sameValue(`${ dstStartDay.with(twoThirty, {
  offset,
  disambiguation: "later"
}) }`, "2000-04-02T03:30:01-07:00[Custom/Spring_Fall]");

// compatible, repeated wall time
assert.sameValue(`${ dstEndDay.with(oneThirty, {
  offset,
  disambiguation: "compatible"
}) }`, "2000-10-29T01:30:01-07:00[Custom/Spring_Fall]");

// earlier, repeated wall time
assert.sameValue(`${ dstEndDay.with(oneThirty, {
  offset,
  disambiguation: "earlier"
}) }`, "2000-10-29T01:30:01-07:00[Custom/Spring_Fall]");

// later, repeated wall time
assert.sameValue(`${ dstEndDay.with(oneThirty, {
  offset,
  disambiguation: "later"
}) }`, "2000-10-29T01:30:01-08:00[Custom/Spring_Fall]");

// reject
assert.throws(RangeError, () => dstStartDay.with(twoThirty, {
  offset,
  disambiguation: "reject"
}));
assert.throws(RangeError, () => dstEndDay.with(oneThirty, {
  offset,
  disambiguation: "reject"
}));

// compatible is the default
assert.sameValue(`${ dstStartDay.with(twoThirty, { offset }) }`, `${ dstStartDay.with(twoThirty, {
  offset,
  disambiguation: "compatible"
}) }`);
assert.sameValue(`${ dstEndDay.with(twoThirty, { offset }) }`, `${ dstEndDay.with(twoThirty, {
  offset,
  disambiguation: "compatible"
}) }`);

// invalid disambiguation
[
  "",
  "EARLIER",
  "balance"
].forEach(disambiguation => assert.throws(RangeError, () => zdt.with({ day: 5 }, { disambiguation })));

// Offset options
var bogus = {
  ...twoThirty,
  offset: "+23:59"
};
// use, with bogus offset, changes to the exact time with the offset
var preserveExact = dstStartDay.with(bogus, { offset: "use" });
assert.sameValue(`${ preserveExact }`, "2000-03-31T18:31:01-08:00[Custom/Spring_Fall]");
assert.sameValue(preserveExact.epochNanoseconds, Temporal.Instant.from("2000-04-02T02:30:01+23:59").epochNanoseconds);

// ignore, with bogus offset, defers to disambiguation option
var offset = "ignore";
assert.sameValue(`${ dstStartDay.with(bogus, {
  offset,
  disambiguation: "earlier"
}) }`, "2000-04-02T01:30:01-08:00[Custom/Spring_Fall]");
assert.sameValue(`${ dstStartDay.with(bogus, {
  offset,
  disambiguation: "later"
}) }`, "2000-04-02T03:30:01-07:00[Custom/Spring_Fall]");

// prefer, with bogus offset, defers to disambiguation option
var offset = "prefer";
assert.sameValue(`${ dstStartDay.with(bogus, {
  offset,
  disambiguation: "earlier"
}) }`, "2000-04-02T01:30:01-08:00[Custom/Spring_Fall]");
assert.sameValue(`${ dstStartDay.with(bogus, {
  offset,
  disambiguation: "later"
}) }`, "2000-04-02T03:30:01-07:00[Custom/Spring_Fall]");

// reject, with bogus offset, throws
assert.throws(RangeError, () => dstStartDay.with({
  ...twoThirty,
  offset: "+23:59"
}, { offset: "reject" }));

var doubleTime = new Temporal.ZonedDateTime(972811801_000_000_000n, dst);
// use changes to the exact time with the offset
var preserveExact = doubleTime.with({ offset: "-07:00" }, { offset: "use" });
assert.sameValue(preserveExact.offset, "-07:00");
assert.sameValue(preserveExact.epochNanoseconds, Temporal.Instant.from("2000-10-29T01:30:01-07:00").epochNanoseconds);

// ignore defers to disambiguation option
var offset = "ignore";
assert.sameValue(doubleTime.with({ offset: "-07:00" }, {
  offset,
  disambiguation: "earlier"
}).offset, "-07:00");
assert.sameValue(doubleTime.with({ offset: "-07:00" }, {
  offset,
  disambiguation: "later"
}).offset, "-08:00");

// prefer adjusts offset of repeated clock time
assert.sameValue(doubleTime.with({ offset: "-07:00" }, { offset: "prefer" }).offset, "-07:00");

// reject adjusts offset of repeated clock time
assert.sameValue(doubleTime.with({ offset: "-07:00" }, { offset: "reject" }).offset, "-07:00");

// use does not cause the offset to change when adjusting repeated clock time
assert.sameValue(doubleTime.with({ minute: 31 }, { offset: "use" }).offset, "-08:00");

// ignore may cause the offset to change when adjusting repeated clock time
assert.sameValue(doubleTime.with({ minute: 31 }, { offset: "ignore" }).offset, "-07:00");

// prefer does not cause the offset to change when adjusting repeated clock time
assert.sameValue(doubleTime.with({ minute: 31 }, { offset: "prefer" }).offset, "-08:00");

// reject does not cause the offset to change when adjusting repeated clock time
assert.sameValue(doubleTime.with({ minute: 31 }, { offset: "reject" }).offset, "-08:00");

// prefer is the default
assert.sameValue(`${ dstStartDay.with(twoThirty) }`, `${ dstStartDay.with(twoThirty, { offset: "prefer" }) }`);
assert.sameValue(`${ dstEndDay.with(twoThirty) }`, `${ dstEndDay.with(twoThirty, { offset: "prefer" }) }`);
assert.sameValue(`${ doubleTime.with({ minute: 31 }) }`, `${ doubleTime.with({ minute: 31 }, { offset: "prefer" }) }`);

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
assert.throws(TypeError, () => zdt.with(dstStartDay));

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

