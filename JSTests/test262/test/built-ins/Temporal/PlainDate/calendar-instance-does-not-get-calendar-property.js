// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate
description: >
  A Temporal.Calendar instance passed to new PlainDate() does not have
  its 'calendar' property observably checked
features: [Temporal]
---*/

const arg = new Temporal.Calendar("iso8601");
Object.defineProperty(arg, "calendar", {
  get() {
    throw new Test262Error("calendar.calendar should not be accessed");
  },
});

new Temporal.PlainDate(2000, 5, 2, arg);
new Temporal.PlainDate(2000, 5, 2, { calendar: arg });
