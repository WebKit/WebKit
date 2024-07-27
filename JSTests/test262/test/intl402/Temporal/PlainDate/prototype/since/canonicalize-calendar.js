// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.since
description: Calendar ID is canonicalized
features: [Temporal]
---*/

const instance = new Temporal.PlainDate(2024, 7, 2, "islamic-civil");

[
  "2024-07-02[u-ca=islamicc]",
  { year: 1445, month: 12, day: 25, calendar: "islamicc" },
].forEach((arg) => {
  const result = instance.since(arg);  // would throw if calendar was not canonicalized
  assert(result.blank, "calendar ID is canonicalized");
});
