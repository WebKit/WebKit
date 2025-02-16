// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: DateTimeFormat
includes: [deepEqual.js]
features: [Temporal]
---*/

// Tolerate implementation variance by expecting consistency without being prescriptive.
// TODO: can we change tests to be less reliant on CLDR formats while still testing that
// Temporal and Intl are behaving as expected?
const usDayPeriodSpace =
  new Intl.DateTimeFormat("en-US", { timeStyle: "short" })
    .formatToParts(0)
    .find((part, i, parts) => part.type === "literal" && parts[i + 1].type === "dayPeriod")?.value || "";
const usDateRangeSeparator = new Intl.DateTimeFormat("en-US", { dateStyle: "short" })
  .formatRangeToParts(1 * 86400 * 1000, 366 * 86400 * 1000)
  .find((part) => part.type === "literal" && part.source === "shared").value;
const deDateRangeSeparator = new Intl.DateTimeFormat("de-AT", { dateStyle: "short" })
  .formatRangeToParts(1 * 86400 * 1000, 366 * 86400 * 1000)
  .find((part) => part.type === "literal" && part.source === "shared").value;

// Workarounds for https:// unicode-org.atlassian.net/browse/CLDR-16243
const deMonthDayRangeSeparator = new Intl.DateTimeFormat("de-AT", { month: "numeric", day: "numeric" })
  .formatRangeToParts(1 * 86400 * 1000, 90 * 86400 * 1000)
  .find((part) => part.type === "literal" && part.source === "shared").value;
const deMonthYearSeparator = new Intl.DateTimeFormat("de-AT", { year: "numeric", month: "numeric" })
  .formatToParts(0)
  .find((part) => part.type === "literal").value;
const deMonthInYearMonthHasLeadingZero = new Intl.DateTimeFormat("de-AT", {
  year: "numeric",
  month: "numeric"
})
  .formatToParts(new Date(2000, 3, 1))
  .find((part) => part.type === "month")
  .value.startsWith("0");

// should return an Array
assert(Array.isArray(Intl.DateTimeFormat.supportedLocalesOf()));
var onlyOnce = (value) => {
  var obj = {
    calls: 0,
    toString() {
      if (++this.calls > 1) throw new RangeError("prop read twice");
      return value;
    }
  };
  return obj;
};
var optionsAT = { timeZone: onlyOnce("Europe/Vienna") };
var optionsUS = {
  calls: 0,
  value: "America/New_York",
  get timeZone() {
    if (++this.calls > 1) throw new RangeError("prop read twice");
    return this.value;
  },
  set timeZone(val) {
    this.value = val;
  }
};
var localesAT = ["de-AT"];
var us = new Intl.DateTimeFormat("en-US", optionsUS);
var at = new Intl.DateTimeFormat(localesAT, optionsAT);
optionsAT.timeZone = {
  toString: () => "Bogus/Time-Zone",
  toJSON: () => "Bogus/Time-Zone"
};
optionsUS.timeZone = "Bogus/Time-Zone";
var us2 = new Intl.DateTimeFormat("en-US");
var at2 = new Intl.DateTimeFormat(localesAT);
localesAT[0] = ["invalid locale"];
var usCalendar = us.resolvedOptions().calendar;
var atCalendar = at.resolvedOptions().calendar;
var t1 = "1976-11-18T14:23:30+00:00[UTC]";
var t2 = "2020-02-20T15:44:56-05:00[America/New_York]";
var start = new Date("1922-12-30");
var end = new Date("1991-12-26");

// should work for Instant
assert.sameValue(us.format(Temporal.Instant.from(t1)), `11/18/1976, 9:23:30${usDayPeriodSpace}AM`);
assert.sameValue(at.format(Temporal.Instant.from(t1)), "18.11.1976, 15:23:30");

// should work for DateTime
assert.sameValue(us.format(Temporal.PlainDateTime.from(t1)), `11/18/1976, 2:23:30${usDayPeriodSpace}PM`);
assert.sameValue(at.format(Temporal.PlainDateTime.from(t1)), "18.11.1976, 14:23:30");

// should work for Time
assert.sameValue(us.format(Temporal.PlainTime.from(t1)), `2:23:30${usDayPeriodSpace}PM`);
assert.sameValue(at.format(Temporal.PlainTime.from(t1)), "14:23:30");

