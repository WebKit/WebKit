// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: property bags
includes: [temporalHelpers.js]
features: [Temporal]
---*/

var lagos = Temporal.TimeZone.from("+01:00");

// can be constructed with monthCode and without month
assert.sameValue(`${ Temporal.ZonedDateTime.from({
  year: 1976,
  monthCode: "M11",
  day: 18,
  timeZone: lagos
}) }`, "1976-11-18T00:00:00+01:00[+01:00]");

// can be constructed with month and without monthCode
assert.sameValue(`${ Temporal.ZonedDateTime.from({
  year: 1976,
  month: 11,
  day: 18,
  timeZone: lagos
}) }`, "1976-11-18T00:00:00+01:00[+01:00]");

// month and monthCode must agree
assert.throws(RangeError, () => Temporal.ZonedDateTime.from({
  year: 1976,
  month: 11,
  monthCode: "M12",
  day: 18,
  timeZone: lagos
}));

// Temporal.ZonedDateTime.from({}) throws
assert.throws(TypeError, () => Temporal.ZonedDateTime.from({}))

// Temporal.ZonedDateTime.from(required prop undefined) throws
assert.throws(TypeError, () => Temporal.ZonedDateTime.from({
  year: 1976,
  month: undefined,
  monthCode: undefined,
  day: 18,
  timeZone: lagos
}))

// options may be a function object
assert.sameValue(`${ Temporal.ZonedDateTime.from({
  year: 1976,
  month: 11,
  day: 18,
  timeZone: lagos
}, () => {
}) }`, "1976-11-18T00:00:00+01:00[+01:00]");

// object must contain at least the required correctly-spelled properties
assert.throws(TypeError, () => Temporal.ZonedDateTime.from({
  years: 1976,
  months: 11,
  days: 18,
  timeZone: lagos
}));

// incorrectly-spelled properties are ignored
assert.sameValue(`${ Temporal.ZonedDateTime.from({
  year: 1976,
  month: 11,
  day: 18,
  timeZone: lagos,
  hours: 12
}) }`, "1976-11-18T00:00:00+01:00[+01:00]");

// casts offset property
var zdt = Temporal.ZonedDateTime.from({
  year: 1976,
  month: 11,
  day: 18,
  offset: -1030,
  timeZone: Temporal.TimeZone.from("-10:30")
});
assert.sameValue(`${ zdt }`, "1976-11-18T00:00:00-10:30[-10:30]");

// overflow options
var bad = {
  year: 2019,
  month: 1,
  day: 32,
  timeZone: lagos
};
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(bad, { overflow: "reject" }));
assert.sameValue(`${ Temporal.ZonedDateTime.from(bad) }`, "2019-01-31T00:00:00+01:00[+01:00]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(bad, { overflow: "constrain" }) }`, "2019-01-31T00:00:00+01:00[+01:00]");

// Offset options

// { offset: 'reject' } throws if offset does not match offset time zone
var obj = {
  year: 2020,
  month: 3,
  day: 8,
  hour: 1,
  offset: "-04:00",
  timeZone: "-08:00"
};
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(obj));
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(obj, { offset: "reject" }));

// { offset: 'reject' } throws if offset does not match IANA time zone
var obj = {
  year: 2020,
  month: 3,
  day: 8,
  hour: 1,
  offset: "-04:00",
  timeZone: "UTC"
};
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(obj));
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(obj, { offset: "reject" }));

var cali = TemporalHelpers.springForwardFallBackTimeZone();
var date = {
year: 2000,
month: 10,
day: 29,
timeZone: cali
};
// { offset: 'prefer' } if offset matches time zone (first 1:30 when DST ends)
var obj = {
  ...date,
  hour: 1,
  minute: 30,
  offset: "-07:00"
};
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { offset: "prefer" }) }`, "2000-10-29T01:30:00-07:00[Custom/Spring_Fall]");

// { offset: 'prefer' } if offset matches time zone (second 1:30 when DST ends)
var obj = {
  ...date,
  hour: 1,
  minute: 30,
  offset: "-08:00"
};
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { offset: "prefer" }) }`, "2000-10-29T01:30:00-08:00[Custom/Spring_Fall]");

