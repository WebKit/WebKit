// Copyright (C) 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.subtract
description: Basic addition and subtraction in the ethioaa calendar
features: [Temporal, Intl.Era-monthcode]
includes: [temporalHelpers.js]
---*/

const calendar = "ethioaa";
const options = { overflow: "reject" };

// Years

const years1 = new Temporal.Duration(-1);
const years1n = new Temporal.Duration(1);
const years5 = new Temporal.Duration(-5);
const years5n = new Temporal.Duration(5);

const date750302 = Temporal.ZonedDateTime.from({ year: 7503, monthCode: "M02", day: 1, hour: 12, minute: 34, timeZone: "UTC", calendar }, options);
const date750802 = Temporal.ZonedDateTime.from({ year: 7508, monthCode: "M02", day: 29, hour: 12, minute: 34, timeZone: "UTC", calendar }, options);

TemporalHelpers.assertPlainDateTime(
  date750302.subtract(years1).toPlainDateTime(),
  7504, 2, "M02", 1, 12, 34, 0, 0, 0, 0, "Adding 1 year to day 1 of a month", "aa", 7504
);

TemporalHelpers.assertPlainDateTime(
  date750802.subtract(years1).toPlainDateTime(),
  7509, 2, "M02", 29, 12, 34, 0, 0, 0, 0, "Adding 1 year to day 29 of a month", "aa", 7509
);

TemporalHelpers.assertPlainDateTime(
  date750302.subtract(years5).toPlainDateTime(),
  7508, 2, "M02", 1, 12, 34, 0, 0, 0, 0, "Adding 5 years to day 1 of a month", "aa", 7508
);

TemporalHelpers.assertPlainDateTime(
  date750802.subtract(years5).toPlainDateTime(),
  7513, 2, "M02", 29, 12, 34, 0, 0, 0, 0, "Adding 5 years to day 29 of a month", "aa", 7513
);

TemporalHelpers.assertPlainDateTime(
  date750302.subtract(years1n).toPlainDateTime(),
  7502, 2, "M02", 1, 12, 34, 0, 0, 0, 0, "Subtracting 1 year from day 1 of a month", "aa", 7502
);

TemporalHelpers.assertPlainDateTime(
  date750802.subtract(years1n).toPlainDateTime(),
  7507, 2, "M02", 29, 12, 34, 0, 0, 0, 0, "Subtracting 1 year from day 29 of a month", "aa", 7507
);

TemporalHelpers.assertPlainDateTime(
  date750302.subtract(years5n).toPlainDateTime(),
  7498, 2, "M02", 1, 12, 34, 0, 0, 0, 0, "Subtracting 5 years from day 1 of a month", "aa", 7498
);

TemporalHelpers.assertPlainDateTime(
  date750802.subtract(years5n).toPlainDateTime(),
  7503, 2, "M02", 29, 12, 34, 0, 0, 0, 0, "Subtracting 5 years from day 29 of a month", "aa", 7503
);

// Months

const months1 = new Temporal.Duration(0, -1);
const months1n = new Temporal.Duration(0, 1);
const months4 = new Temporal.Duration(0, -4);
const months4n = new Temporal.Duration(0, 4);

const date750401 = Temporal.ZonedDateTime.from({ year: 7504, monthCode: "M01", day: 1, hour: 12, minute: 34, timeZone: "UTC", calendar }, options);
const date750406 = Temporal.ZonedDateTime.from({ year: 7504, monthCode: "M06", day: 1, hour: 12, minute: 34, timeZone: "UTC", calendar }, options);
const date750411 = Temporal.ZonedDateTime.from({ year: 7504, monthCode: "M11", day: 1, hour: 12, minute: 34, timeZone: "UTC", calendar }, options);
const date750313 = Temporal.ZonedDateTime.from({ year: 7503, monthCode: "M13", day: 1, hour: 12, minute: 34, timeZone: "UTC", calendar }, options);

TemporalHelpers.assertPlainDateTime(
  date750411.subtract(months1).toPlainDateTime(),
  7504, 12, "M12", 1, 12, 34, 0, 0, 0, 0, "Adding 1 month, with result in same year", "aa", 7504
);

TemporalHelpers.assertPlainDateTime(
  date750313.subtract(months1).toPlainDateTime(),
  7504, 1, "M01", 1, 12, 34, 0, 0, 0, 0, "Adding 1 month, with result in next year", "aa", 7504
);

TemporalHelpers.assertPlainDateTime(
  date750406.subtract(months4).toPlainDateTime(),
  7504, 10, "M10", 1, 12, 34, 0, 0, 0, 0, "Adding 4 months, with result in same year", "aa", 7504
);

