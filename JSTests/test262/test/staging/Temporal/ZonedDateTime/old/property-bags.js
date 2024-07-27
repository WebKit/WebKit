// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: property bags
features: [Temporal]
---*/

// can be constructed with monthCode and without month
assert.sameValue(`${ Temporal.ZonedDateTime.from({
  year: 1976,
  monthCode: "M11",
  day: 18,
  timeZone: "+01:00"
}) }`, "1976-11-18T00:00:00+01:00[+01:00]");

// can be constructed with month and without monthCode
assert.sameValue(`${ Temporal.ZonedDateTime.from({
  year: 1976,
  month: 11,
  day: 18,
  timeZone: "+01:00"
}) }`, "1976-11-18T00:00:00+01:00[+01:00]");

// month and monthCode must agree
assert.throws(RangeError, () => Temporal.ZonedDateTime.from({
  year: 1976,
  month: 11,
  monthCode: "M12",
  day: 18,
  timeZone: "+01:00"
}));

// Temporal.ZonedDateTime.from({}) throws
assert.throws(TypeError, () => Temporal.ZonedDateTime.from({}))

// Temporal.ZonedDateTime.from(required prop undefined) throws
assert.throws(TypeError, () => Temporal.ZonedDateTime.from({
  year: 1976,
  month: undefined,
  monthCode: undefined,
  day: 18,
  timeZone: "+01:00"
}))

// options may be a function object
assert.sameValue(`${ Temporal.ZonedDateTime.from({
  year: 1976,
  month: 11,
  day: 18,
  timeZone: "+01:00"
}, () => {
}) }`, "1976-11-18T00:00:00+01:00[+01:00]");

// object must contain at least the required correctly-spelled properties
assert.throws(TypeError, () => Temporal.ZonedDateTime.from({
  years: 1976,
  months: 11,
  days: 18,
  timeZone: "+01:00"
}));

// incorrectly-spelled properties are ignored
assert.sameValue(`${ Temporal.ZonedDateTime.from({
  year: 1976,
  month: 11,
  day: 18,
  timeZone: "+01:00",
  hours: 12
}) }`, "1976-11-18T00:00:00+01:00[+01:00]");

// does not accept non-string offset property
[
  null,
  true,
  1000,
  1000n,
  Symbol(),
  {}
].forEach(offset => {
  assert.throws(
    typeof offset === "string" || (typeof offset === "object" && offset !== null) || typeof offset === "function"
      ? RangeError
      : TypeError,
    () => Temporal.ZonedDateTime.from({
      year: 1976,
      month: 11,
      day: 18,
      offset: offset,
      timeZone: "+10:00"
    })
  )
});


// overflow options
var bad = {
  year: 2019,
  month: 1,
  day: 32,
  timeZone: "+01:00"
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
