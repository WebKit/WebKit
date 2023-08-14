// Copyright (C) 2023 Justin Grant. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.equals
description: Conversion of ISO date-time strings to the argument of Temporal.TimeZone.prototype.equals
features: [Temporal]
---*/

let tzUTC = Temporal.TimeZone.from("UTC");
let arg = "2021-08-19T17:30";
assert.throws(RangeError, () => tzUTC.equals(arg), "bare date-time string is not a time zone");

[
  "2021-08-19T17:30-07:00:01",
  "2021-08-19T17:30-07:00:00",
  "2021-08-19T17:30-07:00:00.1",
  "2021-08-19T17:30-07:00:00.0",
  "2021-08-19T17:30-07:00:00.01",
  "2021-08-19T17:30-07:00:00.00",
  "2021-08-19T17:30-07:00:00.001",
  "2021-08-19T17:30-07:00:00.000",
  "2021-08-19T17:30-07:00:00.0001",
  "2021-08-19T17:30-07:00:00.0000",
  "2021-08-19T17:30-07:00:00.00001",
  "2021-08-19T17:30-07:00:00.00000",
  "2021-08-19T17:30-07:00:00.000001",
  "2021-08-19T17:30-07:00:00.000000",
  "2021-08-19T17:30-07:00:00.0000001",
  "2021-08-19T17:30-07:00:00.0000000",
  "2021-08-19T17:30-07:00:00.00000001",
  "2021-08-19T17:30-07:00:00.00000000",
  "2021-08-19T17:30-07:00:00.000000001",
  "2021-08-19T17:30-07:00:00.000000000",
].forEach((timeZone) => {
  assert.throws(
    RangeError,
    () => tzUTC.equals(timeZone),
    `ISO string ${timeZone} with a sub-minute offset is not a valid time zone`
  );
});

arg = "2021-08-19T17:30Z";
tzUTC = Temporal.TimeZone.from(arg);
assert.sameValue(tzUTC.equals(arg), true, "date-time + Z is UTC time zone");

arg = "2021-08-19T17:30-07:00";
tzUTC = Temporal.TimeZone.from(arg);
assert.sameValue(tzUTC.equals(arg), true, "date-time + offset is the offset time zone");

arg = "2021-08-19T17:30[UTC]";
tzUTC = Temporal.TimeZone.from(arg);
assert.sameValue(tzUTC.equals(arg), true, "date-time + IANA annotation is the IANA time zone");

arg = "2021-08-19T17:30Z[UTC]";
tzUTC = Temporal.TimeZone.from(arg);
assert.sameValue(tzUTC.equals(arg), true, "date-time + Z + IANA annotation is the IANA time zone");

arg = "2021-08-19T17:30-07:00[UTC]";
tzUTC = Temporal.TimeZone.from(arg);
assert.sameValue(tzUTC.equals(arg), true, "date-time + offset + IANA annotation is the IANA time zone");
