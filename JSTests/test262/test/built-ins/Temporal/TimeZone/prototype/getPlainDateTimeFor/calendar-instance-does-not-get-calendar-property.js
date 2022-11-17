// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getplaindatetimefor
description: >
  A Temporal.Calendar instance passed to getPlainDateTimeFor() does not have its
  'calendar' property observably checked
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

const arg = new Temporal.Calendar("iso8601");
Object.defineProperty(arg, "calendar", {
  get() {
    throw new Test262Error("calendar.calendar should not be accessed");
  },
});

instance.getPlainDateTimeFor(new Temporal.Instant(0n), arg);
instance.getPlainDateTimeFor(new Temporal.Instant(0n), { calendar: arg });