TemporalHelpers.assertPlainDateTime(
  date750313.subtract(months4).toPlainDateTime(),
  7504, 4, "M04", 1, 12, 34, 0, 0, 0, 0, "Adding 4 months, with result in next year", "aa", 7504
);

TemporalHelpers.assertPlainDateTime(
  date750411.subtract(months1n).toPlainDateTime(),
  7504, 10, "M10", 1, 12, 34, 0, 0, 0, 0, "Subtracting 1 month, with result in same year", "aa", 7504
);

TemporalHelpers.assertPlainDateTime(
  date750401.subtract(months1n).toPlainDateTime(),
  7503, 13, "M13", 1, 12, 34, 0, 0, 0, 0, "Subtracting 1 month, with result in previous year", "aa", 7503
);

TemporalHelpers.assertPlainDateTime(
  date750406.subtract(months4n).toPlainDateTime(),
  7504, 2, "M02", 1, 12, 34, 0, 0, 0, 0, "Subtracting 4 months, with result in same year", "aa", 7504
);

TemporalHelpers.assertPlainDateTime(
  date750401.subtract(months4n).toPlainDateTime(),
  7503, 10, "M10", 1, 12, 34, 0, 0, 0, 0, "Subtracting 4 months, with result in previous year", "aa", 7503
);

// Weeks

const months2weeks3 = new Temporal.Duration(0, /* months = */ -2, /* weeks = */ -3);
const months2weeks3n = new Temporal.Duration(0, 2, 3);

const date750601 = Temporal.ZonedDateTime.from({ year: 7506, monthCode: "M01", day: 1, hour: 12, minute: 34, timeZone: "UTC", calendar }, options);

TemporalHelpers.assertPlainDateTime(
  date750601.subtract(months2weeks3).toPlainDateTime(),
  7506, 3, "M03", 22, 12, 34, 0, 0, 0, 0, "add 2 months 3 weeks from non-leap day/month, ending in same year", "aa", 7506
);

TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 7506, monthCode: "M12", day: 29, hour: 12, minute: 34, timeZone: "UTC", calendar }, options).subtract(months2weeks3).toPlainDateTime(),
  7507, 2, "M02", 20, 12, 34, 0, 0, 0, 0, "add 2 months 3 weeks from end of year to next year", "aa", 7507
);

TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 7506, monthCode: "M06", day: 1, hour: 12, minute: 34, timeZone: "UTC", calendar }, options).subtract(months2weeks3n).toPlainDateTime(),
  7506, 3, "M03", 10, 12, 34, 0, 0, 0, 0, "subtract 2 months 3 weeks from non-leap day/month, ending in same year", "aa", 7506
);

TemporalHelpers.assertPlainDateTime(
  date750601.subtract(months2weeks3n).toPlainDateTime(),
  7505, 11, "M11", 10, 12, 34, 0, 0, 0, 0, "subtract 2 months 3 weeks from beginning of year to previous year", "aa", 7505
);


// Days

const days10 = new Temporal.Duration(0, 0, 0, /* days = */ -10);
const days10n = new Temporal.Duration(0, 0, 0, 10);

const date75060129 = Temporal.ZonedDateTime.from({ year: 7506, monthCode: "M01", day: 30, hour: 12, minute: 34, timeZone: "UTC", calendar }, options);

TemporalHelpers.assertPlainDateTime(
  date750601.subtract(days10).toPlainDateTime(),
  7506, 1, "M01", 11, 12, 34, 0, 0, 0, 0, "add 10 days, ending in same month", "aa", 7506
);

TemporalHelpers.assertPlainDateTime(
  date75060129.subtract(days10).toPlainDateTime(),
  7506, 2, "M02", 10, 12, 34, 0, 0, 0, 0, "add 10 days, ending in following month", "aa", 7506
);

TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 7506, monthCode: "M13", day: 5, hour: 12, minute: 34, timeZone: "UTC", calendar }, options).subtract(days10).toPlainDateTime(),
  7507, 1, "M01", 10, 12, 34, 0, 0, 0, 0, "add 10 days, ending in following year", "aa", 7507
);

TemporalHelpers.assertPlainDateTime(
  date75060129.subtract(days10n).toPlainDateTime(),
  7506, 1, "M01", 20, 12, 34, 0, 0, 0, 0, "subtract 10 days, ending in same month", "aa", 7506
);

TemporalHelpers.assertPlainDateTime(
  date750406.subtract(days10n).toPlainDateTime(),
  7504, 5, "M05", 21, 12, 34, 0, 0, 0, 0, "subtract 10 days, ending in previous month", "aa", 7504
);

TemporalHelpers.assertPlainDateTime(
  date750601.subtract(days10n).toPlainDateTime(),
  7505, 12, "M12", 26, 12, 34, 0, 0, 0, 0, "subtract 10 days, ending in previous year", "aa", 7505
);
