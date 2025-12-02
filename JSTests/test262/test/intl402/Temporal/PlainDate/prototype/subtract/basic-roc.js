// Copyright (C) 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.subtract
description: >
  Check various basic calculations not involving leap years or constraining
  (roc calendar)
features: [Temporal, Intl.Era-monthcode]
includes: [temporalHelpers.js]
---*/

const calendar = "roc";

const years1 = new Temporal.Duration(-1);
const years1n = new Temporal.Duration(1);
const years4 = new Temporal.Duration(-4);
const years4n = new Temporal.Duration(4);

const date1110716 = Temporal.PlainDate.from({ year: 111, monthCode: "M07", day: 16, calendar });

TemporalHelpers.assertPlainDate(
  date1110716.subtract(years1),
  112, 7, "M07", 16, "add 1y",
  "roc", 112);
TemporalHelpers.assertPlainDate(
  date1110716.subtract(years4),
  115, 7, "M07", 16, "add 4y",
  "roc", 115);

TemporalHelpers.assertPlainDate(
  date1110716.subtract(years1n),
  110, 7, "M07", 16, "subtract 1y",
  "roc", 110);
TemporalHelpers.assertPlainDate(
  date1110716.subtract(years4n),
  107, 7, "M07", 16, "subtract 4y",
  "roc", 107);


// Months

const months5 = new Temporal.Duration(0, -5);
const months5n = new Temporal.Duration(0, 5);
const years1months2 = new Temporal.Duration(-1, -2);
const years1months2n = new Temporal.Duration(1, 2);

TemporalHelpers.assertPlainDate(
  date1110716.subtract(months5),
  111, 12, "M12", 16, "add 5mo with result in the same year",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M08", day: 16, calendar }).subtract(months5),
  112, 1, "M01", 16, "add 5mo with result in the next year",
  "roc", 112);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 109, monthCode: "M10", day: 1, calendar }).subtract(months5),
  110, 3, "M03", 1, "add 5mo with result in the next year on day 1 of month",
  "roc", 110);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M10", day: 31, calendar }).subtract(months5),
  112, 3, "M03", 31, "add 5mo with result in the next year on day 31 of month",
  "roc", 112);

TemporalHelpers.assertPlainDate(
  date1110716.subtract(years1months2),
  112, 9, "M09", 16, "add 1y 2mo",
  "roc", 112);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M11", day: 30, calendar }).subtract(years1months2),
  113, 1, "M01", 30, "add 1y 2mo with result in the next year",
  "roc", 113);

TemporalHelpers.assertPlainDate(
  date1110716.subtract(months5n),
  111, 2, "M02", 16, "subtract 5mo with result in the same year",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M01", day: 16, calendar }).subtract(months5n),
  110, 8, "M08", 16, "subtract 5mo with result in the previous year",
  "roc", 110);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 109, monthCode: "M02", day: 1, calendar }).subtract(months5n),
  108, 9, "M09", 1, "subtract 5mo with result in the previous year on day 1 of month",
  "roc", 108);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M03", day: 31, calendar }).subtract(months5n),
  110, 10, "M10", 31, "subtract 5mo with result in the previous year on day 31 of month",
  "roc", 110);

TemporalHelpers.assertPlainDate(
  date1110716.subtract(years1months2n),
  110, 5, "M05", 16, "subtract 1y 2mo",
  "roc", 110);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M02", day: 17, calendar }).subtract(years1months2n),
  109, 12, "M12", 17, "subtract 1y 2mo with result in the previous year",
  "roc", 109);

// Weeks

const weeks1 = new Temporal.Duration(0, 0, -1);
const weeks1n = new Temporal.Duration(0, 0, 1);
const weeks6 = new Temporal.Duration(0, 0, -6);
const weeks6n = new Temporal.Duration(0, 0, 6);
const years1weeks2 = new Temporal.Duration(-1, 0, -2);
const years1weeks2n = new Temporal.Duration(1, 0, 2);
const months2weeks3 = new Temporal.Duration(0, -2, -3);
const months2weeks3n = new Temporal.Duration(0, 2, 3);

