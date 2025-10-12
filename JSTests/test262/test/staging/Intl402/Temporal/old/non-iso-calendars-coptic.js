// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Non-ISO Calendars
features: [Temporal]
locale:
  - en-US-u-ca-coptic
---*/

const calendar = "coptic";

// verify that Intl.DateTimeFormat.formatToParts output matches snapshot data
function compareFormatToPartsSnapshot(isoString, expectedComponents) {
  const date = new Date(isoString);
  const formatter = new Intl.DateTimeFormat(`en-US-u-ca-${calendar}`, { timeZone: "UTC" });
  const actualComponents = formatter.formatToParts(date);
  for (let [expectedType, expectedValue] of Object.entries(expectedComponents)) {
    const part = actualComponents.find(({type}) => type === expectedType);
    const contextMessage = `${expectedType} component of ${isoString} formatted in ${calendar}`;
    assert.notSameValue(part, undefined, contextMessage);
    assert.sameValue(part.value, `${expectedValue}`, contextMessage);
  }
}

compareFormatToPartsSnapshot("2000-01-01T00:00Z", {
  year: 1716,
  era: "ERA1",
  month: 4,
  day: 22,
});

compareFormatToPartsSnapshot("0001-01-01T00:00Z", {
  year: 284,
  era: "ERA0",
  month: 5,
  day: 8,
});

var fromWithCases = {
  year2000: {
    era: "am",
    eraYear: 1716,
    year: 1716,
    month: 4,
    monthCode: "M04",
    day: 22,
  },
  year1: {
    era: "am",
    eraYear: -283,
    year: -283,
    month: 5,
    monthCode: "M05",
    day: 8,
  }
};
var dates = {
  year2000: Temporal.PlainDate.from("2000-01-01"),
  year1: Temporal.PlainDate.from("0001-01-01")
};
for (var [name, result] of Object.entries(fromWithCases)) {
  var date = dates[name];
  var inCal = date.withCalendar(calendar);

  assert.sameValue(inCal.era, result.era, `${name}: era`);
  assert.sameValue(inCal.eraYear, result.eraYear, `${name}: eraYear`);
  assert.sameValue(inCal.year, result.year, `${name}: year`);
  assert.sameValue(inCal.month, result.month, `${name}: month`);
  assert.sameValue(inCal.monthCode, result.monthCode, `${name}: monthCode`);
  assert.sameValue(inCal.day, result.day, `${name}: day`);

  var dateRoundtrip2 = Temporal.PlainDate.from({
    calendar,
    year: result.year,
    day: result.day,
    monthCode: result.monthCode
  });
  assert.sameValue(dateRoundtrip2.toString(), inCal.toString());

  var dateRoundtrip3 = Temporal.PlainDate.from({
    calendar,
    year: result.year,
    day: result.day,
    month: result.month
  });
  assert.sameValue(dateRoundtrip3.toString(), inCal.toString());

  var dateRoundtrip4 = Temporal.PlainDate.from({
    calendar,
    year: result.year,
    day: result.day,
    monthCode: result.monthCode
  });
  assert.sameValue(dateRoundtrip4.toString(), inCal.toString());

  assert.throws(RangeError, () => Temporal.PlainDate.from({
    calendar,
    day: result.day,
    month: result.month === 1 ? 2 : result.month - 1,
    monthCode: result.monthCode,
    year: result.year
  }));

  var afterWithDay = inCal.with({ day: 1 });
  assert.sameValue(afterWithDay.year, inCal.year, `${name} (after setting day)`);
  assert.sameValue(afterWithDay.month, inCal.month, `${name} (after setting day)`);
  assert.sameValue(afterWithDay.day, 1, `${name} (after setting day)`);

  var afterWithMonth = afterWithDay.with({ month: 1 });
  assert.sameValue(afterWithMonth.year, inCal.year, `${name} (after setting month)`);
  assert.sameValue(afterWithMonth.month, 1, `${name} (after setting month)`);
  assert.sameValue(afterWithMonth.day, 1, `${name} (after setting month)`);

  var afterWithYear = afterWithMonth.with({ year: 2220 });
  assert.sameValue(afterWithYear.year, 2220, `${name} (after setting year)`);
  assert.sameValue(afterWithYear.month, 1, `${name} (after setting year)`);
  assert.sameValue(afterWithYear.day, 1, `${name} (after setting year)`);
}

