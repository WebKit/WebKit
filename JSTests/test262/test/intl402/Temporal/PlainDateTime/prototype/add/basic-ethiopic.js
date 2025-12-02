// Copyright (C) 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.add
description: Basic addition and subtraction in the ethiopic calendar
features: [Temporal, Intl.Era-monthcode]
includes: [temporalHelpers.js]
---*/

const calendar = "ethiopic";
const options = { overflow: "reject" };

// Years

const years1 = new Temporal.Duration(1);
const years1n = new Temporal.Duration(-1);
const years5 = new Temporal.Duration(5);
const years5n = new Temporal.Duration(-5);

const date201802 = Temporal.PlainDateTime.from({ year: 2018, monthCode: "M02", day: 1, hour: 12, minute: 34, calendar }, options);
const date202302 = Temporal.PlainDateTime.from({ year: 2023, monthCode: "M02", day: 29, hour: 12, minute: 34, calendar }, options);

TemporalHelpers.assertPlainDateTime(
  date201802.add(years1),
  2019, 2, "M02", 1, 12, 34, 0, 0, 0, 0, "Adding 1 year to day 1 of a month", "am", 2019
);

TemporalHelpers.assertPlainDateTime(
  date202302.add(years1),
  2024, 2, "M02", 29, 12, 34, 0, 0, 0, 0, "Adding 1 year to day 29 of a month", "am", 2024
);

TemporalHelpers.assertPlainDateTime(
  date201802.add(years5),
  2023, 2, "M02", 1, 12, 34, 0, 0, 0, 0, "Adding 5 years to day 1 of a month", "am", 2023
);

TemporalHelpers.assertPlainDateTime(
  date202302.add(years5),
  2028, 2, "M02", 29, 12, 34, 0, 0, 0, 0, "Adding 5 years to day 29 of a month", "am", 2028
);

TemporalHelpers.assertPlainDateTime(
  date201802.add(years1n),
  2017, 2, "M02", 1, 12, 34, 0, 0, 0, 0, "Subtracting 1 year from day 1 of a month", "am", 2017
);

TemporalHelpers.assertPlainDateTime(
  date202302.add(years1n),
  2022, 2, "M02", 29, 12, 34, 0, 0, 0, 0, "Subtracting 1 year from day 29 of a month", "am", 2022
);

TemporalHelpers.assertPlainDateTime(
  date201802.add(years5n),
  2013, 2, "M02", 1, 12, 34, 0, 0, 0, 0, "Subtracting 5 years from day 1 of a month", "am", 2013
);

TemporalHelpers.assertPlainDateTime(
  date202302.add(years5n),
  2018, 2, "M02", 29, 12, 34, 0, 0, 0, 0, "Subtracting 5 years from day 29 of a month", "am", 2018
);

// Months

const months1 = new Temporal.Duration(0, 1);
const months1n = new Temporal.Duration(0, -1);
const months4 = new Temporal.Duration(0, 4);
const months4n = new Temporal.Duration(0, -4);

const date201901 = Temporal.PlainDateTime.from({ year: 2019, monthCode: "M01", day: 1, hour: 12, minute: 34, calendar }, options);
const date201906 = Temporal.PlainDateTime.from({ year: 2019, monthCode: "M06", day: 1, hour: 12, minute: 34, calendar }, options);
const date201911 = Temporal.PlainDateTime.from({ year: 2019, monthCode: "M11", day: 1, hour: 12, minute: 34, calendar }, options);
const date201813 = Temporal.PlainDateTime.from({ year: 2018, monthCode: "M13", day: 1, hour: 12, minute: 34, calendar }, options);

TemporalHelpers.assertPlainDateTime(
  date201911.add(months1),
  2019, 12, "M12", 1, 12, 34, 0, 0, 0, 0, "Adding 1 month, with result in same year", "am", 2019
);

TemporalHelpers.assertPlainDateTime(
  date201813.add(months1),
  2019, 1, "M01", 1, 12, 34, 0, 0, 0, 0, "Adding 1 month, with result in next year", "am", 2019
);

TemporalHelpers.assertPlainDateTime(
  date201906.add(months4),
  2019, 10, "M10", 1, 12, 34, 0, 0, 0, 0, "Adding 4 months, with result in same year", "am", 2019
);

TemporalHelpers.assertPlainDateTime(
  date201813.add(months4),
  2019, 4, "M04", 1, 12, 34, 0, 0, 0, 0, "Adding 4 months, with result in next year", "am", 2019
);

