// Copyright (C) 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.add
description: >
  Check various basic calculations not involving leap years or constraining
  (indian calendar)
features: [Temporal, Intl.Era-monthcode]
includes: [temporalHelpers.js]
---*/

const calendar = "indian";

const years1 = new Temporal.Duration(1);
const years1n = new Temporal.Duration(-1);
const years4 = new Temporal.Duration(4);
const years4n = new Temporal.Duration(-4);

const date19200716 = Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M07", day: 16, hour: 12, minute: 34, timeZone: "UTC", calendar });

TemporalHelpers.assertPlainDateTime(
  date19200716.add(years1).toPlainDateTime(),
  1921, 7, "M07", 16, 12, 34, 0, 0, 0, 0, "add 1y",
  "shaka", 1921);
TemporalHelpers.assertPlainDateTime(
  date19200716.add(years4).toPlainDateTime(),
  1924, 7, "M07", 16, 12, 34, 0, 0, 0, 0, "add 4y",
  "shaka", 1924);

TemporalHelpers.assertPlainDateTime(
  date19200716.add(years1n).toPlainDateTime(),
  1919, 7, "M07", 16, 12, 34, 0, 0, 0, 0, "subtract 1y",
  "shaka", 1919);
TemporalHelpers.assertPlainDateTime(
  date19200716.add(years4n).toPlainDateTime(),
  1916, 7, "M07", 16, 12, 34, 0, 0, 0, 0, "subtract 4y",
  "shaka", 1916);


// Months

const months5 = new Temporal.Duration(0, 5);
const months5n = new Temporal.Duration(0, -5);
const months8 = new Temporal.Duration(0, 8);
const months8n = new Temporal.Duration(0, -8);
const years1months2 = new Temporal.Duration(1, 2);
const years1months2n = new Temporal.Duration(-1, -2);

TemporalHelpers.assertPlainDateTime(
  date19200716.add(months5).toPlainDateTime(),
  1920, 12, "M12", 16, 12, 34, 0, 0, 0, 0, "add 5mo with result in the same year",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M08", day: 16, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(months5).toPlainDateTime(),
  1921, 1, "M01", 16, 12, 34, 0, 0, 0, 0, "add 5mo with result in the next year",
  "shaka", 1921);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1918, monthCode: "M10", day: 1, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(months5).toPlainDateTime(),
  1919, 3, "M03", 1, 12, 34, 0, 0, 0, 0, "add 5mo with result in the next year on day 1 of month",
  "shaka", 1919);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M06", day: 31, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(months8).toPlainDateTime(),
  1921, 2, "M02", 31, 12, 34, 0, 0, 0, 0, "add 8mo with result in the next year on day 31 of month",
  "shaka", 1921);

TemporalHelpers.assertPlainDateTime(
  date19200716.add(years1months2).toPlainDateTime(),
  1921, 9, "M09", 16, 12, 34, 0, 0, 0, 0, "add 1y 2mo",
  "shaka", 1921);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M11", day: 30, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(years1months2).toPlainDateTime(),
  1922, 1, "M01", 30, 12, 34, 0, 0, 0, 0, "add 1y 2mo with result in the next year",
  "shaka", 1922);

TemporalHelpers.assertPlainDateTime(
  date19200716.add(months5n).toPlainDateTime(),
  1920, 2, "M02", 16, 12, 34, 0, 0, 0, 0, "subtract 5mo with result in the same year",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M01", day: 16, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(months5n).toPlainDateTime(),
  1919, 8, "M08", 16, 12, 34, 0, 0, 0, 0, "subtract 5mo with result in the previous year",
  "shaka", 1919);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1918, monthCode: "M02", day: 1, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(months5n).toPlainDateTime(),
  1917, 9, "M09", 1, 12, 34, 0, 0, 0, 0, "subtract 5mo with result in the previous year on day 1 of month",
  "shaka", 1917);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M02", day: 31, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(months8n).toPlainDateTime(),
  1919, 6, "M06", 31, 12, 34, 0, 0, 0, 0, "subtract 8mo with result in the previous year on day 31 of month",
  "shaka", 1919);

TemporalHelpers.assertPlainDateTime(
  date19200716.add(years1months2n).toPlainDateTime(),
  1919, 5, "M05", 16, 12, 34, 0, 0, 0, 0, "subtract 1y 2mo",
  "shaka", 1919);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M02", day: 17, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(years1months2n).toPlainDateTime(),
  1918, 12, "M12", 17, 12, 34, 0, 0, 0, 0, "subtract 1y 2mo with result in the previous year",
  "shaka", 1918);

// Weeks

const weeks1 = new Temporal.Duration(0, 0, 1);
const weeks1n = new Temporal.Duration(0, 0, -1);
const weeks6 = new Temporal.Duration(0, 0, 6);
const weeks6n = new Temporal.Duration(0, 0, -6);
const years1weeks2 = new Temporal.Duration(1, 0, 2);
const years1weeks2n = new Temporal.Duration(-1, 0, -2);
const months2weeks3 = new Temporal.Duration(0, 2, 3);
const months2weeks3n = new Temporal.Duration(0, -2, -3);

