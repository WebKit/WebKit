// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tozoneddatetime
description: >
  A Temporal.Calendar instance passed to toZonedDateTime() does not have its
  'calendar' property observably checked
features: [Temporal]
---*/

const instance = new Temporal.Instant(1_000_000_000_000_000_000n);

const arg = new Temporal.Calendar("iso8601");
Object.defineProperty(arg, "calendar", {
  get() {
    throw new Test262Error("calendar.calendar should not be accessed");
  },
});

instance.toZonedDateTime({ calendar: arg, timeZone: "UTC" });
instance.toZonedDateTime({ calendar: { calendar: arg }, timeZone: "UTC" });