// should work for Date
assert.sameValue(us.format(Temporal.PlainDate.from(t1)), "11/18/1976");
assert.sameValue(at.format(Temporal.PlainDate.from(t1)), "18.11.1976");

// should work for YearMonth
var t = Temporal.PlainDate.from(t1);
assert.sameValue(us.format(t.withCalendar(usCalendar).toPlainYearMonth()), "11/1976");
assert.sameValue(at.format(t.withCalendar(atCalendar).toPlainYearMonth()), `11${deMonthYearSeparator}1976`);

// should work for MonthDay
var t = Temporal.PlainDate.from(t1);
assert.sameValue(us.format(t.withCalendar(usCalendar).toPlainMonthDay()), "11/18");
assert.sameValue(at.format(t.withCalendar(atCalendar).toPlainMonthDay()), "18.11.");

// should not break legacy Date
assert.sameValue(us.format(start), "12/29/1922");
assert.sameValue(at.format(start), "30.12.1922");

// should work for Instant
assert.deepEqual(us.formatToParts(Temporal.Instant.from(t2)), [
  {
    type: "month",
    value: "2"
  },
  {
    type: "literal",
    value: "/"
  },
  {
    type: "day",
    value: "20"
  },
  {
    type: "literal",
    value: "/"
  },
  {
    type: "year",
    value: "2020"
  },
  {
    type: "literal",
    value: ", "
  },
  {
    type: "hour",
    value: "3"
  },
  {
    type: "literal",
    value: ":"
  },
  {
    type: "minute",
    value: "44"
  },
  {
    type: "literal",
    value: ":"
  },
  {
    type: "second",
    value: "56"
  },
  {
    type: "literal",
    value: usDayPeriodSpace
  },
  {
    type: "dayPeriod",
    value: "PM"
  }
]);
assert.deepEqual(at.formatToParts(Temporal.Instant.from(t2)), [
  {
    type: "day",
    value: "20"
  },
  {
    type: "literal",
    value: "."
  },
  {
    type: "month",
    value: "2"
  },
  {
    type: "literal",
    value: "."
  },
  {
    type: "year",
    value: "2020"
  },
  {
    type: "literal",
    value: ", "
  },
  {
    type: "hour",
    value: "21"
  },
  {
    type: "literal",
    value: ":"
  },
  {
    type: "minute",
    value: "44"
  },
  {
    type: "literal",
    value: ":"
  },
  {
    type: "second",
    value: "56"
  }
]);
// should work for DateTime
assert.deepEqual(us.formatToParts(Temporal.PlainDateTime.from(t2)), [
  {
    type: "month",
    value: "2"
  },
  {
    type: "literal",
    value: "/"
  },
  {
    type: "day",
    value: "20"
  },
  {
    type: "literal",
    value: "/"
  },
  {
    type: "year",
    value: "2020"
  },
  {
    type: "literal",
    value: ", "
  },
  {
    type: "hour",
    value: "3"
  },
  {
    type: "literal",
    value: ":"
  },
  {
    type: "minute",
    value: "44"
  },
  {
    type: "literal",
    value: ":"
  },
  {
    type: "second",
    value: "56"
  },
  {
    type: "literal",
    value: usDayPeriodSpace
  },
  {
    type: "dayPeriod",
    value: "PM"
  }
]);
assert.deepEqual(at.formatToParts(Temporal.PlainDateTime.from(t2)), [
  {
    type: "day",
    value: "20"
  },
  {
    type: "literal",
    value: "."
  },
  {
    type: "month",
    value: "2"
  },
  {
    type: "literal",
    value: "."
  },
  {
    type: "year",
    value: "2020"
  },
  {
    type: "literal",
    value: ", "
  },
  {
    type: "hour",
    value: "15"
  },
  {
    type: "literal",
    value: ":"
  },
  {
    type: "minute",
    value: "44"
  },
  {
    type: "literal",
    value: ":"
  },
  {
    type: "second",
    value: "56"
  }
]);
// should work for Time
assert.deepEqual(us.formatToParts(Temporal.PlainTime.from(t2)), [
  {
    type: "hour",
    value: "3"
  },
  {
    type: "literal",
    value: ":"
  },
  {
    type: "minute",
    value: "44"
  },
  {
    type: "literal",
    value: ":"
  },
  {
    type: "second",
    value: "56"
  },
  {
    type: "literal",
    value: usDayPeriodSpace
  },
  {
    type: "dayPeriod",
    value: "PM"
  }
]);
assert.deepEqual(at.formatToParts(Temporal.PlainTime.from(t2)), [
  {
    type: "hour",
    value: "15"
  },
  {
    type: "literal",
    value: ":"
  },
  {
    type: "minute",
    value: "44"
  },
  {
    type: "literal",
    value: ":"
  },
  {
    type: "second",
    value: "56"
  }
]);
// should work for Date
assert.deepEqual(us.formatToParts(Temporal.PlainDate.from(t2)), [
  {
    type: "month",
    value: "2"
  },
  {
    type: "literal",
    value: "/"
  },
  {
    type: "day",
    value: "20"
  },
  {
    type: "literal",
    value: "/"
  },
  {
    type: "year",
    value: "2020"
  }
]);
assert.deepEqual(at.formatToParts(Temporal.PlainDate.from(t2)), [
  {
    type: "day",
    value: "20"
  },
  {
    type: "literal",
    value: "."
  },
  {
    type: "month",
    value: "2"
  },
  {
    type: "literal",
    value: "."
  },
  {
    type: "year",
    value: "2020"
  }
]);
// should work for YearMonth
var t = Temporal.PlainDate.from(t2);
assert.deepEqual(us.formatToParts(t.withCalendar(usCalendar).toPlainYearMonth()), [
  {
    type: "month",
    value: "2"
  },
  {
    type: "literal",
    value: "/"
  },
  {
    type: "year",
    value: "2020"
  }
]);
assert.deepEqual(at.formatToParts(t.withCalendar(atCalendar).toPlainYearMonth()), [
  {
    type: "month",
    value: deMonthInYearMonthHasLeadingZero ? "02" : "2"
  },
  {
    type: "literal",
    value: deMonthYearSeparator
  },
  {
    type: "year",
    value: "2020"
  }
]);
// should work for MonthDay
var t = Temporal.PlainDate.from(t2);
assert.deepEqual(us.formatToParts(t.withCalendar(usCalendar).toPlainMonthDay()), [
  {
    type: "month",
    value: "2"
  },
  {
    type: "literal",
    value: "/"
  },
  {
    type: "day",
    value: "20"
  }
]);
assert.deepEqual(at.formatToParts(t.withCalendar(atCalendar).toPlainMonthDay()), [
  {
    type: "day",
    value: "20"
  },
  {
    type: "literal",
    value: "."
  },
  {
    type: "month",
    value: "2"
  },
  {
    type: "literal",
    value: "."
  }
]);
// should not break legacy Date
assert.deepEqual(us.formatToParts(end), [
  {
    type: "month",
    value: "12"
  },
  {
    type: "literal",
    value: "/"
  },
  {
    type: "day",
    value: "25"
  },
  {
    type: "literal",
    value: "/"
  },
  {
    type: "year",
    value: "1991"
  }
]);
assert.deepEqual(at.formatToParts(end), [
  {
    type: "day",
    value: "26"
  },
  {
    type: "literal",
    value: "."
  },
  {
    type: "month",
    value: "12"
  },
  {
    type: "literal",
    value: "."
  },
  {
    type: "year",
    value: "1991"
  }
]);
// formatRange
// should work for Instant
assert.sameValue(
  us.formatRange(Temporal.Instant.from(t1), Temporal.Instant.from(t2)),
  `11/18/1976, 9:23:30${usDayPeriodSpace}AM${usDateRangeSeparator}2/20/2020, 3:44:56${usDayPeriodSpace}PM`
);
assert.sameValue(
  at.formatRange(Temporal.Instant.from(t1), Temporal.Instant.from(t2)),
  `18.11.1976, 15:23:30${deDateRangeSeparator}20.2.2020, 21:44:56`
);

