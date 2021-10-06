// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
description: Basic tests for PlainDate.from() with the Gregorian calendar.
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from("1999-12-31[u-ca=gregory]"),
  1999, 12, "M12", 31, "string era CE", "ce", 1999);

TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from("-000001-12-31[u-ca=gregory]"),
  -1, 12, "M12", 31, "string era BCE", "bce", 2);

TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ era: "ce", eraYear: 1999, month: 12, day: 31, calendar: "gregory" }),
  1999, 12, "M12", 31, "property bag explicit era CE", "ce", 1999);

TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 1999, month: 12, day: 31, calendar: "gregory" }),
  1999, 12, "M12", 31, "property bag implicit era CE", "ce", 1999);

TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ era: "bce", eraYear: 2, month: 12, day: 31, calendar: "gregory" }),
  -1, 12, "M12", 31, "property bag era BCE", "bce", 2);
