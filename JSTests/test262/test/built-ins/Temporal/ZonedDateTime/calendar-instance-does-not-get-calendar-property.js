// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime
description: >
  A Temporal.Calendar instance passed to new ZonedDateTime() does not have
  its 'calendar' property observably checked
features: [Temporal]
---*/

const arg = new Temporal.Calendar("iso8601");
Object.defineProperty(arg, "calendar", {
  get() {
    throw new Test262Error("calendar.calendar should not be accessed");
  },
});

new Temporal.ZonedDateTime(0n, "UTC", arg);
new Temporal.ZonedDateTime(0n, "UTC", { calendar: arg });
