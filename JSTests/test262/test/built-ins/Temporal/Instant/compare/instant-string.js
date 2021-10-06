// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.compare
description: Conversion of ISO date-time strings to Temporal.Instant instances
features: [Temporal]
---*/

const epoch = new Temporal.Instant(0n);
const hourBefore = new Temporal.Instant(-3600_000_000_000n);

let str = "1970-01-01T00:00";
assert.throws(RangeError, () => Temporal.Instant.compare(str, epoch), "bare date-time string is not an instant (first argument)");
assert.throws(RangeError, () => Temporal.Instant.compare(epoch, str), "bare date-time string is not an instant (second argument)");
str = "1970-01-01T00:00[America/Vancouver]";
assert.throws(RangeError, () => Temporal.Instant.compare(str, epoch), "date-time + IANA annotation is not an instant (first argument)");
assert.throws(RangeError, () => Temporal.Instant.compare(epoch, str), "date-time + IANA annotation is not an instant (second argument)");

str = "1970-01-01T00:00Z";
assert.sameValue(Temporal.Instant.compare(str, epoch), 0, "date-time + Z preserves exact time (first argument)");
assert.sameValue(Temporal.Instant.compare(epoch, str), 0, "date-time + Z preserves exact time (second argument)");

str = "1970-01-01T00:00+01:00";
assert.sameValue(Temporal.Instant.compare(str, hourBefore), 0, "date-time + offset preserves exact time with offset (first argument)");
assert.sameValue(Temporal.Instant.compare(hourBefore, str), 0, "date-time + offset preserves exact time with offset (second argument)");

str = "1970-01-01T00:00Z[America/Vancouver]";
assert.sameValue(Temporal.Instant.compare(str, epoch), 0, "date-time + Z + IANA annotation ignores the IANA annotation (first argument)");
assert.sameValue(Temporal.Instant.compare(epoch, str), 0, "date-time + Z + IANA annotation ignores the IANA annotation (second argument)");

str = "1970-01-01T00:00+01:00[America/Vancouver]";
assert.sameValue(Temporal.Instant.compare(str, hourBefore), 0, "date-time + offset + IANA annotation ignores the IANA annotation (first argument)");
assert.sameValue(Temporal.Instant.compare(hourBefore, str), 0, "date-time + offset + IANA annotation ignores the IANA annotation (second argument)");
