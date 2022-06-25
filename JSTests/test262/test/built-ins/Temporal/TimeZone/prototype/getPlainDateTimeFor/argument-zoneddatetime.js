// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getplaindatetimefor
description: Fast path for converting Temporal.ZonedDateTime to Temporal.Instant
info: |
    sec-temporal.timezone.prototype.getplaindatetimefor step 2:
      2. Set _instant_ to ? ToTemporalInstant(_instant_).
    sec-temporal-totemporalinstant step 1.b:
      b. If _item_ has an [[InitializedTemporalZonedDateTime]] internal slot, then
        i. Return ! CreateTemporalInstant(_item_.[[Nanoseconds]]).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

TemporalHelpers.checkToTemporalInstantFastPath((datetime) => {
  const timeZone = Temporal.TimeZone.from("UTC");
  const result = timeZone.getPlainDateTimeFor(datetime);
  TemporalHelpers.assertPlainDateTime(result, 2001, 9, "M09", 9, 1, 46, 40, 987, 654, 321);
});
