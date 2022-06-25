// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.until
description: Fast path for converting Temporal.PlainDate to Temporal.PlainDateTime by reading internal slots
info: |
    sec-temporal.timezone.prototype.getpossibleinstantsfor step 3:
      3. Set _dateTime_ to ? ToTemporalDateTime(_dateTime_).
    sec-temporal-totemporaldatetime step 2.b:
      b. If _item_ has an [[InitializedTemporalDate]] internal slot, then
        i. Return ? CreateTemporalDateTime(_item_.[[ISOYear]], _item_.[[ISOMonth]], _item_.[[ISODay]], 0, 0, 0, 0, 0, 0, _item_.[[Calendar]]).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

TemporalHelpers.checkToTemporalPlainDateTimeFastPath((date) => {
  const timezone = new Temporal.TimeZone("UTC");
  const result = timezone.getPossibleInstantsFor(date);
  assert.sameValue(result.length, 1, "one possible instant");
  assert.sameValue(result[0].epochNanoseconds, 957_225_600_000_000_000n, "epochNanoseconds result");
});