// should work for DateTime
assert.sameValue(
  us.formatRange(Temporal.PlainDateTime.from(t1), Temporal.PlainDateTime.from(t2)),
  `11/18/1976, 2:23:30${usDayPeriodSpace}PM${usDateRangeSeparator}2/20/2020, 3:44:56${usDayPeriodSpace}PM`
);
assert.sameValue(
  at.formatRange(Temporal.PlainDateTime.from(t1), Temporal.PlainDateTime.from(t2)),
  `18.11.1976, 14:23:30${deDateRangeSeparator}20.2.2020, 15:44:56`
);

// should work for Time
assert.sameValue(
  us.formatRange(Temporal.PlainTime.from(t1), Temporal.PlainTime.from(t2)),
  `2:23:30${usDayPeriodSpace}PM${usDateRangeSeparator}3:44:56${usDayPeriodSpace}PM`
);
assert.sameValue(
  at.formatRange(Temporal.PlainTime.from(t1), Temporal.PlainTime.from(t2)),
  `14:23:30${deDateRangeSeparator}15:44:56`
);

// should work for Date
assert.sameValue(
  us.formatRange(Temporal.PlainDate.from(t1), Temporal.PlainDate.from(t2)),
  `11/18/1976${usDateRangeSeparator}2/20/2020`
);
assert.sameValue(
  at.formatRange(Temporal.PlainDate.from(t1), Temporal.PlainDate.from(t2)),
  `18.11.1976${deDateRangeSeparator}20.02.2020`
);

