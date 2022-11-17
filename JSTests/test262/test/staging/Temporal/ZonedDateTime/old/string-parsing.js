// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: string parsing
features: [Temporal]
---*/

// any number of decimal places
assert.sameValue(`${ Temporal.ZonedDateTime.from("1976-11-18T15:23:30.1-08:00[-08:00]") }`, "1976-11-18T15:23:30.1-08:00[-08:00]");
assert.sameValue(`${ Temporal.ZonedDateTime.from("1976-11-18T15:23:30.12-08:00[-08:00]") }`, "1976-11-18T15:23:30.12-08:00[-08:00]");
assert.sameValue(`${ Temporal.ZonedDateTime.from("1976-11-18T15:23:30.123-08:00[-08:00]") }`, "1976-11-18T15:23:30.123-08:00[-08:00]");
assert.sameValue(`${ Temporal.ZonedDateTime.from("1976-11-18T15:23:30.1234-08:00[-08:00]") }`, "1976-11-18T15:23:30.1234-08:00[-08:00]");
assert.sameValue(`${ Temporal.ZonedDateTime.from("1976-11-18T15:23:30.12345-08:00[-08:00]") }`, "1976-11-18T15:23:30.12345-08:00[-08:00]");
assert.sameValue(`${ Temporal.ZonedDateTime.from("1976-11-18T15:23:30.123456-08:00[-08:00]") }`, "1976-11-18T15:23:30.123456-08:00[-08:00]");
assert.sameValue(`${ Temporal.ZonedDateTime.from("1976-11-18T15:23:30.1234567-08:00[-08:00]") }`, "1976-11-18T15:23:30.1234567-08:00[-08:00]");
assert.sameValue(`${ Temporal.ZonedDateTime.from("1976-11-18T15:23:30.12345678-08:00[-08:00]") }`, "1976-11-18T15:23:30.12345678-08:00[-08:00]");
assert.sameValue(`${ Temporal.ZonedDateTime.from("1976-11-18T15:23:30.123456789-08:00[-08:00]") }`, "1976-11-18T15:23:30.123456789-08:00[-08:00]");

// variant decimal separator
assert.sameValue(`${ Temporal.ZonedDateTime.from("1976-11-18T15:23:30,12-08:00[-08:00]") }`, "1976-11-18T15:23:30.12-08:00[-08:00]");

// variant minus sign
assert.sameValue(`${ Temporal.ZonedDateTime.from("1976-11-18T15:23:30.12\u221208:00[-08:00]") }`, "1976-11-18T15:23:30.12-08:00[-08:00]");
assert.sameValue(`${ Temporal.ZonedDateTime.from("\u2212009999-11-18T15:23:30.12+00:00[UTC]") }`, "-009999-11-18T15:23:30.12+00:00[UTC]");

// mixture of basic and extended format
[
  "1976-11-18T152330.1-08:00[-08:00]",
  "19761118T15:23:30.1-08:00[-08:00]",
  "1976-11-18T15:23:30.1-0800[-08:00]",
  "1976-11-18T152330.1-0800[-08:00]",
  "19761118T15:23:30.1-0800[-08:00]",
  "19761118T152330.1-08:00[-08:00]",
  "19761118T152330.1-0800[-08:00]",
  "+001976-11-18T152330.1-08:00[-08:00]",
  "+0019761118T15:23:30.1-08:00[-08:00]",
  "+001976-11-18T15:23:30.1-0800[-08:00]",
  "+001976-11-18T152330.1-0800[-08:00]",
  "+0019761118T15:23:30.1-0800[-08:00]",
  "+0019761118T152330.1-08:00[-08:00]",
  "+0019761118T152330.1-0800[-08:00]"
].forEach(input => assert.sameValue(`${ Temporal.ZonedDateTime.from(input) }`, "1976-11-18T15:23:30.1-08:00[-08:00]"));

// optional parts
assert.sameValue(`${ Temporal.ZonedDateTime.from("1976-11-18T15:23:30-08[-08:00]") }`, "1976-11-18T15:23:30-08:00[-08:00]");
assert.sameValue(`${ Temporal.ZonedDateTime.from("1976-11-18T15-08:00[-08:00]") }`, "1976-11-18T15:00:00-08:00[-08:00]");
assert.sameValue(`${ Temporal.ZonedDateTime.from("2020-01-01[+09:00]") }`, "2020-01-01T00:00:00+09:00[+09:00]");

// no junk at end of string
assert.throws(RangeError, () => Temporal.ZonedDateTime.from("1976-11-18T15:23:30.123456789-08:00[-08:00]junk"))

// constrain has no effect on invalid ISO string
assert.throws(RangeError, () => Temporal.ZonedDateTime.from("2020-13-34T24:60[-08:00]", { overflow: "constrain" }));

// { offset: 'reject' } throws if offset does not match IANA time zone
assert.throws(RangeError, () => Temporal.ZonedDateTime.from("2020-03-08T01:00-04:00[UTC]"));
assert.throws(RangeError, () => Temporal.ZonedDateTime.from("2020-03-08T01:00-04:00[UTC]", { offset: "reject" }));

// throw when bad disambiguation
[
  "",
  "EARLIER",
  "balance"
].forEach(disambiguation => {
  assert.throws(RangeError, () => Temporal.ZonedDateTime.from("2020-11-01T04:00[-08:00]", { disambiguation }));
});
