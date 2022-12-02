// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.add
description: >
    Conversion of ISO date-time strings as relativeTo option to
    Temporal.ZonedDateTime or Temporal.PlainDateTime instances
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Duration(1, 0, 0, 1);

let relativeTo = "2019-11-01T00:00";
const result1 = instance.add(new Temporal.Duration(0, 0, 0, 0, -24), { relativeTo });
TemporalHelpers.assertDuration(result1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, "bare date-time string is a plain relativeTo");

relativeTo = "2019-11-01T00:00-07:00";
const result2 = instance.add(new Temporal.Duration(0, 0, 0, 0, -24), { relativeTo });
TemporalHelpers.assertDuration(result2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, "date-time + offset is a plain relativeTo");

relativeTo = "2019-11-01T00:00[-07:00]";
const result3 = instance.add(new Temporal.Duration(0, 0, 0, 0, -24), { relativeTo });
TemporalHelpers.assertDuration(result3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, "date-time + IANA annotation is a zoned relativeTo");

relativeTo = "2019-11-01T00:00Z[-07:00]";
const result4 = instance.add(new Temporal.Duration(0, 0, 0, 0, -24), { relativeTo });
TemporalHelpers.assertDuration(result4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, "date-time + Z + IANA annotation is a zoned relativeTo");

relativeTo = "2019-11-01T00:00+00:00[UTC]";
const result5 = instance.add(new Temporal.Duration(0, 0, 0, 0, -24), { relativeTo });
TemporalHelpers.assertDuration(result5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, "date-time + offset + IANA annotation is a zoned relativeTo");

relativeTo = "2019-11-01T00:00Z";
assert.throws(RangeError, () => instance.add(new Temporal.Duration(0, 0, 0, 0, -24), { relativeTo }), "date-time + Z throws without an IANA annotation");
relativeTo = "2019-11-01T00:00+04:15[UTC]";
assert.throws(RangeError, () => instance.add(new Temporal.Duration(0, 0, 0, 0, -24), { relativeTo }), "date-time + offset + IANA annotation throws if wall time and exact time mismatch");
