// Copyright (C) 2025 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
features:
  - Temporal
description: |
  pending
esid: pending
---*/

// https://github.com/unicode-org/icu4x/issues/5070

let fromIso = new Temporal.PlainDate(2000, 12, 31, "indian");

let fromIndian = Temporal.PlainDate.from({
  calendar: "indian",
  year: fromIso.year,
  month: fromIso.month,
  day: fromIso.day,
});

assert.sameValue(fromIndian.equals(fromIso), true);

