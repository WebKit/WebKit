// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: String parsing cases that require a TZDB in the implementation
features: [Temporal]
---*/

// parses with an IANA zone but no offset (with disambiguation)
var zdt = Temporal.ZonedDateTime.from("2020-03-08T02:30[America/Los_Angeles]", { disambiguation: "earlier" });
assert.sameValue(zdt.toString(), "2020-03-08T01:30:00-08:00[America/Los_Angeles]");

// "Z" means preserve the exact time in the given IANA time zone
var zdt = Temporal.ZonedDateTime.from("2020-03-08T09:00:00Z[America/Los_Angeles]");
assert.sameValue(zdt.toString(), "2020-03-08T01:00:00-08:00[America/Los_Angeles]");

// Offset options

// { offset: 'prefer' } if offset matches time zone (first 1:30 when DST ends)
var zdt = Temporal.ZonedDateTime.from("2020-11-01T01:30-07:00[America/Los_Angeles]", { offset: "prefer" });
assert.sameValue(zdt.toString(), "2020-11-01T01:30:00-07:00[America/Los_Angeles]");

// { offset: 'prefer' } if offset matches time zone (second 1:30 when DST ends)
var zdt = Temporal.ZonedDateTime.from("2020-11-01T01:30-08:00[America/Los_Angeles]", { offset: "prefer" });
assert.sameValue(zdt.toString(), "2020-11-01T01:30:00-08:00[America/Los_Angeles]");

// { offset: 'prefer' } if offset does not match time zone
var zdt = Temporal.ZonedDateTime.from("2020-11-01T04:00-07:00[America/Los_Angeles]", { offset: "prefer" });
assert.sameValue(zdt.toString(), "2020-11-01T04:00:00-08:00[America/Los_Angeles]");

// { offset: 'ignore' } uses time zone only
var zdt = Temporal.ZonedDateTime.from("2020-11-01T04:00-12:00[America/Los_Angeles]", { offset: "ignore" });
assert.sameValue(zdt.toString(), "2020-11-01T04:00:00-08:00[America/Los_Angeles]");

// { offset: 'use' } uses offset only
var zdt = Temporal.ZonedDateTime.from("2020-11-01T04:00-07:00[America/Los_Angeles]", { offset: "use" });
assert.sameValue(zdt.toString(), "2020-11-01T03:00:00-08:00[America/Los_Angeles]");

// Disambiguation options

// plain datetime with multiple instants - Fall DST in Brazil
var str = "2019-02-16T23:45[America/Sao_Paulo]";
assert.sameValue(`${ Temporal.ZonedDateTime.from(str) }`, "2019-02-16T23:45:00-02:00[America/Sao_Paulo]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(str, { disambiguation: "compatible" }) }`, "2019-02-16T23:45:00-02:00[America/Sao_Paulo]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(str, { disambiguation: "earlier" }) }`, "2019-02-16T23:45:00-02:00[America/Sao_Paulo]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(str, { disambiguation: "later" }) }`, "2019-02-16T23:45:00-03:00[America/Sao_Paulo]");
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(str, { disambiguation: "reject" }));

// plain datetime with multiple instants - Spring DST in Los Angeles
var str = "2020-03-08T02:30[America/Los_Angeles]";
assert.sameValue(`${ Temporal.ZonedDateTime.from(str) }`, "2020-03-08T03:30:00-07:00[America/Los_Angeles]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(str, { disambiguation: "compatible" }) }`, "2020-03-08T03:30:00-07:00[America/Los_Angeles]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(str, { disambiguation: "earlier" }) }`, "2020-03-08T01:30:00-08:00[America/Los_Angeles]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(str, { disambiguation: "later" }) }`, "2020-03-08T03:30:00-07:00[America/Los_Angeles]");
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(str, { disambiguation: "reject" }));

// uses disambiguation if offset is ignored
var str = "2020-03-08T02:30[America/Los_Angeles]";
var offset = "ignore";
assert.sameValue(`${ Temporal.ZonedDateTime.from(str, { offset }) }`, "2020-03-08T03:30:00-07:00[America/Los_Angeles]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(str, {
  offset,
  disambiguation: "compatible"
}) }`, "2020-03-08T03:30:00-07:00[America/Los_Angeles]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(str, {
  offset,
  disambiguation: "earlier"
}) }`, "2020-03-08T01:30:00-08:00[America/Los_Angeles]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(str, {
  offset,
  disambiguation: "later"
}) }`, "2020-03-08T03:30:00-07:00[America/Los_Angeles]");
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(str, {
  offset,
  disambiguation: "reject"
}));

// uses disambiguation if offset is wrong and option is prefer
var str = "2020-03-08T02:30-23:59[America/Los_Angeles]";
var offset = "prefer";
assert.sameValue(`${ Temporal.ZonedDateTime.from(str, { offset }) }`, "2020-03-08T03:30:00-07:00[America/Los_Angeles]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(str, {
  offset,
  disambiguation: "compatible"
}) }`, "2020-03-08T03:30:00-07:00[America/Los_Angeles]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(str, {
  offset,
  disambiguation: "earlier"
}) }`, "2020-03-08T01:30:00-08:00[America/Los_Angeles]");
assert.sameValue(`${ Temporal.ZonedDateTime.from(str, {
  offset,
  disambiguation: "later"
}) }`, "2020-03-08T03:30:00-07:00[America/Los_Angeles]");
assert.throws(RangeError, () => Temporal.ZonedDateTime.from(str, {
  offset,
  disambiguation: "reject"
}));