// should work for YearMonth
var date1 = Temporal.PlainDate.from(t1);
var date2 = Temporal.PlainDate.from(t2);
assert.sameValue(
  us.formatRange(date1.withCalendar(usCalendar).toPlainYearMonth(), date2.withCalendar(usCalendar).toPlainYearMonth()),
  `11/1976${usDateRangeSeparator}2/2020`
);
assert.sameValue(
  at.formatRange(date1.withCalendar(atCalendar).toPlainYearMonth(), date2.withCalendar(atCalendar).toPlainYearMonth()),
  `11${deMonthYearSeparator}1976${deDateRangeSeparator}2${deMonthYearSeparator}2020`
);

// should work for MonthDay
var date1 = Temporal.PlainDate.from(t1);
var date2 = Temporal.PlainDate.from(t2);
assert.sameValue(
  us.formatRange(date2.withCalendar(usCalendar).toPlainMonthDay(), date1.withCalendar(usCalendar).toPlainMonthDay()),
  `2/20${usDateRangeSeparator}11/18`
);
assert.sameValue(
  at.formatRange(date2.withCalendar(atCalendar).toPlainMonthDay(), date1.withCalendar(atCalendar).toPlainMonthDay()),
  `20.02${deMonthDayRangeSeparator}18.11.`
);

// should not break legacy Date
assert.sameValue(us.formatRange(start, end), `12/29/1922${usDateRangeSeparator}12/25/1991`);
assert.sameValue(at.formatRange(start, end), `30.12.1922${deDateRangeSeparator}26.12.1991`);

// should throw a RangeError when called with different calendars
assert.throws(RangeError, () =>
  us.formatRange(Temporal.PlainDateTime.from(t1), Temporal.PlainDateTime.from(t2).withCalendar("japanese"))
);
assert.throws(RangeError, () =>
  us.formatRange(Temporal.PlainDate.from(t1), Temporal.PlainDate.from(t2).withCalendar("japanese"))
);

