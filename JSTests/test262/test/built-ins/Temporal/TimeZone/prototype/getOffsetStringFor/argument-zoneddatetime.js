// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getoffsetstringfor
description: Fast path for converting Temporal.ZonedDateTime to Temporal.Instant
info: |
    sec-temporal.timezone.prototype.getoffsetstringfor step 3:
      3. Set _instant_ to ? ToTemporalInstant(_instant_).
    sec-temporal-totemporalinstant step 1.b:
      b. If _item_ has an [[InitializedTemporalZonedDateTime]] internal slot, then
        i. Return ! CreateTemporalInstant(_item_.[[Nanoseconds]]).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

TemporalHelpers.checkToTemporalInstantFastPath((datetime) => {
  const timeZone = Temporal.TimeZone.from("UTC");
  const result = timeZone.getOffsetStringFor(datetime);
  assert.sameValue(result, "+00:00", "offset result");
});
