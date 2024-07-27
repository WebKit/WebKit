// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.until
description: Calendar ID is canonicalized
features: [Temporal]
---*/

const instance = new Temporal.ZonedDateTime(1719923640_000_000_000n, "UTC", "islamic-civil");

[
  "2024-07-02T12:34+00:00[UTC][u-ca=islamicc]",
  { year: 1445, month: 12, day: 25, hour: 12, minute: 34, calendar: "islamicc", timeZone: "UTC" },
].forEach((arg) => {
  const result = instance.until(arg);  // would throw if calendar was not canonicalized
  assert(result.blank, "calendar ID is canonicalized");
});
