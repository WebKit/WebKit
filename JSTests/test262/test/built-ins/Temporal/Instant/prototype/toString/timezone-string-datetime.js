// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tostring
description: Conversion of ISO date-time strings to Temporal.TimeZone instances
features: [Temporal]
---*/

const instance = new Temporal.Instant(0n);

let timeZone = "2021-08-19T17:30";
assert.throws(RangeError, () => instance.toString({ timeZone }), "bare date-time string is not a time zone");

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
    () => instance.toString({ timeZone }),
    `ISO string ${timeZone} with a sub-minute offset is not a valid time zone`
  );
});

timeZone = "2021-08-19T17:30Z";
const result1 = instance.toString({ timeZone });
assert.sameValue(result1.substr(-6), "+00:00", "date-time + Z is UTC time zone");

timeZone = "2021-08-19T17:30-07:00";
const result2 = instance.toString({ timeZone });
assert.sameValue(result2.substr(-6), "-07:00", "date-time + offset is the offset time zone");

timeZone = "2021-08-19T17:30[UTC]";
const result3 = instance.toString({ timeZone });
assert.sameValue(result3.substr(-6), "+00:00", "date-time + IANA annotation is the offset time zone");

timeZone = "2021-08-19T17:30Z[UTC]";
const result4 = instance.toString({ timeZone });
assert.sameValue(result4.substr(-6), "+00:00", "date-time + Z + IANA annotation is the offset time zone");

timeZone = "2021-08-19T17:30-07:00[UTC]";
const result5 = instance.toString({ timeZone });
assert.sameValue(result5.substr(-6), "+00:00", "date-time + offset + IANA annotation is the offset time zone");
