// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindatetimeiso
description: >
  A Temporal.TimeZone instance passed to plainDateTimeISO() does not have its
  'timeZone' property observably checked
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
Object.defineProperty(timeZone, "timeZone", {
  get() {
    throw new Test262Error("timeZone.timeZone should not be accessed");
  },
});

Temporal.Now.plainDateTimeISO(timeZone);
Temporal.Now.plainDateTimeISO({ timeZone });
