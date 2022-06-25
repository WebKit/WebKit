// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.from
description: Strings with fractional duration units are rounded with the correct rounding mode
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const resultPosHours = Temporal.Duration.from("PT1.03125H");
TemporalHelpers.assertDuration(resultPosHours, 0, 0, 0, 0, 1, 1, 52, 500, 0, 0,
  "positive fractional hours rounded with correct rounding mode");

const resultNegHours = Temporal.Duration.from("-PT1.03125H");
TemporalHelpers.assertDuration(resultNegHours, 0, 0, 0, 0, -1, -1, -52, -500, 0, 0,
  "negative fractional hours rounded with correct rounding mode");

// The following input should not round, but may fail if an implementation does
// floating point arithmetic too early:

const resultPosSeconds = Temporal.Duration.from("PT46H66M71.50040904S");
TemporalHelpers.assertDuration(resultPosSeconds, 0, 0, 0, 0, 46, 66, 71, 500, 409, 40,
  "positive fractional seconds not rounded");

const resultNegSeconds = Temporal.Duration.from("-PT46H66M71.50040904S");
TemporalHelpers.assertDuration(resultNegSeconds, 0, 0, 0, 0, -46, -66, -71, -500, -409, -40,
  "negative fractional seconds not rounded");
