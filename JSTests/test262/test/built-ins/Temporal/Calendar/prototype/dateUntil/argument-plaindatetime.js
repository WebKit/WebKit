// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Fast path for converting Temporal.PlainDateTime to Temporal.PlainDate by reading internal slots
info: |
    sec-temporal.calendar.prototype.dateuntil steps 4–5:
      4. Set _one_ to ? ToTemporalDate(_one_).
      5. Set _two_ to ? ToTemporalDate(_two_).
    sec-temporal-totemporaldate step 2.b:
      b. If _item_ has an [[InitializedTemporalDateTime]] internal slot, then
        i. Return ! CreateTemporalDate(_item_.[[ISOYear]], _item_.[[ISOMonth]], _item_.[[ISODay]], _item_.[[Calendar]]).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const date = new Temporal.PlainDate(2000, 5, 2);

TemporalHelpers.checkPlainDateTimeConversionFastPath((datetime) => {
  const calendar = new Temporal.Calendar("iso8601");
  const result = calendar.dateUntil(datetime, date);
  assert.sameValue(result.total({ unit: "nanoseconds" }), 0, "time part dropped");
});

TemporalHelpers.checkPlainDateTimeConversionFastPath((datetime) => {
  const calendar = new Temporal.Calendar("iso8601");
  const result = calendar.dateUntil(date, datetime);
  assert.sameValue(result.total({ unit: "nanoseconds" }), 0, "time part dropped");
});
