// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tozoneddatetimeiso
description: >
  A Temporal.TimeZone instance passed to toZonedDateTimeISO() does not have its
  'timeZone' property observably checked
features: [Temporal]
---*/

const instance = new Temporal.Instant(0n);

const timeZone = new Temporal.TimeZone("UTC");
Object.defineProperty(timeZone, "timeZone", {
  get() {
    throw new Test262Error("timeZone.timeZone should not be accessed");
  },
});

instance.toZonedDateTimeISO(timeZone);
instance.toZonedDateTimeISO({ timeZone });
