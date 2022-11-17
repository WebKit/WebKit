// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withcalendar
description: >
  A Temporal.Calendar instance passed to withCalendar() does not have its
  'calendar' property observably checked
features: [Temporal]
---*/

const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", { id: "replace-me" });

const arg = new Temporal.Calendar("iso8601");
Object.defineProperty(arg, "calendar", {
  get() {
    throw new Test262Error("calendar.calendar should not be accessed");
  },
});

instance.withCalendar(arg);
instance.withCalendar({ calendar: arg });