const date19201228 = Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M12", day: 28, hour: 12, minute: 34, timeZone: "UTC", calendar });
const date19200127 = Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M01", day: 27, hour: 12, minute: 34, timeZone: "UTC", calendar });
const date19200219 = Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M02", day: 19, hour: 12, minute: 34, timeZone: "UTC", calendar });
const date19200527 = Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M05", day: 27, hour: 12, minute: 34, timeZone: "UTC", calendar });
const date19200604 = Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M06", day: 4, hour: 12, minute: 34, timeZone: "UTC", calendar });
const date19200627 = Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M06", day: 27, hour: 12, minute: 34, timeZone: "UTC", calendar });
const date19200704 = Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M07", day: 4, hour: 12, minute: 34, timeZone: "UTC", calendar });
const date19200727 = Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M07", day: 27, hour: 12, minute: 34, timeZone: "UTC", calendar });
const date19201223 = Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M12", day: 23, hour: 12, minute: 34, timeZone: "UTC", calendar });

TemporalHelpers.assertPlainDateTime(
  date19200219.add(weeks1).toPlainDateTime(),
  1920, 2, "M02", 26, 12, 34, 0, 0, 0, 0, "add 1w",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  date19201223.add(weeks1).toPlainDateTime(),
  1920, 12, "M12", 30, 12, 34, 0, 0, 0, 0, "add 1w with result on the last day of the year",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M12", day: 24, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(weeks1).toPlainDateTime(),
  1921, 1, "M01", 1, 12, 34, 0, 0, 0, 0, "add 1w with result on the first day of the next year",
  "shaka", 1921);

TemporalHelpers.assertPlainDateTime(
  date19200627.add(weeks1).toPlainDateTime(),
  1920, 7, "M07", 3, 12, 34, 0, 0, 0, 0, "add 1w in a 31-day month with result in the next month",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  date19200727.add(weeks1).toPlainDateTime(),
  1920, 8, "M08", 4, 12, 34, 0, 0, 0, 0, "add 1w in a 30-day month with result in the next month",
  "shaka", 1920);

TemporalHelpers.assertPlainDateTime(
  date19200127.add(weeks6).toPlainDateTime(),
  1920, 3, "M03", 8, 12, 34, 0, 0, 0, 0, "add 6w with result in the same year",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  date19201223.add(weeks6).toPlainDateTime(),
  1921, 2, "M02", 5, 12, 34, 0, 0, 0, 0, "add 6w with result in the next year",
  "shaka", 1921);
TemporalHelpers.assertPlainDateTime(
  date19200127.add(weeks6).toPlainDateTime(),
  1920, 3, "M03", 8, 12, 34, 0, 0, 0, 0, "add 6w crossing months of 30 and 31 days",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  date19200527.add(weeks6).toPlainDateTime(),
  1920, 7, "M07", 7, 12, 34, 0, 0, 0, 0, "add 6w crossing months of 31 and 31 days",
  "shaka", 1920);

TemporalHelpers.assertPlainDateTime(
  date19201228.add(years1weeks2).toPlainDateTime(),
  1922, 1, "M01", 12, 12, 34, 0, 0, 0, 0, "add 1y 2w with result in the next year",
  "shaka", 1922);

TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1918, monthCode: "M10", day: 28, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(months2weeks3).toPlainDateTime(),
  1919, 1, "M01", 19, 12, 34, 0, 0, 0, 0, "add 2mo 3w with result in the next year",
  "shaka", 1919);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1918, monthCode: "M10", day: 31, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(months2weeks3).toPlainDateTime(),
  1919, 1, "M01", 21, 12, 34, 0, 0, 0, 0, "add 2mo 3w with result in the next year",
  "shaka", 1919);

TemporalHelpers.assertPlainDateTime(
  date19200219.add(weeks1n).toPlainDateTime(),
  1920, 2, "M02", 12, 12, 34, 0, 0, 0, 0, "subtract 1w",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M01", day: 8, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(weeks1n).toPlainDateTime(),
  1920, 1, "M01", 1, 12, 34, 0, 0, 0, 0, "subtract 1w with result on the first day of the year",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M01", day: 7, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(weeks1n).toPlainDateTime(),
  1919, 12, "M12", 30, 12, 34, 0, 0, 0, 0, "subtract 1w with result on the last day of the previous year",
  "shaka", 1919);

TemporalHelpers.assertPlainDateTime(
  date19200704.add(weeks1n).toPlainDateTime(),
  1920, 6, "M06", 28, 12, 34, 0, 0, 0, 0, "subtract 1w with result in the previous 31-day month",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M02", day: 3, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(weeks1n).toPlainDateTime(),
  1920, 1, "M01", 26, 12, 34, 0, 0, 0, 0, "subtract 1w with result in the previous 30-day month",
  "shaka", 1920);

