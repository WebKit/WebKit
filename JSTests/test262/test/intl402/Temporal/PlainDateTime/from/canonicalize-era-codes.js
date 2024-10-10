// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.from
description: Calendar era code is canonicalized
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const date1 = Temporal.PlainDateTime.from({
  calendar: "gregory",
  era: "ce",
  eraYear: 2024,
  year: 2024,
  month: 1,
  day: 1
});
TemporalHelpers.assertPlainDateTime(
  date1,
  2024, 1, "M01", 1, 0, 0, 0, 0, 0, 0,
  "'ce' is accepted as alias for 'gregory'",
  "gregory", 2024
);

const date2 = Temporal.PlainDateTime.from({
  calendar: "gregory",
  era: "bce",
  eraYear: 44,
  year: -43,
  month: 3,
  day: 15
});
TemporalHelpers.assertPlainDateTime(
  date2,
  -43, 3, "M03", 15, 0, 0, 0, 0, 0, 0,
  "'bce' is accepted as alias for 'gregory-inverse'",
  "gregory-inverse", 44
);
