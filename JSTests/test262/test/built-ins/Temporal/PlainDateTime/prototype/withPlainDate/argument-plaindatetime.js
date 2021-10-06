// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.withplaindate
description: Fast path for converting Temporal.PlainDateTime to Temporal.PlainDate by reading internal slots
info: |
    sec-temporal.plaindatetime.prototype.withplaindate step 3:
      3. Let _plainDate_ be ? ToTemporalDate(_plainDateLike_).
    sec-temporal-totemporaldate step 2.b:
      b. If _item_ has an [[InitializedTemporalDateTime]] internal slot, then
        i. Return ! CreateTemporalDate(_item_.[[ISOYear]], _item_.[[ISOMonth]], _item_.[[ISODay]], _item_.[[Calendar]]).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

TemporalHelpers.checkPlainDateTimeConversionFastPath((datetime) => {
  const receiver = new Temporal.PlainDateTime(2001, 9, 9, 6, 54, 32, 123, 456, 789);
  const result = receiver.withPlainDate(datetime);
  TemporalHelpers.assertPlainDateTime(result, 2000, 5, "M05", 2, 6, 54, 32, 123, 456, 789);
});