TemporalHelpers.assertPlainDateTime(
  date201911.add(months1n),
  2019, 10, "M10", 1, 12, 34, 0, 0, 0, 0, "Subtracting 1 month, with result in same year", "am", 2019
);

TemporalHelpers.assertPlainDateTime(
  date201901.add(months1n),
  2018, 13, "M13", 1, 12, 34, 0, 0, 0, 0, "Subtracting 1 month, with result in previous year", "am", 2018
);

TemporalHelpers.assertPlainDateTime(
  date201906.add(months4n),
  2019, 2, "M02", 1, 12, 34, 0, 0, 0, 0, "Subtracting 4 months, with result in same year", "am", 2019
);

TemporalHelpers.assertPlainDateTime(
  date201901.add(months4n),
  2018, 10, "M10", 1, 12, 34, 0, 0, 0, 0, "Subtracting 4 months, with result in previous year", "am", 2018
);

// Weeks

const months2weeks3 = new Temporal.Duration(0, /* months = */ 2, /* weeks = */ 3);
const months2weeks3n = new Temporal.Duration(0, -2, -3);

const date202101 = Temporal.PlainDateTime.from({ year: 2021, monthCode: "M01", day: 1, hour: 12, minute: 34, calendar }, options);

TemporalHelpers.assertPlainDateTime(
  date202101.add(months2weeks3),
  2021, 3, "M03", 22, 12, 34, 0, 0, 0, 0, "add 2 months 3 weeks from non-leap day/month, ending in same year", "am", 2021
);

TemporalHelpers.assertPlainDateTime(
  Temporal.PlainDateTime.from({ year: 2021, monthCode: "M12", day: 29, hour: 12, minute: 34, calendar }, options).add(months2weeks3),
  2022, 2, "M02", 20, 12, 34, 0, 0, 0, 0, "add 2 months 3 weeks from end of year to next year", "am", 2022
);

TemporalHelpers.assertPlainDateTime(
  Temporal.PlainDateTime.from({ year: 2021, monthCode: "M06", day: 1, hour: 12, minute: 34, calendar }, options).add(months2weeks3n),
  2021, 3, "M03", 10, 12, 34, 0, 0, 0, 0, "subtract 2 months 3 weeks from non-leap day/month, ending in same year", "am", 2021
);

TemporalHelpers.assertPlainDateTime(
  date202101.add(months2weeks3n),
  2020, 11, "M11", 10, 12, 34, 0, 0, 0, 0, "subtract 2 months 3 weeks from beginning of year to previous year", "am", 2020
);


// Days

const days10 = new Temporal.Duration(0, 0, 0, /* days = */ 10);
const days10n = new Temporal.Duration(0, 0, 0, -10);

const date20210129 = Temporal.PlainDateTime.from({ year: 2021, monthCode: "M01", day: 30, hour: 12, minute: 34, calendar }, options);

TemporalHelpers.assertPlainDateTime(
  date202101.add(days10),
  2021, 1, "M01", 11, 12, 34, 0, 0, 0, 0, "add 10 days, ending in same month", "am", 2021
);

TemporalHelpers.assertPlainDateTime(
  date20210129.add(days10),
  2021, 2, "M02", 10, 12, 34, 0, 0, 0, 0, "add 10 days, ending in following month", "am", 2021
);

TemporalHelpers.assertPlainDateTime(
  Temporal.PlainDateTime.from({ year: 2021, monthCode: "M13", day: 5, hour: 12, minute: 34, calendar }, options).add(days10),
  2022, 1, "M01", 10, 12, 34, 0, 0, 0, 0, "add 10 days, ending in following year", "am", 2022
);

TemporalHelpers.assertPlainDateTime(
  date20210129.add(days10n),
  2021, 1, "M01", 20, 12, 34, 0, 0, 0, 0, "subtract 10 days, ending in same month", "am", 2021
);

TemporalHelpers.assertPlainDateTime(
  date201906.add(days10n),
  2019, 5, "M05", 21, 12, 34, 0, 0, 0, 0, "subtract 10 days, ending in previous month", "am", 2019
);

TemporalHelpers.assertPlainDateTime(
  date202101.add(days10n),
  2020, 12, "M12", 26, 12, 34, 0, 0, 0, 0, "subtract 10 days, ending in previous year", "am", 2020
);