var durationCases = {
  days: {
    duration: { days: 280 },
    result: {
      eraYear: 2000,
      era: "am",
      year: 2000,
      month: 10,
      monthCode: "M10",
      day: 11,
    },
    startDate: {
      year: 2000,
      month: 1,
      day: 1
    }
  },
  weeks: {
    duration: { weeks: 40 },
    result: {
      eraYear: 2000,
      era: "am",
      year: 2000,
      month: 10,
      monthCode: "M10",
      day: 11,
    },
    startDate: {
      year: 2000,
      month: 1,
      day: 1
    }
  },
  months: {
    duration: { months: 6 },
    result: {
      era: "am",
      eraYear: 2001,
      year: 2001,
      month: 5,
      monthCode: "M05",
      day: 1,
    },
    startDate: {
      year: 2000,
      month: 12,
      day: 1
    }
  },
  years: {
    duration: {
      years: 3,
      months: 6,
      days: 17
    },
    result: {
      era: "am",
      eraYear: 2001,
      year: 2001,
      month: 5,
      monthCode: "M05",
      day: 18,
    },
    startDate: {
      year: 1997,
      monthCode: "M12",
      day: 1
    }
  }
};
for (var [unit, {duration, result, startDate}] of Object.entries(durationCases)) {
  duration = Temporal.Duration.from(duration);

  var start = Temporal.PlainDate.from({
    ...startDate,
    calendar
  });

  var end = start.add(duration);
  assert.sameValue(end.era, result.era, `${unit}`);
  assert.sameValue(end.eraYear, result.eraYear, `${unit}`);
  assert.sameValue(end.year, result.year, `${unit}`);
  assert.sameValue(end.month, result.month, `${unit}`);
  assert.sameValue(end.monthCode, result.monthCode, `${unit}`);
  assert.sameValue(end.day, result.day, `${unit}`);

  var calculatedStart = end.subtract(duration);
  var expectedCalculatedStart = start;
  assert.sameValue(calculatedStart.toString(), expectedCalculatedStart.toString(), `${unit}`);

  var diff = start.until(end, { largestUnit: unit });
  assert.sameValue(diff.toString(), duration.toString(), `${unit}`);

  if (unit === "months") {
    var startYesterday = start.subtract({ days: 1 });
    var endYesterday = startYesterday.add(duration);
    assert.sameValue(endYesterday.day, Math.min(startYesterday.day, endYesterday.daysInMonth), `${unit}`);

    var endYesterdayNextDay = endYesterday.add({ days: 1 });
    while (endYesterdayNextDay.day !== 1) {
      endYesterdayNextDay = endYesterdayNextDay.add({ days: 1 });
    }
    assert.sameValue(endYesterdayNextDay.era, result.era, `${unit}`);
    assert.sameValue(endYesterdayNextDay.eraYear, result.eraYear, `${unit}`);
    assert.sameValue(endYesterdayNextDay.year, result.year, `${unit}`);
    assert.sameValue(endYesterdayNextDay.month, result.month, `${unit}`);
    assert.sameValue(endYesterdayNextDay.monthCode, result.monthCode, `${unit}`);
    assert.sameValue(endYesterdayNextDay.day, result.day, `${unit}`);

    var endReverse = endYesterdayNextDay.subtract({ days: 1 });
    var startReverse = endReverse.subtract(duration);
    assert.sameValue(startReverse.day, Math.min(endReverse.day, startReverse.daysInMonth));

    var startReverseNextDay = startReverse.add({ days: 1 });
    while (startReverseNextDay.day !== 1) {
      startReverseNextDay = startReverseNextDay.add({ days: 1 });
    }
    assert.sameValue(startReverseNextDay.era, start.era, `${unit}`);
    assert.sameValue(startReverseNextDay.eraYear, start.eraYear, `${unit}`);
    assert.sameValue(startReverseNextDay.year, start.year, `${unit}`);
    assert.sameValue(startReverseNextDay.month, start.month, `${unit}`);
    assert.sameValue(startReverseNextDay.monthCode, start.monthCode, `${unit}`);
    assert.sameValue(startReverseNextDay.day, start.day, `${unit}`);
  }
}

