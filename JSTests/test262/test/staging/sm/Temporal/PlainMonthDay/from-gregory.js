// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-Temporal-PlainMonthDay-shell.js]
flags:
  - noStrict
features:
  - Temporal
description: |
  pending
esid: pending
---*/

// Equivalent monthCode and month are resolved to the same PlainMonthDay.
{
  let withMonthCode = Temporal.PlainMonthDay.from({
    calendar: "gregory",
    year: 2023,
    monthCode: "M02",
    day: 30,
  });

  let withMonth = Temporal.PlainMonthDay.from({
    calendar: "gregory",
    year: 2023,
    month: 2,
    day: 30,
  });

  assert.sameValue(withMonthCode.equals(withMonth), true);
}

// eraYear and year must be consistent when monthCode is present.
{
  let fields = {
    calendar: "gregory",
    era: "ce",
    eraYear: 2024,
    year: 2023,
    monthCode: "M01",
    day: 1,
  };
  assertThrowsInstanceOf(() => Temporal.PlainMonthDay.from(fields), RangeError);
}

// eraYear and year must be consistent when month is present.
{
  let fields = {
    calendar: "gregory",
    era: "ce",
    eraYear: 2024,
    year: 2023,
    month: 1,
    day: 1,
  };
  assertThrowsInstanceOf(() => Temporal.PlainMonthDay.from(fields), RangeError);
}

// monthCode and month must be consistent.
{
  let fields = {
    calendar: "gregory",
    year: 2024,
    monthCode: "M01",
    month: 2,
    day: 1,
  };
  assertThrowsInstanceOf(() => Temporal.PlainMonthDay.from(fields), RangeError);
}

