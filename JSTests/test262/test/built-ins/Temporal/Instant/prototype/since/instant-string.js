// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.since
description: Conversion of ISO date-time strings to Temporal.Instant instances
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Instant(0n);

let str = "1970-01-01T00:00";
assert.throws(RangeError, () => instance.since(str), "bare date-time string is not an instant");
str = "1970-01-01T00:00[UTC]";
assert.throws(RangeError, () => instance.since(str), "date-time + IANA annotation is not an instant");

str = "1970-01-01T00:00Z";
const result1 = instance.since(str);
TemporalHelpers.assertDuration(result1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "date-time + Z preserves exact time");

str = "1970-01-01T00:00+01:00";
const result2 = instance.since(str);
TemporalHelpers.assertDuration(result2, 0, 0, 0, 0, 0, 0, 3600, 0, 0, 0, "date-time + offset preserves exact time with offset");

str = "1970-01-01T00:00Z[Etc/Ignored]";
const result3 = instance.since(str);
TemporalHelpers.assertDuration(result3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "date-time + Z + IANA annotation ignores the IANA annotation");

str = "1970-01-01T00:00+01:00[Etc/Ignored]";
const result4 = instance.since(str);
TemporalHelpers.assertDuration(result4, 0, 0, 0, 0, 0, 0, 3600, 0, 0, 0, "date-time + offset + IANA annotation ignores the IANA annotation");
