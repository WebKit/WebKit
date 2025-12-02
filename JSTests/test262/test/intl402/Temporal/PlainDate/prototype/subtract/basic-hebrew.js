// Copyright (C) 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.subtract
description: Basic addition and subtraction in the hebrew calendar
features: [Temporal, Intl.Era-monthcode]
includes: [temporalHelpers.js]
---*/

const calendar = "hebrew";
const options = { overflow: "reject" };

// Years

const years1 = new Temporal.Duration(-1);
const years1n = new Temporal.Duration(1);
const years5 = new Temporal.Duration(-5);
const years5n = new Temporal.Duration(5);

const date577902 = Temporal.PlainDate.from({ year: 5779, monthCode: "M02", day: 1, calendar }, options);
const date578402 = Temporal.PlainDate.from({ year: 5784, monthCode: "M02", day: 29, calendar }, options);

TemporalHelpers.assertPlainDate(
  date577902.subtract(years1),
  5780, 2, "M02", 1, "Adding 1 year to day 1 of a month",
  "am", 5780
);

TemporalHelpers.assertPlainDate(
  date578402.subtract(years1),
  5785, 2, "M02", 29, "Adding 1 year to day 29 of a month",
  "am", 5785
);

TemporalHelpers.assertPlainDate(
  date577902.subtract(years5),
  5784, 2, "M02", 1, "Adding 5 years to day 1 of a month",
  "am", 5784
);

TemporalHelpers.assertPlainDate(
  date578402.subtract(years5),
  5789, 2, "M02", 29, "Adding 5 years to day 29 of a month",
  "am", 5789
);

TemporalHelpers.assertPlainDate(
  date577902.subtract(years1n),
  5778, 2, "M02", 1, "Subtracting 1 year from day 1 of a month",
  "am", 5778
);

TemporalHelpers.assertPlainDate(
  date578402.subtract(years1n),
  5783, 2, "M02", 29, "Subtracting 1 year from day 29 of a month",
  "am", 5783
);

TemporalHelpers.assertPlainDate(
  date577902.subtract(years5n),
  5774, 2, "M02", 1, "Subtracting 5 years from day 1 of a month",
  "am", 5774
);

TemporalHelpers.assertPlainDate(
  date578402.subtract(years5n),
  5779, 2, "M02", 29, "Subtracting 5 years from day 29 of a month",
  "am", 5779
);

// Months

const months1 = new Temporal.Duration(0, -1);
const months1n = new Temporal.Duration(0, 1);
const months4 = new Temporal.Duration(0, -4);
const months4n = new Temporal.Duration(0, 4);

const date578001 = Temporal.PlainDate.from({ year: 5780, monthCode: "M01", day: 1, calendar }, options);
const date578006 = Temporal.PlainDate.from({ year: 5780, monthCode: "M06", day: 1, calendar }, options);
const date578011 = Temporal.PlainDate.from({ year: 5780, monthCode: "M11", day: 1, calendar }, options);
const date578012 = Temporal.PlainDate.from({ year: 5780, monthCode: "M12", day: 1, calendar }, options);

TemporalHelpers.assertPlainDate(
  date578011.subtract(months1),
  5780, 12, "M12", 1, "Adding 1 month, with result in same year",
  "am", 5780
);

TemporalHelpers.assertPlainDate(
  date578012.subtract(months1),
  5781, 1, "M01", 1, "Adding 1 month, with result in next year",
  "am", 5781
);

TemporalHelpers.assertPlainDate(
  date578006.subtract(months4),
  5780, 10, "M10", 1, "Adding 4 months, with result in same year",
  "am", 5780
);

TemporalHelpers.assertPlainDate(
  date578012.subtract(months4),
  5781, 4, "M04", 1, "Adding 4 months, with result in next year",
  "am", 5781
);

TemporalHelpers.assertPlainDate(
  date578011.subtract(months1n),
  5780, 10, "M10", 1, "Subtracting 1 month, with result in same year",
  "am", 5780
);

TemporalHelpers.assertPlainDate(
  date578001.subtract(months1n),
  5779, 13, "M12", 1, "Subtracting 1 month, with result in previous year",
  "am", 5779
);

TemporalHelpers.assertPlainDate(
  date578006.subtract(months4n),
  5780, 2, "M02", 1, "Subtracting 4 months, with result in same year",
  "am", 5780
);

TemporalHelpers.assertPlainDate(
  date578001.subtract(months4n),
  5779, 10, "M09", 1, "Subtracting 4 months, with result in previous year",
  "am", 5779
);

// Weeks

const months2weeks3 = new Temporal.Duration(0, /* months = */ -2, /* weeks = */ -3);
const months2weeks3n = new Temporal.Duration(0, 2, 3);

const date578201 = Temporal.PlainDate.from({ year: 5782, monthCode: "M01", day: 1, calendar }, options);

TemporalHelpers.assertPlainDate(
  date578201.subtract(months2weeks3),
  5782, 3, "M03", 22, "add 2 months 3 weeks, ending in same year",
  "am", 5782
);

TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 5782, monthCode: "M12", day: 29, calendar }, options).subtract(months2weeks3),
  5783, 3, "M03", 20, "add 2 months 3 weeks from end of year to next year",
  "am", 5783
);

TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 5782, monthCode: "M10", day: 1, calendar }, options).subtract(months2weeks3n),
  5782, 8, "M07", 10, "subtract 2 months 3 weeks, ending in same year",
  "am", 5782
);

TemporalHelpers.assertPlainDate(
  date578201.subtract(months2weeks3n),
  5781, 10, "M10", 9, "subtract 2 months 3 weeks from beginning of year to previous year",
  "am", 5781
);


// Days

const days10 = new Temporal.Duration(0, 0, 0, /* days = */ -10);
const days10n = new Temporal.Duration(0, 0, 0, 10);

const date57800129 = Temporal.PlainDate.from({ year: 5780, monthCode: "M01", day: 29, calendar }, options);

TemporalHelpers.assertPlainDate(
  date578201.subtract(days10),
  5782, 1, "M01", 11, "add 10 days, ending in same month",
  "am", 5782
);

TemporalHelpers.assertPlainDate(
  date57800129.subtract(days10),
  5780, 2, "M02", 9, "add 10 days, ending in following month",
  "am", 5780
);

TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 5782, monthCode: "M12", day: 29, calendar }, options).subtract(days10),
  5783, 1, "M01", 10, "add 10 days, ending in following year",
  "am", 5783
);

TemporalHelpers.assertPlainDate(
  date57800129.subtract(days10n),
  5780, 1, "M01", 19, "subtract 10 days, ending in same month",
  "am", 5780
);

TemporalHelpers.assertPlainDate(
  date578006.subtract(days10n),
  5780, 5, "M05", 21, "subtract 10 days, ending in previous month",
  "am", 5780
);

TemporalHelpers.assertPlainDate(
  date578201.subtract(days10n),
  5781, 12, "M12", 20, "subtract 10 days, ending in previous year",
  "am", 5781
);
