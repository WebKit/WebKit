// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaindate.prototype.dayofyear
description: Basic tests for dayOfYear().
features: [Temporal]
---*/

for (let i = 1; i <= 7; ++i) {
  const plainDate = new Temporal.PlainDate(1976, 11, 14 + i);
  assert.sameValue(plainDate.dayOfYear, 319 + i, `${plainDate} should be on day ${319 + i}`);
}
