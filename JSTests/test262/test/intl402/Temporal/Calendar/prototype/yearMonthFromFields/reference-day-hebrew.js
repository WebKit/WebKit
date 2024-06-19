// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearmonthfromfields
description: Reference ISO day is chosen to be the first of the calendar month
info: |
  6.d. Perform ! CreateDataPropertyOrThrow(_fields_, *"day"*, *1*<sub>𝔽</sub>).
    e. Let _result_ be ? CalendarDateToISO(_calendar_.[[Identifier]], _fields_, _options_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const hebrew = new Temporal.Calendar("hebrew");

const result4 = hebrew.yearMonthFromFields({ year: 5782, monthCode: "M04", day: 20 });
TemporalHelpers.assertPlainYearMonth(
  result4,
  5782, 4, "M04",
  "reference day is the first of the calendar month even if day is given",
  /* era = */ undefined, /* era year = */ undefined, /* reference day = */ 5
);
const isoFields = result4.getISOFields();
assert.sameValue(isoFields.isoYear, 2021, "Tevet 5782 begins in ISO year 2021");
assert.sameValue(isoFields.isoMonth, 12, "Tevet 5782 begins in ISO month 12");

const result5 = hebrew.yearMonthFromFields({ year: 5783, monthCode: "M05L" }, { overflow: "constrain" });
TemporalHelpers.assertPlainYearMonth(
  result5,
  5783, 6, "M06",
  "month code M05L does not exist in year 5783 (overflow constrain); Hebrew calendar constrains Adar I to Adar",
  /* era = */ undefined, /* era year = */ undefined, /* reference day = */ 22
);

assert.throws(
  RangeError,
  () => hebrew.yearMonthFromFields({ year: 5783, monthCode: "M05L" }, { overflow: "reject" }),
  "month code M05L does not exist in year 5783 (overflow reject)",
);

const result6 = hebrew.yearMonthFromFields({ year: 5783, month: 13 }, { overflow: "constrain" });
TemporalHelpers.assertPlainYearMonth(
  result6,
  5783, 12, "M12",
  "month 13 does not exist in year 5783 (overflow constrain)",
  /* era = */ undefined, /* era year = */ undefined, /* reference day = */ 18
);

assert.throws(
  RangeError,
  () => hebrew.yearMonthFromFields({ year: 5783, month: 13 }, { overflow: "reject" }),
  "month 13 does not exist in year 5783 (overflow reject)",
);

const result7 = hebrew.yearMonthFromFields({ year: 5782, monthCode: "M04", day: 50 }, { overflow: "constrain" });
TemporalHelpers.assertPlainYearMonth(
  result7,
  5782, 4, "M04",
  "reference day is set correctly even if day is out of range (overflow constrain)",
  /* era = */ undefined, /* era year = */ undefined, /* reference day = */ 5
);

const result8 = hebrew.yearMonthFromFields({ year: 5782, monthCode: "M04", day: 50 }, { overflow: "reject" });
TemporalHelpers.assertPlainYearMonth(
  result8,
  5782, 4, "M04",
  "reference day is set correctly even if day is out of range (overflow reject)",
  /* era = */ undefined, /* era year = */ undefined, /* reference day = */ 5
);