// formatRangeToParts
// should work for Instant
assert.deepEqual(us.formatRangeToParts(Temporal.Instant.from(t1), Temporal.Instant.from(t2)), [
  {
    type: "month",
    value: "11",
    source: "startRange"
  },
  {
    type: "literal",
    value: "/",
    source: "startRange"
  },
  {
    type: "day",
    value: "18",
    source: "startRange"
  },
  {
    type: "literal",
    value: "/",
    source: "startRange"
  },
  {
    type: "year",
    value: "1976",
    source: "startRange"
  },
  {
    type: "literal",
    value: ", ",
    source: "startRange"
  },
  {
    type: "hour",
    value: "9",
    source: "startRange"
  },
  {
    type: "literal",
    value: ":",
    source: "startRange"
  },
  {
    type: "minute",
    value: "23",
    source: "startRange"
  },
  {
    type: "literal",
    value: ":",
    source: "startRange"
  },
  {
    type: "second",
    value: "30",
    source: "startRange"
  },
  {
    type: "literal",
    value: usDayPeriodSpace,
    source: "startRange"
  },
  {
    type: "dayPeriod",
    value: "AM",
    source: "startRange"
  },
  {
    type: "literal",
    value: usDateRangeSeparator,
    source: "shared"
  },
  {
    type: "month",
    value: "2",
    source: "endRange"
  },
  {
    type: "literal",
    value: "/",
    source: "endRange"
  },
  {
    type: "day",
    value: "20",
    source: "endRange"
  },
  {
    type: "literal",
    value: "/",
    source: "endRange"
  },
  {
    type: "year",
    value: "2020",
    source: "endRange"
  },
  {
    type: "literal",
    value: ", ",
    source: "endRange"
  },
  {
    type: "hour",
    value: "3",
    source: "endRange"
  },
  {
    type: "literal",
    value: ":",
    source: "endRange"
  },
  {
    type: "minute",
    value: "44",
    source: "endRange"
  },
  {
    type: "literal",
    value: ":",
    source: "endRange"
  },
  {
    type: "second",
    value: "56",
    source: "endRange"
  },
  {
    type: "literal",
    value: usDayPeriodSpace,
    source: "endRange"
  },
  {
    type: "dayPeriod",
    value: "PM",
    source: "endRange"
  }
]);
assert.deepEqual(at.formatRangeToParts(Temporal.Instant.from(t1), Temporal.Instant.from(t2)), [
  {
    type: "day",
    value: "18",
    source: "startRange"
  },
  {
    type: "literal",
    value: ".",
    source: "startRange"
  },
  {
    type: "month",
    value: "11",
    source: "startRange"
  },
  {
    type: "literal",
    value: ".",
    source: "startRange"
  },
  {
    type: "year",
    value: "1976",
    source: "startRange"
  },
  {
    type: "literal",
    value: ", ",
    source: "startRange"
  },
  {
    type: "hour",
    value: "15",
    source: "startRange"
  },
  {
    type: "literal",
    value: ":",
    source: "startRange"
  },
  {
    type: "minute",
    value: "23",
    source: "startRange"
  },
  {
    type: "literal",
    value: ":",
    source: "startRange"
  },
  {
    type: "second",
    value: "30",
    source: "startRange"
  },
  {
    type: "literal",
    value: deDateRangeSeparator,
    source: "shared"
  },
  {
    type: "day",
    value: "20",
    source: "endRange"
  },
  {
    type: "literal",
    value: ".",
    source: "endRange"
  },
  {
    type: "month",
    value: "2",
    source: "endRange"
  },
  {
    type: "literal",
    value: ".",
    source: "endRange"
  },
  {
    type: "year",
    value: "2020",
    source: "endRange"
  },
  {
    type: "literal",
    value: ", ",
    source: "endRange"
  },
  {
    type: "hour",
    value: "21",
    source: "endRange"
  },
  {
    type: "literal",
    value: ":",
    source: "endRange"
  },
  {
    type: "minute",
    value: "44",
    source: "endRange"
  },
  {
    type: "literal",
    value: ":",
    source: "endRange"
  },
  {
    type: "second",
    value: "56",
    source: "endRange"
  }
]);
// should work for DateTime
assert.deepEqual(us.formatRangeToParts(Temporal.PlainDateTime.from(t1), Temporal.PlainDateTime.from(t2)), [
  {
    type: "month",
    value: "11",
    source: "startRange"
  },
  {
    type: "literal",
    value: "/",
    source: "startRange"
  },
  {
    type: "day",
    value: "18",
    source: "startRange"
  },
  {
    type: "literal",
    value: "/",
    source: "startRange"
  },
  {
    type: "year",
    value: "1976",
    source: "startRange"
  },
  {
    type: "literal",
    value: ", ",
    source: "startRange"
  },
  {
    type: "hour",
    value: "2",
    source: "startRange"
  },
  {
    type: "literal",
    value: ":",
    source: "startRange"
  },
  {
    type: "minute",
    value: "23",
    source: "startRange"
  },
  {
    type: "literal",
    value: ":",
    source: "startRange"
  },
  {
    type: "second",
    value: "30",
    source: "startRange"
  },
  {
    type: "literal",
    value: usDayPeriodSpace,
    source: "startRange"
  },
  {
    type: "dayPeriod",
    value: "PM",
    source: "startRange"
  },
  {
    type: "literal",
    value: usDateRangeSeparator,
    source: "shared"
  },
  {
    type: "month",
    value: "2",
    source: "endRange"
  },
  {
    type: "literal",
    value: "/",
    source: "endRange"
  },
  {
    type: "day",
    value: "20",
    source: "endRange"
  },
  {
    type: "literal",
    value: "/",
    source: "endRange"
  },
  {
    type: "year",
    value: "2020",
    source: "endRange"
  },
  {
    type: "literal",
    value: ", ",
    source: "endRange"
  },
  {
    type: "hour",
    value: "3",
    source: "endRange"
  },
  {
    type: "literal",
    value: ":",
    source: "endRange"
  },
  {
    type: "minute",
    value: "44",
    source: "endRange"
  },
  {
    type: "literal",
    value: ":",
    source: "endRange"
  },
  {
    type: "second",
    value: "56",
    source: "endRange"
  },
  {
    type: "literal",
    value: usDayPeriodSpace,
    source: "endRange"
  },
  {
    type: "dayPeriod",
    value: "PM",
    source: "endRange"
  }
]);
assert.deepEqual(at.formatRangeToParts(Temporal.PlainDateTime.from(t1), Temporal.PlainDateTime.from(t2)), [
  {
    type: "day",
    value: "18",
    source: "startRange"
  },
  {
    type: "literal",
    value: ".",
    source: "startRange"
  },
  {
    type: "month",
    value: "11",
    source: "startRange"
  },
  {
    type: "literal",
    value: ".",
    source: "startRange"
  },
  {
    type: "year",
    value: "1976",
    source: "startRange"
  },
  {
    type: "literal",
    value: ", ",
    source: "startRange"
  },
  {
    type: "hour",
    value: "14",
    source: "startRange"
  },
  {
    type: "literal",
    value: ":",
    source: "startRange"
  },
  {
    type: "minute",
    value: "23",
    source: "startRange"
  },
  {
    type: "literal",
    value: ":",
    source: "startRange"
  },
  {
    type: "second",
    value: "30",
    source: "startRange"
  },
  {
    type: "literal",
    value: deDateRangeSeparator,
    source: "shared"
  },
  {
    type: "day",
    value: "20",
    source: "endRange"
  },
  {
    type: "literal",
    value: ".",
    source: "endRange"
  },
  {
    type: "month",
    value: "2",
    source: "endRange"
  },
  {
    type: "literal",
    value: ".",
    source: "endRange"
  },
  {
    type: "year",
    value: "2020",
    source: "endRange"
  },
  {
    type: "literal",
    value: ", ",
    source: "endRange"
  },
  {
    type: "hour",
    value: "15",
    source: "endRange"
  },
  {
    type: "literal",
    value: ":",
    source: "endRange"
  },
  {
    type: "minute",
    value: "44",
    source: "endRange"
  },
  {
    type: "literal",
    value: ":",
    source: "endRange"
  },
  {
    type: "second",
    value: "56",
    source: "endRange"
  }
]);
// should work for Time
assert.deepEqual(us.formatRangeToParts(Temporal.PlainTime.from(t1), Temporal.PlainTime.from(t2)), [
  {
    type: "hour",
    value: "2",
    source: "startRange"
  },
  {
    type: "literal",
    value: ":",
    source: "startRange"
  },
  {
    type: "minute",
    value: "23",
    source: "startRange"
  },
  {
    type: "literal",
    value: ":",
    source: "startRange"
  },
  {
    type: "second",
    value: "30",
    source: "startRange"
  },
  {
    type: "literal",
    value: usDayPeriodSpace,
    source: "startRange"
  },
  {
    type: "dayPeriod",
    value: "PM",
    source: "startRange"
  },
  {
    type: "literal",
    value: usDateRangeSeparator,
    source: "shared"
  },
  {
    type: "hour",
    value: "3",
    source: "endRange"
  },
  {
    type: "literal",
    value: ":",
    source: "endRange"
  },
  {
    type: "minute",
    value: "44",
    source: "endRange"
  },
  {
    type: "literal",
    value: ":",
    source: "endRange"
  },
  {
    type: "second",
    value: "56",
    source: "endRange"
  },
  {
    type: "literal",
    value: usDayPeriodSpace,
    source: "endRange"
  },
  {
    type: "dayPeriod",
    value: "PM",
    source: "endRange"
  }
]);
assert.deepEqual(at.formatRangeToParts(Temporal.PlainTime.from(t1), Temporal.PlainTime.from(t2)), [
  {
    type: "hour",
    value: "14",
    source: "startRange"
  },
  {
    type: "literal",
    value: ":",
    source: "startRange"
  },
  {
    type: "minute",
    value: "23",
    source: "startRange"
  },
  {
    type: "literal",
    value: ":",
    source: "startRange"
  },
  {
    type: "second",
    value: "30",
    source: "startRange"
  },
  {
    type: "literal",
    value: deDateRangeSeparator,
    source: "shared"
  },
  {
    type: "hour",
    value: "15",
    source: "endRange"
  },
  {
    type: "literal",
    value: ":",
    source: "endRange"
  },
  {
    type: "minute",
    value: "44",
    source: "endRange"
  },
  {
    type: "literal",
    value: ":",
    source: "endRange"
  },
  {
    type: "second",
    value: "56",
    source: "endRange"
  }
]);
// should work for Date
assert.deepEqual(us.formatRangeToParts(Temporal.PlainDate.from(t1), Temporal.PlainDate.from(t2)), [
  {
    type: "month",
    value: "11",
    source: "startRange"
  },
  {
    type: "literal",
    value: "/",
    source: "startRange"
  },
  {
    type: "day",
    value: "18",
    source: "startRange"
  },
  {
    type: "literal",
    value: "/",
    source: "startRange"
  },
  {
    type: "year",
    value: "1976",
    source: "startRange"
  },
  {
    type: "literal",
    value: usDateRangeSeparator,
    source: "shared"
  },
  {
    type: "month",
    value: "2",
    source: "endRange"
  },
  {
    type: "literal",
    value: "/",
    source: "endRange"
  },
  {
    type: "day",
    value: "20",
    source: "endRange"
  },
  {
    type: "literal",
    value: "/",
    source: "endRange"
  },
  {
    type: "year",
    value: "2020",
    source: "endRange"
  }
]);
assert.deepEqual(at.formatRangeToParts(Temporal.PlainDate.from(t1), Temporal.PlainDate.from(t2)), [
  {
    type: "day",
    value: "18",
    source: "startRange"
  },
  {
    type: "literal",
    value: ".",
    source: "startRange"
  },
  {
    type: "month",
    value: "11",
    source: "startRange"
  },
  {
    type: "literal",
    value: ".",
    source: "startRange"
  },
  {
    type: "year",
    value: "1976",
    source: "startRange"
  },
  {
    type: "literal",
    value: deDateRangeSeparator,
    source: "shared"
  },
  {
    type: "day",
    value: "20",
    source: "endRange"
  },
  {
    type: "literal",
    value: ".",
    source: "endRange"
  },
  {
    type: "month",
    value: "02",
    source: "endRange"
  },
  {
    type: "literal",
    value: ".",
    source: "endRange"
  },
  {
    type: "year",
    value: "2020",
    source: "endRange"
  }
]);
// should work for YearMonth
var date1 = Temporal.PlainDate.from(t1);
var date2 = Temporal.PlainDate.from(t2);
assert.deepEqual(
  us.formatRangeToParts(
    date1.withCalendar(usCalendar).toPlainYearMonth(),
    date2.withCalendar(usCalendar).toPlainYearMonth()
  ),
  [
    {
      type: "month",
      value: "11",
      source: "startRange"
    },
    {
      type: "literal",
      value: "/",
      source: "startRange"
    },
    {
      type: "year",
      value: "1976",
      source: "startRange"
    },
    {
      type: "literal",
      value: usDateRangeSeparator,
      source: "shared"
    },
    {
      type: "month",
      value: "2",
      source: "endRange"
    },
    {
      type: "literal",
      value: "/",
      source: "endRange"
    },
    {
      type: "year",
      value: "2020",
      source: "endRange"
    }
  ]
);
assert.deepEqual(
  at.formatRangeToParts(
    date1.withCalendar(atCalendar).toPlainYearMonth(),
    date2.withCalendar(atCalendar).toPlainYearMonth()
  ),
  [
    {
      type: "month",
      value: "11",
      source: "startRange"
    },
    {
      type: "literal",
      value: deMonthYearSeparator,
      source: "startRange"
    },
    {
      type: "year",
      value: "1976",
      source: "startRange"
    },
    {
      type: "literal",
      value: deDateRangeSeparator,
      source: "shared"
    },
    {
      type: "month",
      value: "2",
      source: "endRange"
    },
    {
      type: "literal",
      value: deMonthYearSeparator,
      source: "endRange"
    },
    {
      type: "year",
      value: "2020",
      source: "endRange"
    }
  ]
);
// should work for MonthDay
var date1 = Temporal.PlainDate.from(t1);
var date2 = Temporal.PlainDate.from(t2);
assert.deepEqual(
  us.formatRangeToParts(
    date2.withCalendar(usCalendar).toPlainMonthDay(),
    date1.withCalendar(usCalendar).toPlainMonthDay()
  ),
  [
    {
      type: "month",
      value: "2",
      source: "startRange"
    },
    {
      type: "literal",
      value: "/",
      source: "startRange"
    },
    {
      type: "day",
      value: "20",
      source: "startRange"
    },
    {
      type: "literal",
      value: usDateRangeSeparator,
      source: "shared"
    },
    {
      type: "month",
      value: "11",
      source: "endRange"
    },
    {
      type: "literal",
      value: "/",
      source: "endRange"
    },
    {
      type: "day",
      value: "18",
      source: "endRange"
    }
  ]
);
assert.deepEqual(
  at.formatRangeToParts(
    date2.withCalendar(atCalendar).toPlainMonthDay(),
    date1.withCalendar(atCalendar).toPlainMonthDay()
  ),
  [
    {
      type: "day",
      value: "20",
      source: "startRange"
    },
    {
      type: "literal",
      value: ".",
      source: "startRange"
    },
    {
      type: "month",
      value: "02",
      source: "startRange"
    },
    {
      type: "literal",
      value: deMonthDayRangeSeparator,
      source: "shared"
    },
    {
      type: "day",
      value: "18",
      source: "endRange"
    },
    {
      type: "literal",
      value: ".",
      source: "endRange"
    },
    {
      type: "month",
      value: "11",
      source: "endRange"
    },
    {
      type: "literal",
      value: ".",
      source: "shared"
    }
  ]
);
// should not break legacy Date
assert.deepEqual(us.formatRangeToParts(start, end), [
  {
    type: "month",
    value: "12",
    source: "startRange"
  },
  {
    type: "literal",
    value: "/",
    source: "startRange"
  },
  {
    type: "day",
    value: "29",
    source: "startRange"
  },
  {
    type: "literal",
    value: "/",
    source: "startRange"
  },
  {
    type: "year",
    value: "1922",
    source: "startRange"
  },
  {
    type: "literal",
    value: usDateRangeSeparator,
    source: "shared"
  },
  {
    type: "month",
    value: "12",
    source: "endRange"
  },
  {
    type: "literal",
    value: "/",
    source: "endRange"
  },
  {
    type: "day",
    value: "25",
    source: "endRange"
  },
  {
    type: "literal",
    value: "/",
    source: "endRange"
  },
  {
    type: "year",
    value: "1991",
    source: "endRange"
  }
]);
assert.deepEqual(at.formatRangeToParts(start, end), [
  {
    type: "day",
    value: "30",
    source: "startRange"
  },
  {
    type: "literal",
    value: ".",
    source: "startRange"
  },
  {
    type: "month",
    value: "12",
    source: "startRange"
  },
  {
    type: "literal",
    value: ".",
    source: "startRange"
  },
  {
    type: "year",
    value: "1922",
    source: "startRange"
  },
  {
    type: "literal",
    value: deDateRangeSeparator,
    source: "shared"
  },
  {
    type: "day",
    value: "26",
    source: "endRange"
  },
  {
    type: "literal",
    value: ".",
    source: "endRange"
  },
  {
    type: "month",
    value: "12",
    source: "endRange"
  },
  {
    type: "literal",
    value: ".",
    source: "endRange"
  },
  {
    type: "year",
    value: "1991",
    source: "endRange"
  }
]);
// should throw a TypeError when called with dissimilar types
assert.throws(TypeError, () => at.formatRangeToParts(Temporal.Instant.from(t1), Temporal.PlainDateTime.from(t2)));
// should throw a RangeError when called with different calendars
assert.throws(RangeError, () =>
  at.formatRangeToParts(Temporal.PlainDateTime.from(t1), Temporal.PlainDateTime.from(t2).withCalendar("japanese"))
);
assert.throws(RangeError, () =>
  at.formatRangeToParts(Temporal.PlainDate.from(t1), Temporal.PlainDate.from(t2).withCalendar("japanese"))
);