const date1101228 = Temporal.PlainDate.from({ year: 110, monthCode: "M12", day: 28, calendar });
const date1110127 = Temporal.PlainDate.from({ year: 111, monthCode: "M01", day: 27, calendar });
const date1110219 = Temporal.PlainDate.from({ year: 111, monthCode: "M02", day: 19, calendar });
const date1110604 = Temporal.PlainDate.from({ year: 111, monthCode: "M06", day: 4, calendar });
const date1110627 = Temporal.PlainDate.from({ year: 111, monthCode: "M06", day: 27, calendar });
const date1110727 = Temporal.PlainDate.from({ year: 111, monthCode: "M07", day: 27, calendar });
const date1111224 = Temporal.PlainDate.from({ year: 111, monthCode: "M12", day: 24, calendar });

TemporalHelpers.assertPlainDate(
  date1110219.subtract(weeks1),
  111, 2, "M02", 26, "add 1w",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  date1111224.subtract(weeks1),
  111, 12, "M12", 31, "add 1w with result on the last day of the year",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M12", day: 25, calendar }).subtract(weeks1),
  112, 1, "M01", 1, "add 1w with result on the first day of the next year",
  "roc", 112);

TemporalHelpers.assertPlainDate(
  date1110127.subtract(weeks1),
  111, 2, "M02", 3, "add 1w in a 31-day month with result in the next month",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  date1110727.subtract(weeks1),
  111, 8, "M08", 3, "add 1w in another 31-day month with result in the next month",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  date1110627.subtract(weeks1),
  111, 7, "M07", 4, "add 1w in a 30-day month with result in the next month",
  "roc", 111);

TemporalHelpers.assertPlainDate(
  date1110127.subtract(weeks6),
  111, 3, "M03", 10, "add 6w with result in the same year",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  date1111224.subtract(weeks6),
  112, 2, "M02", 4, "add 6w with result in the next year",
  "roc", 112);
TemporalHelpers.assertPlainDate(
  date1110627.subtract(weeks6),
  111, 8, "M08", 8, "add 6w crossing months of 30 and 31 days",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  date1110727.subtract(weeks6),
  111, 9, "M09", 7, "add 6w crossing months of 31 and 31 days",
  "roc", 111);

TemporalHelpers.assertPlainDate(
  date1101228.subtract(years1weeks2),
  112, 1, "M01", 11, "add 1y 2w with result in the next year",
  "roc", 112);

TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 109, monthCode: "M10", day: 28, calendar }).subtract(months2weeks3),
  110, 1, "M01", 18, "add 2mo 3w with result in the next year",
  "roc", 110);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 109, monthCode: "M10", day: 31, calendar }).subtract(months2weeks3),
  110, 1, "M01", 21, "add 2mo 3w with result in the next year",
  "roc", 110);

TemporalHelpers.assertPlainDate(
  date1110219.subtract(weeks1n),
  111, 2, "M02", 12, "subtract 1w",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M01", day: 8, calendar }).subtract(weeks1n),
  111, 1, "M01", 1, "subtract 1w with result on the first day of the year",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M01", day: 7, calendar }).subtract(weeks1n),
  110, 12, "M12", 31, "subtract 1w with result on the last day of the previous year",
  "roc", 110);

TemporalHelpers.assertPlainDate(
  date1110604.subtract(weeks1n),
  111, 5, "M05", 28, "subtract 1w with result in the previous 31-day month",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M07", day: 3, calendar }).subtract(weeks1n),
  111, 6, "M06", 26, "subtract 1w with result in the previous 30-day month",
  "roc", 111);

TemporalHelpers.assertPlainDate(
  date1110604.subtract(weeks6n),
  111, 4, "M04", 23, "subtract 6w with result in the same year",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  date1110127.subtract(weeks6n),
  110, 12, "M12", 16, "subtract 6w with result in the previous year",
  "roc", 110);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M09", day: 8, calendar }).subtract(weeks6n),
  111, 7, "M07", 28, "subtract 6w crossing months of 30 and 31 days",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M08", day: 8, calendar }).subtract(weeks6n),
  111, 6, "M06", 27, "subtract 6w crossing months of 31 and 31 days",
  "roc", 111);

TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 112, monthCode: "M01", day: 5, calendar }).subtract(years1weeks2n),
  110, 12, "M12", 22, "subtract 1y 2w with result in the previous year",
  "roc", 110);

TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 109, monthCode: "M03", day: 2, calendar }).subtract(months2weeks3n),
  108, 12, "M12", 12, "subtract 2mo 3w with result in the previous year",
  "roc", 108);

// Days

const days10 = new Temporal.Duration(0, 0, 0, -10);
const days10n = new Temporal.Duration(0, 0, 0, 10);
const weeks2days3 = new Temporal.Duration(0, 0, -2, -3);
const weeks2days3n = new Temporal.Duration(0, 0, 2, 3);
const years1months2days4 = new Temporal.Duration(-1, -2, 0, -4);
const years1months2days4n = new Temporal.Duration(1, 2, 0, 4);

TemporalHelpers.assertPlainDate(
  date1110716.subtract(days10),
  111, 7, "M07", 26, "add 10 days with result in the same month",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M07", day: 26, calendar }).subtract(days10),
  111, 8, "M08", 5, "add 10 days with result in the next month",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M12", day: 26, calendar }).subtract(days10),
  112, 1, "M01", 5, "add 10 days with result in the next year",
  "roc", 112);

TemporalHelpers.assertPlainDate(
  date1101228.subtract(weeks2days3),
  111, 1, "M01", 14, "add 2w 3d with result in the next year",
  "roc", 111);

TemporalHelpers.assertPlainDate(
  date1110716.subtract(years1months2days4),
  112, 9, "M09", 20, "add 1y 2mo 4d",
  "roc", 112);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M02", day: 27, calendar }).subtract(years1months2days4),
  112, 5, "M05", 1, "add 1y 2mo 4d with result in a month following a 30-day month",
  "roc", 112);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M07", day: 30, calendar }).subtract(years1months2days4),
  112, 10, "M10", 4, "add 1y 2mo 4d with result in a month following a 30-day month",
  "roc", 112);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M01", day: 28, calendar }).subtract(years1months2days4),
  112, 4, "M04", 1, "add 1y 2mo 4d with result in a month following a 31-day month",
  "roc", 112);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M06", day: 30, calendar }).subtract(years1months2days4),
  112, 9, "M09", 3, "add 1y 2mo 4d with result in a month following a 31-day month",
  "roc", 112);

TemporalHelpers.assertPlainDate(
  date1110716.subtract(days10n),
  111, 7, "M07", 6, "subtract 10 days with result in the same month",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M07", day: 6, calendar }).subtract(days10n),
  111, 6, "M06", 26, "subtract 10 days with result in the previous month",
  "roc", 111);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M01", day: 4, calendar }).subtract(days10n),
  110, 12, "M12", 25, "subtract 10 days with result in the previous year",
  "roc", 110);

TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M01", day: 15, calendar }).subtract(weeks2days3n),
  110, 12, "M12", 29, "subtract 2w 3d with result in the previous year",
  "roc", 110);

TemporalHelpers.assertPlainDate(
  date1110716.subtract(years1months2days4n),
  110, 5, "M05", 12, "subtract 1y 2mo 4d",
  "roc", 110);
TemporalHelpers.assertPlainDate(
  Temporal.PlainDate.from({ year: 111, monthCode: "M07", day: 4, calendar }).subtract(years1months2days4n),
  110, 4, "M04", 30, "subtract 1y 2mo 4d with result in a 30-day month",
  "roc", 110);
TemporalHelpers.assertPlainDate(
  date1110604.subtract(years1months2days4n),
  110, 3, "M03", 31, "subtract 1y 2mo 4d with result in a 31-day month",
  "roc", 110);
