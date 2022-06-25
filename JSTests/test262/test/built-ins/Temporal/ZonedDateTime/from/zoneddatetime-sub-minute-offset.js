// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: Fuzzy matching behaviour with UTC offsets in ISO 8601 strings and offset option
includes: [temporalHelpers.js]
features: [Temporal]
---*/

["use", "ignore", "prefer", "reject"].forEach((offset) => {
  const result = Temporal.ZonedDateTime.from("1970-01-01T12:00-00:44:30.123456789[-00:44:30.123456789]", { offset });
  assert.sameValue(result.epochNanoseconds, 45870_123_456_789n, `accepts the exact offset string (offset=${offset})`);
  assert.sameValue(result.offset, "-00:44:30.123456789", "offset property is correct");
});

assert.throws(RangeError, () => Temporal.ZonedDateTime.from("1970-01-01T00:00-00:44:30[-00:44:30.123456789]", { offset: "reject" }), "offset=reject does not accept any other rounding than minutes");

const str = "1970-01-01T12:00-00:45[-00:44:30.123456789]";

["ignore", "prefer", "reject"].forEach((offset) => {
  const result = Temporal.ZonedDateTime.from(str, { offset });
  assert.sameValue(result.epochNanoseconds, 45870_123_456_789n, `accepts the offset string rounded to minutes (offset=${offset})`);
  assert.sameValue(result.offset, "-00:44:30.123456789", "offset property is still the full precision");
  TemporalHelpers.assertPlainDateTime(result.toPlainDateTime(), 1970, 1, "M01", 1, 12, 0, 0, 0, 0, 0, "wall time is preserved");
});

const result = Temporal.ZonedDateTime.from(str, { offset: "use" });
assert.sameValue(result.epochNanoseconds, 45900_000_000_000n, "prioritizes the offset string with HH:MM precision when offset=use");
assert.sameValue(result.offset, "-00:44:30.123456789", "offset property is still the full precision");
TemporalHelpers.assertPlainDateTime(result.toPlainDateTime(), 1970, 1, "M01", 1, 12, 0, 29, 876, 543, 211, "wall time is shifted by the difference between exact and rounded offset");

const properties = { year: 1970, month: 1, day: 1, hour: 12, offset: "-00:45", timeZone: "-00:44:30.123456789" };

["ignore", "prefer"].forEach((offset) => {
  const result = Temporal.ZonedDateTime.from(properties, { offset });
  assert.sameValue(result.epochNanoseconds, 45870_123_456_789n, `no fuzzy matching is done on offset in property bag (offset=${offset})`);
});

const result2 = Temporal.ZonedDateTime.from(properties, { offset: "use" });
assert.sameValue(result2.epochNanoseconds, 45900_000_000_000n, "no fuzzy matching is done on offset in property bag (offset=use)");

assert.throws(RangeError, () => Temporal.ZonedDateTime.from(properties, { offset: "reject" }), "no fuzzy matching is done on offset in property bag (offset=reject)");