TemporalHelpers.assertPlainDateTime(
  date19200604.add(weeks6n).toPlainDateTime(),
  1920, 4, "M04", 24, 12, 34, 0, 0, 0, 0, "subtract 6w with result in the same year",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  date19200127.add(weeks6n).toPlainDateTime(),
  1919, 12, "M12", 15, 12, 34, 0, 0, 0, 0, "subtract 6w with result in the previous year",
  "shaka", 1919);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M07", day: 8, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(weeks6n).toPlainDateTime(),
  1920, 5, "M05", 28, 12, 34, 0, 0, 0, 0, "subtract 6w crossing months of 30 and 31 days",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M03", day: 8, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(weeks6n).toPlainDateTime(),
  1920, 1, "M01", 27, 12, 34, 0, 0, 0, 0, "subtract 6w crossing months of 31 and 31 days",
  "shaka", 1920);

TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1921, monthCode: "M01", day: 5, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(years1weeks2n).toPlainDateTime(),
  1919, 12, "M12", 21, 12, 34, 0, 0, 0, 0, "subtract 1y 2w with result in the previous year",
  "shaka", 1919);

TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1918, monthCode: "M03", day: 2, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(months2weeks3n).toPlainDateTime(),
  1917, 12, "M12", 11, 12, 34, 0, 0, 0, 0, "subtract 2mo 3w with result in the previous year",
  "shaka", 1917);

// Days

const days10 = new Temporal.Duration(0, 0, 0, 10);
const days10n = new Temporal.Duration(0, 0, 0, -10);
const weeks2days3 = new Temporal.Duration(0, 0, 2, 3);
const weeks2days3n = new Temporal.Duration(0, 0, -2, -3);
const years1months2days4 = new Temporal.Duration(1, 2, 0, 4);
const years1months2days4n = new Temporal.Duration(-1, -2, 0, -4);

TemporalHelpers.assertPlainDateTime(
  date19200716.add(days10).toPlainDateTime(),
  1920, 7, "M07", 26, 12, 34, 0, 0, 0, 0, "add 10 days with result in the same month",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M07", day: 26, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(days10).toPlainDateTime(),
  1920, 8, "M08", 6, 12, 34, 0, 0, 0, 0, "add 10 days with result in the next month",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M12", day: 26, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(days10).toPlainDateTime(),
  1921, 1, "M01", 6, 12, 34, 0, 0, 0, 0, "add 10 days with result in the next year",
  "shaka", 1921);

TemporalHelpers.assertPlainDateTime(
  date19201228.add(weeks2days3).toPlainDateTime(),
  1921, 1, "M01", 15, 12, 34, 0, 0, 0, 0, "add 2w 3d with result in the next year",
  "shaka", 1921);

TemporalHelpers.assertPlainDateTime(
  date19200716.add(years1months2days4).toPlainDateTime(),
  1921, 9, "M09", 20, 12, 34, 0, 0, 0, 0, "add 1y 2mo 4d",
  "shaka", 1921);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M11", day: 30, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(years1months2days4).toPlainDateTime(),
  1922, 2, "M02", 3, 12, 34, 0, 0, 0, 0, "add 1y 2mo 4d with result in a month following a 30-day month",
  "shaka", 1922);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M05", day: 26, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(years1months2days4).toPlainDateTime(),
  1921, 7, "M07", 30, 12, 34, 0, 0, 0, 0, "add 1y 2mo 4d with result in a month following a 31-day month",
  "shaka", 1921);

TemporalHelpers.assertPlainDateTime(
  date19200716.add(days10n).toPlainDateTime(),
  1920, 7, "M07", 6, 12, 34, 0, 0, 0, 0, "subtract 10 days with result in the same month",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1920, monthCode: "M07", day: 6, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(days10n).toPlainDateTime(),
  1920, 6, "M06", 27, 12, 34, 0, 0, 0, 0, "subtract 10 days with result in the previous month",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1921, monthCode: "M01", day: 4, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(days10n).toPlainDateTime(),
  1920, 12, "M12", 24, 12, 34, 0, 0, 0, 0, "subtract 10 days with result in the previous year",
  "shaka", 1920);

TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1921, monthCode: "M01", day: 15, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(weeks2days3n).toPlainDateTime(),
  1920, 12, "M12", 28, 12, 34, 0, 0, 0, 0, "subtract 2w 3d with result in the previous year",
  "shaka", 1920);

TemporalHelpers.assertPlainDateTime(
  date19200716.add(years1months2days4n).toPlainDateTime(),
  1919, 5, "M05", 12, 12, 34, 0, 0, 0, 0, "subtract 1y 2mo 4d",
  "shaka", 1919);
TemporalHelpers.assertPlainDateTime(
  Temporal.ZonedDateTime.from({ year: 1921, monthCode: "M12", day: 4, hour: 12, minute: 34, timeZone: "UTC", calendar }).add(years1months2days4n).toPlainDateTime(),
  1920, 9, "M09", 30, 12, 34, 0, 0, 0, 0, "subtract 1y 2mo 4d with result in a 30-day month",
  "shaka", 1920);
TemporalHelpers.assertPlainDateTime(
  date19200604.add(years1months2days4n).toPlainDateTime(),
  1919, 3, "M03", 31, 12, 34, 0, 0, 0, 0, "subtract 1y 2mo 4d with result in a 31-day month",
  "shaka", 1919);
