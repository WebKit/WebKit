// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.zoneddatetime
description: >
  A Temporal.TimeZone instance passed to zonedDateTime() does not have its
  'timeZone' property observably checked
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
Object.defineProperty(timeZone, "timeZone", {
  get() {
    throw new Test262Error("timeZone.timeZone should not be accessed");
  },
});

Temporal.Now.zonedDateTime("iso8601", timeZone);
Temporal.Now.zonedDateTime("iso8601", { timeZone });
