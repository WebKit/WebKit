// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.with
description: Calendar-specific mutually exclusive keys in mergeFields
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.PlainDate(1981, 12, 15, "gregory");

TemporalHelpers.assertPlainDate(instance, 1981, 12, "M12", 15,
  "check that all fields are as expected",
  /* era = */ "ce", /* eraYear = */ 1981
);

TemporalHelpers.assertPlainDate(
  instance.with({ era: "bce", eraYear: 1 }),
  0, 12, "M12", 15,
  "era and eraYear together exclude year",
  "bce", 1
);

TemporalHelpers.assertPlainDate(
  instance.with({ year: -2 }),
  -2, 12, "M12", 15,
  "year excludes era and eraYear",
  "bce", 3
);

TemporalHelpers.assertPlainDate(
  instance.with({ month: 5 }),
  1981, 5, "M05", 15,
  "month excludes monthCode",
  "ce", 1981
);

TemporalHelpers.assertPlainDate(
  instance.with({ monthCode: "M05" }),
  1981, 5, "M05", 15,
  "monthCode excludes month",
  "ce", 1981
);