// { offset: 'prefer' } if offset does not match time zone"
var obj = {
  ...date,
  hour: 4,
  offset: "-07:00"
};
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { offset: "prefer" }) }`, "2000-10-29T04:00:00-08:00[Custom/Spring_Fall]");

// { offset: 'ignore' } uses time zone only
var obj = {
  ...date,
  hour: 4,
  offset: "-12:00"
};
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { offset: "ignore" }) }`, "2000-10-29T04:00:00-08:00[Custom/Spring_Fall]");

// { offset: 'use' } uses offset only
var obj = {
  ...date,
  hour: 4,
  offset: "-07:00"
};
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { offset: "use" }) }`, "2000-10-29T03:00:00-08:00[Custom/Spring_Fall]");

// Disambiguation options

// plain datetime with multiple instants - Fall DST
var obj = {
  year: 2000,
  month: 10,
  day: 29,
  hour: 1,
  minute: 45,
  timeZone: cali
};
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj) }`, "2000-10-29T01:45:00-07:00[Custom/Spring_Fall]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { disambiguation: "compatible" }) }`, "2000-10-29T01:45:00-07:00[Custom/Spring_Fall]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { disambiguation: "earlier" }) }`, "2000-10-29T01:45:00-07:00[Custom/Spring_Fall]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { disambiguation: "later" }) }`, "2000-10-29T01:45:00-08:00[Custom/Spring_Fall]");
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(obj, { disambiguation: "reject" }));

// plain datetime with multiple instants - Spring DST
var obj = {
  year: 2000,
  month: 4,
  day: 2,
  hour: 2,
  minute: 30,
  timeZone: cali
};
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj) }`, "2000-04-02T03:30:00-07:00[Custom/Spring_Fall]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { disambiguation: "compatible" }) }`, "2000-04-02T03:30:00-07:00[Custom/Spring_Fall]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { disambiguation: "earlier" }) }`, "2000-04-02T01:30:00-08:00[Custom/Spring_Fall]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { disambiguation: "later" }) }`, "2000-04-02T03:30:00-07:00[Custom/Spring_Fall]");
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(obj, { disambiguation: "reject" }));

// uses disambiguation if offset is ignored
var obj = {
  year: 2000,
  month: 4,
  day: 2,
  hour: 2,
  minute: 30,
  timeZone: cali
};
var offset = "ignore";
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { offset }) }`, "2000-04-02T03:30:00-07:00[Custom/Spring_Fall]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, {
  offset,
  disambiguation: "compatible"
}) }`, "2000-04-02T03:30:00-07:00[Custom/Spring_Fall]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, {
  offset,
  disambiguation: "earlier"
}) }`, "2000-04-02T01:30:00-08:00[Custom/Spring_Fall]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, {
  offset,
  disambiguation: "later"
}) }`, "2000-04-02T03:30:00-07:00[Custom/Spring_Fall]");
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(obj, { disambiguation: "reject" }));

// uses disambiguation if offset is wrong and option is prefer
var obj = {
  year: 2000,
  month: 4,
  day: 2,
  hour: 2,
  minute: 30,
  offset: "-23:59",
  timeZone: cali
};
var offset = "prefer";
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { offset }) }`, "2000-04-02T03:30:00-07:00[Custom/Spring_Fall]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, {
  offset,
  disambiguation: "compatible"
}) }`, "2000-04-02T03:30:00-07:00[Custom/Spring_Fall]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, {
  offset,
  disambiguation: "earlier"
}) }`, "2000-04-02T01:30:00-08:00[Custom/Spring_Fall]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, {
  offset,
  disambiguation: "later"
}) }`, "2000-04-02T03:30:00-07:00[Custom/Spring_Fall]");
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(obj, {
  offset,
  disambiguation: "reject"
}));

// throw when bad disambiguation
[
  "",
  "EARLIER",
  "balance",
  3,
  null
].forEach(disambiguation => {
  assert.throws(RangeError, () => Temporal.ZonedDateTime.from("2020-11-01T04:00[UTC]", { disambiguation }));
});

// sub-minute time zone offsets

// does not truncate offset property to minutes
var zdt = Temporal.ZonedDateTime.from({
  year: 1971,
  month: 1,
  day: 1,
  hour: 12,
  timeZone: "-00:44:30"
});
assert.sameValue(zdt.offset, "-00:44:30");