var daysInMonthCases = [
  {
    year: 2001,
    leap: false,
    days: [
      30,
      30,
      30,
      30,
      30,
      30,
      30,
      30,
      30,
      30,
      30,
      30,
      5
    ]
  },
];
for (var {year, leap, days} of daysInMonthCases) {
  var date = Temporal.PlainDate.from({
    year,
    month: 1,
    day: 1,
    calendar
  });
  assert.sameValue(date.inLeapYear, leap);

  var {monthsInYear} = date;
  assert.sameValue(monthsInYear, days.length);

  for (var i = monthsInYear, monthStart = undefined; i >= 1; i--) {
    monthStart = monthStart ? monthStart.add({ months: -1 }) : date.add({ months: monthsInYear - 1 });

    var {month, monthCode, daysInMonth} = monthStart;
    assert.sameValue(month, i);
    assert.sameValue(daysInMonth, days[i - 1]);

    assert.sameValue(monthCode.endsWith("L"), false);
    assert.throws(RangeError, () => monthStart.with({ monthCode: `M${ month.toString().padStart(2, "0") }L` }));
    assert.throws(RangeError, () => monthStart.with({ monthCode: `M${ month.toString().padStart(2, "0") }L` }, { overflow: "reject" }));

    var oneDayPastMonthEnd = monthStart.with({ day: daysInMonth + 1 });
    assert.sameValue(oneDayPastMonthEnd.day, daysInMonth);
    assert.throws(RangeError, () => monthStart.with({ day: daysInMonth + 1 }, { overflow: "reject" }));
  }
}

var monthDayCases = [
  {
    year: 2006,
    month: 13,
    monthCode: "M13",
    day: 5,
  },
];
for (var {monthCode, month, day, year} of monthDayCases) {
  var md = Temporal.PlainMonthDay.from({
    year,
    month,
    day,
    calendar
  });
  var isoString = md.toString();

  var mdFromIso = Temporal.PlainMonthDay.from(isoString);
  assert.sameValue(mdFromIso.toString(), isoString);
  assert.sameValue(md.monthCode, monthCode);
  assert.sameValue(md.day, day);

  var md2 = Temporal.PlainMonthDay.from({
    monthCode,
    day,
    calendar
  });
  assert.sameValue(md2.monthCode, monthCode);
  assert.sameValue(md2.day, day);
  assert.sameValue(md.equals(md2), true);

  assert.throws(RangeError, () => {
    Temporal.PlainMonthDay.from({
      monthCode: "M15",
      day: 1,
      calendar
    }, { overflow: "reject" });
  });

  assert.throws(RangeError, () => {
    Temporal.PlainMonthDay.from({
      monthCode: "M15",
      day: 1,
      calendar
    });
  });

  assert.throws(RangeError, () => {
    Temporal.PlainMonthDay.from({
      year,
      month: 15,
      day: 1,
      calendar
    }, { overflow: "reject" });
  });

  var constrained = Temporal.PlainMonthDay.from({
    year,
    month: 15,
    day: 1,
    calendar
  });
  assert.sameValue(constrained.monthCode, "M13");
}
