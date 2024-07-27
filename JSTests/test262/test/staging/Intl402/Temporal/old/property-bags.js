// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: property bags
features: [Temporal]
---*/

var date = {
  year: 2000,
  month: 10,
  day: 29,
  timeZone: "America/Vancouver"
};
// { offset: 'prefer' } if offset matches time zone (first 1:30 when DST ends)
var obj = {
  ...date,
  hour: 1,
  minute: 30,
  offset: "-07:00"
};
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { offset: "prefer" }) }`, "2000-10-29T01:30:00-07:00[America/Vancouver]");

// { offset: 'prefer' } if offset matches time zone (second 1:30 when DST ends)
var obj = {
  ...date,
  hour: 1,
  minute: 30,
  offset: "-08:00"
};
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { offset: "prefer" }) }`, "2000-10-29T01:30:00-08:00[America/Vancouver]");

// { offset: 'prefer' } if offset does not match time zone"
var obj = {
  ...date,
  hour: 4,
  offset: "-07:00"
};
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { offset: "prefer" }) }`, "2000-10-29T04:00:00-08:00[America/Vancouver]");

// { offset: 'ignore' } uses time zone only
var obj = {
  ...date,
  hour: 4,
  offset: "-12:00"
};
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { offset: "ignore" }) }`, "2000-10-29T04:00:00-08:00[America/Vancouver]");

// { offset: 'use' } uses offset only
var obj = {
  ...date,
  hour: 4,
  offset: "-07:00"
};
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { offset: "use" }) }`, "2000-10-29T03:00:00-08:00[America/Vancouver]");

// Disambiguation options

// plain datetime with multiple instants - Fall DST
var obj = {
  year: 2000,
  month: 10,
  day: 29,
  hour: 1,
  minute: 45,
  timeZone: "America/Vancouver"
};
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj) }`, "2000-10-29T01:45:00-07:00[America/Vancouver]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { disambiguation: "compatible" }) }`, "2000-10-29T01:45:00-07:00[America/Vancouver]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { disambiguation: "earlier" }) }`, "2000-10-29T01:45:00-07:00[America/Vancouver]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { disambiguation: "later" }) }`, "2000-10-29T01:45:00-08:00[America/Vancouver]");
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(obj, { disambiguation: "reject" }));

// plain datetime with multiple instants - Spring DST
var obj = {
  year: 2000,
  month: 4,
  day: 2,
  hour: 2,
  minute: 30,
  timeZone: "America/Vancouver"
};
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj) }`, "2000-04-02T03:30:00-07:00[America/Vancouver]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { disambiguation: "compatible" }) }`, "2000-04-02T03:30:00-07:00[America/Vancouver]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { disambiguation: "earlier" }) }`, "2000-04-02T01:30:00-08:00[America/Vancouver]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { disambiguation: "later" }) }`, "2000-04-02T03:30:00-07:00[America/Vancouver]");
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(obj, { disambiguation: "reject" }));

// uses disambiguation if offset is ignored
var obj = {
  year: 2000,
  month: 4,
  day: 2,
  hour: 2,
  minute: 30,
  timeZone: "America/Vancouver"
};
var offset = "ignore";
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { offset }) }`, "2000-04-02T03:30:00-07:00[America/Vancouver]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, {
  offset,
  disambiguation: "compatible"
}) }`, "2000-04-02T03:30:00-07:00[America/Vancouver]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, {
  offset,
  disambiguation: "earlier"
}) }`, "2000-04-02T01:30:00-08:00[America/Vancouver]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, {
  offset,
  disambiguation: "later"
}) }`, "2000-04-02T03:30:00-07:00[America/Vancouver]");
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(obj, { disambiguation: "reject" }));

// uses disambiguation if offset is wrong and option is prefer
var obj = {
  year: 2000,
  month: 4,
  day: 2,
  hour: 2,
  minute: 30,
  offset: "-23:59",
  timeZone: "America/Vancouver"
};
var offset = "prefer";
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, { offset }) }`, "2000-04-02T03:30:00-07:00[America/Vancouver]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, {
  offset,
  disambiguation: "compatible"
}) }`, "2000-04-02T03:30:00-07:00[America/Vancouver]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, {
  offset,
  disambiguation: "earlier"
}) }`, "2000-04-02T01:30:00-08:00[America/Vancouver]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(obj, {
  offset,
  disambiguation: "later"
}) }`, "2000-04-02T03:30:00-07:00[America/Vancouver]");
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(obj, {
  offset,
  disambiguation: "reject"
}));

// sub-minute time zone offsets

// does not truncate offset property to minutes
var zdt = Temporal.ZonedDateTime.from({
  year: 1971,
  month: 1,
  day: 1,
  hour: 12,
  timeZone: "Africa/Monrovia"
});
assert.sameValue(zdt.offset, "-00:44:30");
