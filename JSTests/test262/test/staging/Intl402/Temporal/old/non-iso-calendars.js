// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Non-ISO Calendars
features: [Temporal]
---*/

var testChineseData = new Date("2001-02-01T00:00Z").toLocaleString("en-US-u-ca-chinese", {
  day: "numeric",
  month: "numeric",
  year: "numeric",
  era: "short",
  timeZone: "UTC"
});
var hasOutdatedChineseIcuData = !testChineseData.endsWith("2001");

// verify that Intl.DateTimeFormat.formatToParts output matches snapshot data
var getLocalizedDates = isoString => {
  var calendars = [
    "iso8601",
    "buddhist",
    "chinese",
    "coptic",
    "dangi",
    "ethioaa",
    "ethiopic",
    "gregory",
    "hebrew",
    "indian",
    "islamic",
    "islamic-umalqura",
    "islamic-tbla",
    "islamic-civil",
    "islamic-rgsa",
    "islamicc",
    "japanese",
    "persian",
    "roc"
  ];
  var date = new Date(isoString);
  return calendars.map(id => `${ id }: ${ date.toLocaleDateString(`en-US-u-ca-${ id }`, { timeZone: "UTC" }) }`).join("\n");
};
var year2000Content = getLocalizedDates("2000-01-01T00:00Z");
var year1Content = getLocalizedDates("0001-01-01T00:00Z");
var year2000Snapshot = "iso8601: 1/1/2000\n" + "buddhist: 1/1/2543 BE\n" + "chinese: 11/25/1999\n" + "coptic: 4/22/1716 ERA1\n" + "dangi: 11/25/1999\n" + "ethioaa: 4/22/7492 ERA0\n" + "ethiopic: 4/22/1992 ERA1\n" + "gregory: 1/1/2000\n" + "hebrew: 23 Tevet 5760\n" + "indian: 10/11/1921 Saka\n" + "islamic: 9/25/1420 AH\n" + "islamic-umalqura: 9/24/1420 AH\n" + "islamic-tbla: 9/25/1420 AH\n" + "islamic-civil: 9/24/1420 AH\n" + "islamic-rgsa: 9/25/1420 AH\n" + "islamicc: 9/24/1420 AH\n" + "japanese: 1/1/12 H\n" + "persian: 10/11/1378 AP\n" + "roc: 1/1/89 Minguo";
assert.sameValue(year2000Content, year2000Snapshot);
var year1Snapshot = "iso8601: 1/1/1\n" + "buddhist: 1/3/544 BE\n" + "chinese: 11/21/0\n" + "coptic: 5/8/284 ERA0\n" + "dangi: 11/21/0\n" + "ethioaa: 5/8/5493 ERA0\n" + "ethiopic: 5/8/5493 ERA0\n" + "gregory: 1/1/1\n" + "hebrew: 18 Tevet 3761\n" + "indian: 10/11/-78 Saka\n" + "islamic: 5/20/-640 AH\n" + "islamic-umalqura: 5/18/-640 AH\n" + "islamic-tbla: 5/19/-640 AH\n" + "islamic-civil: 5/18/-640 AH\n" + "islamic-rgsa: 5/20/-640 AH\n" + "islamicc: 5/18/-640 AH\n" + "japanese: 1/3/-643 Taika (645\u2013650)\n" + "persian: 10/11/-621 AP\n" + "roc: 1/3/1911 B.R.O.C.";
assert.sameValue(year1Content, year1Snapshot);
var fromWithCases = {
  iso8601: {
    year2000: {
      year: 2000,
      month: 1,
      day: 1
    },
    year1: {
      year: 1,
      month: 1,
      day: 1
    }
  },
  buddhist: {
    year2000: {
      year: 2543,
      month: 1,
      day: 1,
      era: "be"
    },
    year1: RangeError
  },
  chinese: {
    year2000: {
      year: 1999,
      month: 11,
      day: 25
    },
    year1: {
      year: 0,
      month: 12,
      monthCode: "M11",
      day: 21
    }
  },
  coptic: {
    year2000: {
      year: 1716,
      month: 4,
      day: 22,
      era: "era1"
    },
    year1: {
      year: -283,
      eraYear: 284,
      month: 5,
      day: 8,
      era: "era0"
    }
  },
  dangi: {
    year2000: {
      year: 1999,
      month: 11,
      day: 25
    },
    year1: {
      year: 0,
      month: 12,
      monthCode: "M11",
      day: 21
    }
  },
  ethioaa: {
    year2000: {
      year: 7492,
      month: 4,
      day: 22,
      era: "era0"
    },
    year1: {
      year: 5493,
      month: 5,
      day: 8,
      era: "era0"
    }
  },
  ethiopic: {
    year2000: {
      eraYear: 1992,
      year: 7492,
      month: 4,
      day: 22,
      era: "era1"
    },
    year1: {
      year: 5493,
      month: 5,
      day: 8,
      era: "era0"
    }
  },
  gregory: {
    year2000: {
      year: 2000,
      month: 1,
      day: 1,
      era: "ce"
    },
    year1: {
      year: 1,
      month: 1,
      day: 1,
      era: "ce"
    }
  },
  hebrew: {
    year2000: {
      year: 5760,
      month: 4,
      day: 23
    },
    year1: {
      year: 3761,
      month: 4,
      day: 18
    }
  },
  indian: {
    year2000: {
      year: 1921,
      month: 10,
      day: 11,
      era: "saka"
    },
    year1: {
      year: -78,
      month: 10,
      day: 11,
      era: "saka"
    }
  },
  islamic: {
    year2000: {
      year: 1420,
      month: 9,
      day: 25,
      era: "ah"
    },
    year1: {
      year: -640,
      month: 5,
      day: 20,
      era: "ah"
    }
  },
  "islamic-umalqura": {
    year2000: {
      year: 1420,
      month: 9,
      day: 24,
      era: "ah"
    },
    year1: {
      year: -640,
      month: 5,
      day: 18,
      era: "ah"
    }
  },
  "islamic-tbla": {
    year2000: {
      year: 1420,
      month: 9,
      day: 25,
      era: "ah"
    },
    year1: {
      year: -640,
      month: 5,
      day: 19,
      era: "ah"
    }
  },
  "islamic-civil": {
    year2000: {
      year: 1420,
      month: 9,
      day: 24,
      era: "ah"
    },
    year1: {
      year: -640,
      month: 5,
      day: 18,
      era: "ah"
    }
  },
  "islamic-rgsa": {
    year2000: {
      year: 1420,
      month: 9,
      day: 25,
      era: "ah"
    },
    year1: {
      year: -640,
      month: 5,
      day: 20,
      era: "ah"
    }
  },
  islamicc: {
    year2000: {
      year: 1420,
      month: 9,
      day: 24,
      era: "ah"
    },
    year1: {
      year: -640,
      month: 5,
      day: 18,
      era: "ah"
    }
  },
  japanese: {
    year2000: {
      year: 2000,
      eraYear: 12,
      month: 1,
      day: 1,
      era: "heisei"
    },
    year1: RangeError
  },
  persian: {
    year2000: {
      year: 1378,
      month: 10,
      day: 11,
      era: "ap"
    },
    year1: {
      year: -621,
      month: 10,
      day: 11,
      era: "ap"
    }
  },
  roc: {
    year2000: {
      year: 89,
      month: 1,
      day: 1,
      era: "minguo"
    },
    year1: RangeError
  }
};
var logPerf = false;
var totalNow = 0;
for (var [id, tests] of Object.entries(fromWithCases)) {
  var dates = {
    year2000: Temporal.PlainDate.from("2000-01-01"),
    year1: Temporal.PlainDate.from("0001-01-01")
  };
  for (var [name, date] of Object.entries(dates)) {
    var values = tests[name];
    var errorExpected = values === RangeError;
    if ((id === "chinese" || id === "dangi") && hasOutdatedChineseIcuData ) {
      var now = globalThis.performance ? globalThis.performance.now() : Date.now();
      if (errorExpected) {
        assert.throws(RangeError, () => {
          var inCal = date.withCalendar(id);
          Temporal.PlainDate.from({
            calendar: id,
            year: inCal.year,
            day: inCal.day,
            monthCode: inCal.monthCode
          });
        });
      }
      var inCal = date.withCalendar(id);
      assert.sameValue(`${ name } ${ id } day: ${ inCal.day }`, `${ name } ${ id } day: ${ values.day }`);
      if (values.eraYear === undefined && values.era !== undefined)
        values.eraYear = values.year;
      assert.sameValue(`${ name } ${ id } eraYear: ${ inCal.eraYear }`, `${ name } ${ id } eraYear: ${ values.eraYear }`);
      assert.sameValue(`${ name } ${ id } era: ${ inCal.era }`, `${ name } ${ id } era: ${ values.era }`);
      assert.sameValue(`${ name } ${ id } year: ${ inCal.year }`, `${ name } ${ id } year: ${ values.year }`);
      assert.sameValue(`${ name } ${ id } month: ${ inCal.month }`, `${ name } ${ id } month: ${ values.month }`);
      if (values.monthCode === undefined)
        values.monthCode = `M${ values.month.toString().padStart(2, "0") }`;
      assert.sameValue(`${ name } ${ id } monthCode: ${ inCal.monthCode }`, `${ name } ${ id } monthCode: ${ values.monthCode }`);
      if (values.era) {
        var dateRoundtrip1 = Temporal.PlainDate.from({
          calendar: id,
          eraYear: values.eraYear,
          era: values.era,
          day: values.day,
          monthCode: values.monthCode
        });
        assert.sameValue(dateRoundtrip1.toString(), inCal.toString());
        assert.throws(RangeError, () => Temporal.PlainDate.from({
          calendar: id,
          eraYear: values.eraYear,
          era: values.era,
          day: values.day,
          monthCode: values.monthCode,
          year: values.year + 1
        }));
      }
      var dateRoundtrip2 = Temporal.PlainDate.from({
        calendar: id,
        year: values.year,
        day: values.day,
        monthCode: values.monthCode
      });
      assert.sameValue(dateRoundtrip2.toString(), inCal.toString());
      var dateRoundtrip3 = Temporal.PlainDate.from({
        calendar: id,
        year: values.year,
        day: values.day,
        month: values.month
      });
      assert.sameValue(dateRoundtrip3.toString(), inCal.toString());
      var dateRoundtrip4 = Temporal.PlainDate.from({
        calendar: id,
        year: values.year,
        day: values.day,
        monthCode: values.monthCode
      });
      assert.sameValue(dateRoundtrip4.toString(), inCal.toString());
      assert.throws(RangeError, () => Temporal.PlainDate.from({
        calendar: id,
        day: values.day,
        month: values.month === 1 ? 2 : values.month - 1,
        monthCode: values.monthCode,
        year: values.year
      }));
      var ms = (globalThis.performance ? globalThis.performance.now() : Date.now()) - now;
      totalNow += ms;
      if (logPerf)
        console.log(`from: ${ id } ${ name }: ${ ms.toFixed(2) }ms, total: ${ totalNow.toFixed(2) }ms`);
    };
    if ((id === "chinese" || id === "dangi") && hasOutdatedChineseIcuData ) {
      var now = globalThis.performance ? globalThis.performance.now() : Date.now();
      var inCal = date.withCalendar(id);
      if (errorExpected) {
        assert.throws(RangeError, () => inCal.with({ day: 1 }).year);
      }
      var afterWithDay = inCal.with({ day: 1 });
      var t = "(after setting day)";
      assert.sameValue(`${ t } year: ${ afterWithDay.year }`, `${ t } year: ${ inCal.year }`);
      assert.sameValue(`${ t } month: ${ afterWithDay.month }`, `${ t } month: ${ inCal.month }`);
      assert.sameValue(`${ t } day: ${ afterWithDay.day }`, `${ t } day: 1`);
      var afterWithMonth = afterWithDay.with({ month: 1 });
      t = "(after setting month)";
      assert.sameValue(`${ t } year: ${ afterWithMonth.year }`, `${ t } year: ${ inCal.year }`);
      assert.sameValue(`${ t } month: ${ afterWithMonth.month }`, `${ t } month: 1`);
      assert.sameValue(`${ t } day: ${ afterWithMonth.day }`, `${ t } day: 1`);
      var afterWithYear = afterWithMonth.with({ year: 2220 });
      t = "(after setting year)";
      assert.sameValue(`${ t } year: ${ afterWithYear.year }`, `${ t } year: 2220`);
      assert.sameValue(`${ t } month: ${ afterWithYear.month }`, `${ t } month: 1`);
      assert.sameValue(`${ t } day: ${ afterWithYear.day }`, `${ t } day: 1`);
      var ms = (globalThis.performance ? globalThis.performance.now() : Date.now()) - now;
      totalNow += ms;
      if (logPerf)
        console.log(`with: ${ id } ${ name }: ${ ms.toFixed(2) }ms, total: ${ totalNow.toFixed(2) }ms`);
    };
  }
}
var addDaysWeeksCases = {
  iso8601: {
    year: 2000,
    month: 10,
    day: 7,
    monthCode: "M10",
    eraYear: undefined,
    era: undefined
  },
  buddhist: RangeError,
  chinese: {
    year: 2000,
    month: 10,
    day: 16,
    monthCode: "M10",
    eraYear: undefined,
    era: undefined
  },
  coptic: {
    year: 2000,
    month: 10,
    day: 11,
    monthCode: "M10",
    eraYear: 2000,
    era: "era1"
  },
  dangi: {
    year: 2000,
    month: 10,
    day: 16,
    monthCode: "M10",
    eraYear: undefined,
    era: undefined
  },
  ethioaa: {
    year: 2000,
    month: 10,
    day: 11,
    monthCode: "M10",
    eraYear: 2000,
    era: "era0"
  },
  ethiopic: {
    year: 2000,
    month: 10,
    day: 11,
    monthCode: "M10",
    eraYear: 2000,
    era: "era0"
  },
  gregory: {
    year: 2000,
    month: 10,
    day: 7,
    monthCode: "M10",
    eraYear: 2000,
    era: "ce"
  },
  hebrew: {
    year: 2000,
    month: 10,
    day: 14,
    monthCode: "M10",
    eraYear: undefined,
    era: undefined
  },
  indian: {
    year: 2000,
    month: 10,
    day: 6,
    monthCode: "M10",
    eraYear: 2000,
    era: "saka"
  },
  islamic: {
    year: 2000,
    month: 10,
    day: 15,
    monthCode: "M10",
    eraYear: 2000,
    era: "ah"
  },
  "islamic-umalqura": {
    year: 2000,
    month: 10,
    day: 15,
    monthCode: "M10",
    eraYear: 2000,
    era: "ah"
  },
  "islamic-tbla": {
    year: 2000,
    month: 10,
    day: 15,
    monthCode: "M10",
    eraYear: 2000,
    era: "ah"
  },
  "islamic-civil": {
    year: 2000,
    month: 10,
    day: 15,
    monthCode: "M10",
    eraYear: 2000,
    era: "ah"
  },
  "islamic-rgsa": {
    year: 2000,
    month: 10,
    day: 15,
    monthCode: "M10",
    eraYear: 2000,
    era: "ah"
  },
  islamicc: {
    year: 2000,
    month: 10,
    day: 15,
    monthCode: "M10",
    eraYear: 2000,
    era: "ah"
  },
  japanese: {
    year: 2000,
    month: 10,
    day: 7,
    monthCode: "M10",
    eraYear: 12,
    era: "heisei"
  },
  persian: {
    year: 2000,
    month: 10,
    day: 5,
    monthCode: "M10",
    eraYear: 2000,
    era: "ap"
  },
  roc: {
    year: 2000,
    month: 10,
    day: 8,
    monthCode: "M10",
    eraYear: 2000,
    era: "minguo"
  }
};
const addMonthsCases = {
  iso8601: { year: 2001, month: 6, day: 1, monthCode: 'M06', eraYear: undefined, era: undefined },
  // See https://bugs.chromium.org/p/chromium/issues/detail?id=1173158
  buddhist: RangeError, // { year: 2001, month: 6, day: 1, monthCode: 'M06', eraYear: 2001, era: 'be' },
  chinese: { year: 2001, month: 6, day: 1, monthCode: 'M05', eraYear: undefined, era: undefined },
  coptic: { year: 2001, month: 5, day: 1, monthCode: 'M05', eraYear: 2001, era: 'era1' },
  dangi: { year: 2001, month: 6, day: 1, monthCode: 'M05', eraYear: undefined, era: undefined },
  ethioaa: { year: 2001, month: 5, day: 1, monthCode: 'M05', eraYear: 2001, era: 'era0' },
  ethiopic: { year: 2001, month: 5, day: 1, monthCode: 'M05', eraYear: 2001, era: 'era0' },
  gregory: { year: 2001, month: 6, day: 1, monthCode: 'M06', eraYear: 2001, era: 'ce' },
  hebrew: { year: 2001, month: 6, day: 1, monthCode: 'M05L', eraYear: undefined, era: undefined },
  indian: { year: 2001, month: 6, day: 1, monthCode: 'M06', eraYear: 2001, era: 'saka' },
  islamic: { year: 2001, month: 6, day: 1, monthCode: 'M06', eraYear: 2001, era: 'ah' },
  'islamic-umalqura': { year: 2001, month: 6, day: 1, monthCode: 'M06', eraYear: 2001, era: 'ah' },
  'islamic-tbla': { year: 2001, month: 6, day: 1, monthCode: 'M06', eraYear: 2001, era: 'ah' },
  'islamic-civil': { year: 2001, month: 6, day: 1, monthCode: 'M06', eraYear: 2001, era: 'ah' },
  'islamic-rgsa': { year: 2001, month: 6, day: 1, monthCode: 'M06', eraYear: 2001, era: 'ah' },
  islamicc: { year: 2001, month: 6, day: 1, monthCode: 'M06', eraYear: 2001, era: 'ah' },
  japanese: { year: 2001, month: 6, day: 1, monthCode: 'M06', eraYear: 13, era: 'heisei' },
  persian: { year: 2001, month: 6, day: 1, monthCode: 'M06', eraYear: 2001, era: 'ap' },
  roc: { year: 2001, month: 6, day: 1, monthCode: 'M06', eraYear: 2001, era: 'minguo' }
};
var i = 0;
const addYearsMonthsDaysCases = Object.entries(addMonthsCases).reduce((obj, entry) => {
  obj[entry[0]] = entry[1] === RangeError ? RangeError : { ...entry[1], day: 18 };
  return obj;
}, {});
var tests = {
  days: {
    duration: { days: 280 },
    results: addDaysWeeksCases,
    startDate: {
      year: 2000,
      month: 1,
      day: 1
    }
  },
  weeks: {
    duration: { weeks: 40 },
    results: addDaysWeeksCases,
    startDate: {
      year: 2000,
      month: 1,
      day: 1
    }
  },
  months: {
    duration: { months: 6 },
    results: addMonthsCases,
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
    results: addYearsMonthsDaysCases,
    startDate: {
      year: 1997,
      monthCode: "M12",
      day: 1
    }
  }
};
var calendars = Object.keys(addMonthsCases);
for (var id of calendars) {
  for (var [unit, {duration, results, startDate}] of Object.entries(tests)) {
    var values = results[id];
    duration = Temporal.Duration.from(duration);
    if ((id === "chinese" || id === "dangi") && hasOutdatedChineseIcuData ) {
      var now = globalThis.performance ? globalThis.performance.now() : Date.now();
      if (values === RangeError) {
        assert.throws(RangeError, () => Temporal.PlainDate.from({
          ...startDate,
          calendar: id
        }));
      }
      var start = Temporal.PlainDate.from({
        ...startDate,
        calendar: id
      });
      var end = start.add(duration);
      assert.sameValue(`add ${ unit } ${ id } day: ${ end.day }`, `add ${ unit } ${ id } day: ${ values.day }`);
      assert.sameValue(`add ${ unit } ${ id } eraYear: ${ end.eraYear }`, `add ${ unit } ${ id } eraYear: ${ values.eraYear }`);
      assert.sameValue(`add ${ unit } ${ id } era: ${ end.era }`, `add ${ unit } ${ id } era: ${ values.era }`);
      assert.sameValue(`add ${ unit } ${ id } year: ${ end.year }`, `add ${ unit } ${ id } year: ${ values.year }`);
      assert.sameValue(`add ${ unit } ${ id } month: ${ end.month }`, `add ${ unit } ${ id } month: ${ values.month }`);
      assert.sameValue(`add ${ unit } ${ id } monthCode: ${ end.monthCode }`, `add ${ unit } ${ id } monthCode: ${ values.monthCode }`);
      var calculatedStart = end.subtract(duration);
      var isLunisolar = [
        "chinese",
        "dangi",
        "hebrew"
      ].includes(id);
      var expectedCalculatedStart = isLunisolar && duration.years !== 0 && !end.monthCode.endsWith("L") ? start.subtract({ months: 1 }) : start;
      assert.sameValue(`start ${ calculatedStart.toString() }`, `start ${ expectedCalculatedStart.toString() }`);
      var diff = start.until(end, { largestUnit: unit });
      assert.sameValue(`diff ${ unit } ${ id }: ${ diff }`, `diff ${ unit } ${ id }: ${ duration }`);
      if (unit === "months") {
        var startYesterday = start.subtract({ days: 1 });
        var endYesterday = startYesterday.add(duration);
        assert.sameValue(`add from end-of-month ${ unit } ${ id } day (initial): ${ endYesterday.day }`, `add from end-of-month ${ unit } ${ id } day (initial): ${ Math.min(startYesterday.day, endYesterday.daysInMonth) }`);
        var endYesterdayNextDay = endYesterday.add({ days: 1 });
        while (endYesterdayNextDay.day !== 1) {
          endYesterdayNextDay = endYesterdayNextDay.add({ days: 1 });
        }
        assert.sameValue(`add from end-of-month ${ unit } ${ id } day: ${ endYesterdayNextDay.day }`, `add from end-of-month ${ unit } ${ id } day: ${ values.day }`);
        assert.sameValue(`add from end-of-month ${ unit } ${ id } eraYear: ${ endYesterdayNextDay.eraYear }`, `add from end-of-month ${ unit } ${ id } eraYear: ${ values.eraYear }`);
        assert.sameValue(`add from end-of-month ${ unit } ${ id } era: ${ endYesterdayNextDay.era }`, `add from end-of-month ${ unit } ${ id } era: ${ values.era }`);
        assert.sameValue(`add from end-of-month ${ unit } ${ id } year: ${ endYesterdayNextDay.year }`, `add from end-of-month ${ unit } ${ id } year: ${ values.year }`);
        assert.sameValue(`add from end-of-month ${ unit } ${ id } month: ${ endYesterdayNextDay.month }`, `add from end-of-month ${ unit } ${ id } month: ${ values.month }`);
        assert.sameValue(`add from end-of-month ${ unit } ${ id } monthCode: ${ endYesterdayNextDay.monthCode }`, `add from end-of-month ${ unit } ${ id } monthCode: ${ values.monthCode }`);
        var endReverse = endYesterdayNextDay.subtract({ days: 1 });
        var startReverse = endReverse.subtract(duration);
        assert.sameValue(`subtract from end-of-month ${ unit } ${ id } day (initial): ${ startReverse.day }`, `subtract from end-of-month ${ unit } ${ id } day (initial): ${ Math.min(endReverse.day, startReverse.daysInMonth) }`);
        var startReverseNextDay = startReverse.add({ days: 1 });
        while (startReverseNextDay.day !== 1) {
          startReverseNextDay = startReverseNextDay.add({ days: 1 });
        }
        assert.sameValue(`subtract from end-of-month ${ unit } ${ id } day: ${ startReverseNextDay.day }`, `subtract from end-of-month ${ unit } ${ id } day: ${ start.day }`);
        assert.sameValue(`subtract from end-of-month ${ unit } ${ id } eraYear: ${ startReverseNextDay.eraYear }`, `subtract from end-of-month ${ unit } ${ id } eraYear: ${ start.eraYear }`);
        assert.sameValue(`subtract from end-of-month ${ unit } ${ id } era: ${ startReverseNextDay.era }`, `subtract from end-of-month ${ unit } ${ id } era: ${ start.era }`);
        assert.sameValue(`subtract from end-of-month ${ unit } ${ id } year: ${ startReverseNextDay.year }`, `subtract from end-of-month ${ unit } ${ id } year: ${ start.year }`);
        assert.sameValue(`subtract from end-of-month ${ unit } ${ id } month: ${ startReverseNextDay.month }`, `subtract from end-of-month ${ unit } ${ id } month: ${ start.month }`);
        assert.sameValue(`subtract from end-of-month ${ unit } ${ id } monthCode: ${ startReverseNextDay.monthCode }`, `subtract from end-of-month ${ unit } ${ id } monthCode: ${ start.monthCode }`);
      }
      var ms = (globalThis.performance ? globalThis.performance.now() : Date.now()) - now;
      totalNow += ms;
      if (logPerf)
        console.log(`${ id } add ${ duration }: ${ ms.toFixed(2) }ms, total: ${ totalNow.toFixed(2) }ms`);
    };
  }
}
var daysInMonthCases = {
  iso8601: {
    year: 2001,
    leap: false,
    days: [
      31,
      28,
      31,
      30,
      31,
      30,
      31,
      31,
      30,
      31,
      30,
      31
    ]
  },
  buddhist: {
    year: 4001,
    leap: false,
    days: [
      31,
      28,
      31,
      30,
      31,
      30,
      31,
      31,
      30,
      31,
      30,
      31
    ]
  },
  chinese: {
    year: 2001,
    leap: "M04L",
    days: [
      30,
      30,
      29,
      30,
      29,
      30,
      29,
      29,
      30,
      29,
      30,
      29,
      30
    ]
  },
  coptic: {
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
  dangi: {
    year: 2001,
    leap: "M04L",
    days: [
      30,
      30,
      30,
      29,
      29,
      30,
      29,
      29,
      30,
      29,
      30,
      29,
      30
    ]
  },
  ethioaa: {
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
  ethiopic: {
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
  gregory: {
    year: 2001,
    leap: false,
    days: [
      31,
      28,
      31,
      30,
      31,
      30,
      31,
      31,
      30,
      31,
      30,
      31
    ]
  },
  hebrew: {
    year: 2001,
    leap: "M05L",
    days: [
      30,
      30,
      30,
      29,
      30,
      30,
      29,
      30,
      29,
      30,
      29,
      30,
      29
    ]
  },
  indian: {
    year: 2001,
    leap: false,
    days: [
      30,
      31,
      31,
      31,
      31,
      31,
      30,
      30,
      30,
      30,
      30,
      30
    ]
  },
  islamic: {
    year: 2001,
    leap: false,
    days: [
      29,
      30,
      29,
      29,
      30,
      29,
      30,
      30,
      29,
      30,
      30,
      29
    ]
  },
  "islamic-umalqura": {
    year: 2001,
    leap: true,
    days: [
      30,
      29,
      30,
      29,
      30,
      29,
      30,
      29,
      30,
      29,
      30,
      30
    ]
  },
  "islamic-tbla": {
    year: 2001,
    leap: true,
    days: [
      30,
      29,
      30,
      29,
      30,
      29,
      30,
      29,
      30,
      29,
      30,
      30
    ]
  },
  "islamic-civil": {
    year: 2001,
    leap: true,
    days: [
      30,
      29,
      30,
      29,
      30,
      29,
      30,
      29,
      30,
      29,
      30,
      30
    ]
  },
  "islamic-rgsa": {
    year: 2001,
    leap: false,
    days: [
      29,
      30,
      29,
      29,
      30,
      29,
      30,
      30,
      29,
      30,
      30,
      29
    ]
  },
  islamicc: {
    year: 2001,
    leap: true,
    days: [
      30,
      29,
      30,
      29,
      30,
      29,
      30,
      29,
      30,
      29,
      30,
      30
    ]
  },
  japanese: {
    year: 2001,
    leap: false,
    days: [
      31,
      28,
      31,
      30,
      31,
      30,
      31,
      31,
      30,
      31,
      30,
      31
    ]
  },
  persian: {
    year: 2001,
    leap: false,
    days: [
      31,
      31,
      31,
      31,
      31,
      31,
      30,
      30,
      30,
      30,
      30,
      29
    ]
  },
  roc: {
    year: 2001,
    leap: true,
    days: [
      31,
      29,
      31,
      30,
      31,
      30,
      31,
      31,
      30,
      31,
      30,
      31
    ]
  }
};
totalNow = 0;
for (var id of calendars) {
  var {year, leap, days} = daysInMonthCases[id];
  var date = hasOutdatedChineseIcuData && (id === "chinese" || id === "dangi") ? undefined : Temporal.PlainDate.from({
    year,
    month: 1,
    day: 1,
    calendar: id
  });
  if ((id === "chinese" || id === "dangi") && hasOutdatedChineseIcuData ) {
    if (typeof leap === "boolean") {
      assert.sameValue(date.inLeapYear, leap);
    } else {
      assert.sameValue(date.inLeapYear, true);
      var leapMonth = date.with({ monthCode: leap });
      assert.sameValue(leapMonth.monthCode, leap);
    }
  };
  if ((id === "chinese" || id === "dangi") && hasOutdatedChineseIcuData ) {
    var now = globalThis.performance ? globalThis.performance.now() : Date.now();
    var {monthsInYear} = date;
    assert.sameValue(monthsInYear, days.length);
    for (var i = monthsInYear, leapMonthIndex = undefined, monthStart = undefined; i >= 1; i--) {
      monthStart = monthStart ? monthStart.add({ months: -1 }) : date.add({ months: monthsInYear - 1 });
      var {month, monthCode, daysInMonth} = monthStart;
      assert.sameValue(`${ id } month ${ i } (code ${ monthCode }) days: ${ daysInMonth }`, `${ id } month ${ i } (code ${ monthCode }) days: ${ days[i - 1] }`);
      if (monthCode.endsWith("L")) {
        assert.sameValue(date.with({ monthCode }).monthCode, monthCode);
        leapMonthIndex = i;
      } else {
        if (leapMonthIndex && i === leapMonthIndex - 1) {
          var inLeapMonth = monthStart.with({ monthCode: `M${ month.toString().padStart(2, "0") }L` });
          assert.sameValue(inLeapMonth.monthCode, `${ monthCode }L`);
        } else {
          assert.throws(RangeError, () => monthStart.with({ monthCode: `M${ month.toString().padStart(2, "0") }L` }, { overflow: "reject" }));
          if ([
              "chinese",
              "dangi"
            ].includes(id)) {
            if (i === 1 || i === 12 || i === 13) {
              assert.throws(RangeError, () => monthStart.with({ monthCode: `M${ month.toString().padStart(2, "0") }L` }));
            } else {
              var fakeL = monthStart.with({
                monthCode: `M${ month.toString().padStart(2, "0") }L`,
                day: 5
              });
              assert.sameValue(`fake leap month ${ fakeL.monthCode }`, `fake leap month M${ month.toString().padStart(2, "0") }`);
              assert.sameValue(fakeL.day, fakeL.daysInMonth);
            }
          }
        }
        if (![
            "chinese",
            "dangi",
            "hebrew"
          ].includes(id)) {
          assert.throws(RangeError, () => monthStart.with({ monthCode: `M${ month.toString().padStart(2, "0") }L` }));
        }
      }
      assert.throws(RangeError, () => monthStart.with({ day: daysInMonth + 1 }, { overflow: "reject" }));
      var oneDayPastMonthEnd = monthStart.with({ day: daysInMonth + 1 });
      assert.sameValue(oneDayPastMonthEnd.day, daysInMonth);
    }
    var ms = (globalThis.performance ? globalThis.performance.now() : Date.now()) - now;
    totalNow += ms;
    if (logPerf)
      console.log(`${ id } months check ${ id }: ${ ms.toFixed(2) }ms, total: ${ totalNow.toFixed(2) }ms`);
  };
}
var monthDayCases = [
  {
    calendar: "iso8601",
    isoReferenceYear: 1972,
    year: 2004,
    month: 2,
    day: 29
  },
  {
    calendar: "buddhist",
    isoReferenceYear: 1972,
    year: 2004,
    month: 2,
    day: 29
  },
  {
    calendar: "chinese",
    isoReferenceYear: 1963,
    year: 2001,
    month: 5,
    monthCode: "M04L",
    day: 15
  },
  {
    calendar: "chinese",
    isoReferenceYear: 1971,
    year: 2000,
    month: 6,
    day: 30
  },
  {
    calendar: "coptic",
    isoReferenceYear: 1971,
    year: 2006,
    month: 13,
    day: 6
  },
  {
    calendar: "dangi",
    isoReferenceYear: 1963,
    year: 2001,
    month: 5,
    monthCode: "M04L",
    day: 15
  },
  {
    calendar: "dangi",
    isoReferenceYear: 1971,
    year: 2000,
    month: 6,
    day: 30
  },
  {
    calendar: "ethiopic",
    isoReferenceYear: 1971,
    year: 2006,
    month: 13,
    day: 6
  },
  {
    calendar: "ethioaa",
    isoReferenceYear: 1971,
    year: 2006,
    month: 13,
    day: 6
  },
  {
    calendar: "gregory",
    isoReferenceYear: 1972,
    year: 2004,
    month: 2,
    day: 29
  },
  {
    calendar: "hebrew",
    isoReferenceYear: 1970,
    year: 5779,
    month: 6,
    monthCode: "M05L",
    day: 15
  },
  {
    calendar: "hebrew",
    isoReferenceYear: 1971,
    year: 5776,
    month: 2,
    day: 30
  },
  {
    calendar: "hebrew",
    isoReferenceYear: 1971,
    year: 5772,
    month: 3,
    day: 30
  },
  {
    calendar: "indian",
    isoReferenceYear: 1968,
    year: 2002,
    month: 1,
    day: 31
  },
  {
    calendar: "islamic",
    isoReferenceYear: 1970,
    year: 2001,
    month: 1,
    day: 30
  },
  {
    calendar: "islamic-umalqura",
    isoReferenceYear: 1969,
    year: 2001,
    month: 1,
    day: 30
  },
  {
    calendar: "islamic-tbla",
    isoReferenceYear: 1971,
    year: 2001,
    month: 1,
    day: 30
  },
  {
    calendar: "islamic-civil",
    isoReferenceYear: 1971,
    year: 2001,
    month: 1,
    day: 30
  },
  {
    calendar: "islamic-rgsa",
    isoReferenceYear: 1970,
    year: 2001,
    month: 1,
    day: 30
  },
  {
    calendar: "islamicc",
    isoReferenceYear: 1971,
    year: 2001,
    month: 1,
    day: 30
  },
  {
    calendar: "japanese",
    isoReferenceYear: 1972,
    year: 2004,
    month: 2,
    day: 29
  },
  {
    calendar: "persian",
    isoReferenceYear: 1972,
    year: 2004,
    month: 12,
    day: 30
  },
  {
    calendar: "roc",
    isoReferenceYear: 1972,
    year: 93,
    month: 2,
    day: 29
  }
];
var i = 0;
for (var test of monthDayCases) {
  var id = test.calendar;
  if ((id === "chinese" || id === "dangi") && hasOutdatedChineseIcuData ) {
    if (test.monthCode === undefined)
      test.monthCode = `M${ test.month.toString().padStart(2, "0") }`;
    var {calendar, monthCode, month, day, year, isoReferenceYear} = test;
    var md = Temporal.PlainMonthDay.from({
      year,
      month,
      day,
      calendar
    });
    var isoString = md.toString();
    var mdFromIso = Temporal.PlainMonthDay.from(isoString);
    assert.sameValue(mdFromIso, md);
    var isoFields = md.getISOFields();
    assert.sameValue(md.monthCode, monthCode);
    assert.sameValue(md.day, day);
    assert.sameValue(isoFields.isoYear, isoReferenceYear);
    var md2 = Temporal.PlainMonthDay.from({
      monthCode,
      day,
      calendar
    });
    var isoFields2 = md2.getISOFields();
    assert.sameValue(md2.monthCode, monthCode);
    assert.sameValue(md2.day, day);
    assert.sameValue(isoFields2.isoYear, isoReferenceYear);
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
    var {monthCode: monthCodeConstrained} = constrained;
    assert(monthCodeConstrained === "M12" || monthCodeConstrained === "M13");
  };
}
