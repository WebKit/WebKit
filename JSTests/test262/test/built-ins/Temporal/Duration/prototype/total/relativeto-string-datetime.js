// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: >
    Conversion of ISO date-time strings as relativeTo option to
    Temporal.ZonedDateTime or Temporal.PlainDateTime instances
features: [Temporal]
---*/

const instance = new Temporal.Duration(1, 0, 0, 0, 24);

let relativeTo = "2019-11-01T00:00";
const result1 = instance.total({ unit: "days", relativeTo });
assert.sameValue(result1, 367, "bare date-time string is a plain relativeTo");

relativeTo = "2019-11-01T00:00-07:00";
const result2 = instance.total({ unit: "days", relativeTo });
assert.sameValue(result2, 367, "date-time + offset is a plain relativeTo");

relativeTo = "2019-11-01T00:00[-07:00]";
const result3 = instance.total({ unit: "days", relativeTo });
assert.sameValue(result3, 367, "date-time + IANA annotation is a zoned relativeTo");

relativeTo = "2019-11-01T00:00Z[-07:00]";
const result4 = instance.total({ unit: "days", relativeTo });
assert.sameValue(result4, 367, "date-time + Z + IANA annotation is a zoned relativeTo");

relativeTo = "2019-11-01T00:00+00:00[UTC]";
const result5 = instance.total({ unit: "days", relativeTo });
assert.sameValue(result5, 367, "date-time + offset + IANA annotation is a zoned relativeTo");

relativeTo = "2019-11-01T00:00Z";
assert.throws(RangeError, () => instance.total({ unit: "days", relativeTo }), "date-time + Z throws without an IANA annotation");
relativeTo = "2019-11-01T00:00+04:15[UTC]";
assert.throws(RangeError, () => instance.total({ unit: "days", relativeTo }), "date-time + offset + IANA annotation throws if wall time and exact time mismatch");
