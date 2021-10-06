// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.compare
description: Fast path for converting Temporal.PlainDateTime to Temporal.PlainDate by reading internal slots
info: |
    sec-temporal.plaindate.compare steps 1–2:
      1. Set _one_ to ? ToTemporalDate(_one_).
      2. Set _two_ to ? ToTemporalDate(_two_).
    sec-temporal-totemporaldate step 2.b:
      b. If _item_ has an [[InitializedTemporalDateTime]] internal slot, then
        i. Return ! CreateTemporalDate(_item_.[[ISOYear]], _item_.[[ISOMonth]], _item_.[[ISODay]], _item_.[[Calendar]]).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const date = new Temporal.PlainDate(2000, 5, 2);

TemporalHelpers.checkPlainDateTimeConversionFastPath((datetime) => {
  const result = Temporal.PlainDate.compare(datetime, date);
  assert.sameValue(result, 0, "comparison result");
});

TemporalHelpers.checkPlainDateTimeConversionFastPath((datetime) => {
  const result = Temporal.PlainDate.compare(date, datetime);
  assert.sameValue(result, 0, "comparison result");
});
