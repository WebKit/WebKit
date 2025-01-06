// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: Calendar era code is canonicalized
features: [Temporal]
---*/

const date1 = Temporal.ZonedDateTime.from({
  calendar: "gregory",
  era: "ce",
  eraYear: 2024,
  year: 2024,
  month: 1,
  day: 1,
  timeZone: "UTC",
});
assert.sameValue(date1.era, "gregory", "'ce' is accepted as alias for 'gregory'");

const date2 = Temporal.ZonedDateTime.from({
  calendar: "gregory",
  era: "bce",
  eraYear: 44,
  year: -43,
  month: 3,
  day: 15,
  timeZone: "Europe/Rome",
});
assert.sameValue(date2.era, "gregory-inverse", "'bce' is accepted as alias for 'gregory-inverse'");
