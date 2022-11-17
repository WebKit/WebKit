// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tozoneddatetime
description: >
  A Temporal.TimeZone instance passed to toZonedDateTime() does not have its
  'timeZone' property observably checked
features: [Temporal]
---*/

const instance = new Temporal.PlainDateTime(2000, 5, 2);

const timeZone = new Temporal.TimeZone("UTC");
Object.defineProperty(timeZone, "timeZone", {
  get() {
    throw new Test262Error("timeZone.timeZone should not be accessed");
  },
});

instance.toZonedDateTime(timeZone);
instance.toZonedDateTime({ timeZone });
