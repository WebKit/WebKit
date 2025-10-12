// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
features:
  - Temporal
description: |
  pending
esid: pending
---*/

// Test we get consistent results for Temporal and Intl.DateTimeFormat, even
// though the former uses ICU4X, whereas the latter uses ICU4C for calendrical
// computations.

// Map Hebrew months from English name to their corresponding month number.
const hebrewMonths = {
  Tishri: 1,
  Heshvan: 2,
  Kislev: 3,
  Tevet: 4,
  Shevat: 5,
  "Adar I": 6,
  Adar: 6,
  "Adar II": 7,
  Nisan: [7, 8],
  Iyar: [8, 9],
  Sivan: [9, 10],
  Tamuz: [10, 11],
  Av: [11, 12],
  Elul: [12, 13],
};

function toFields(dtf, date, calendar, timeZone) {
  let {epochMilliseconds} = date.withCalendar("iso8601").toZonedDateTime(timeZone);
  let parts = dtf.formatToParts(epochMilliseconds);

  let monthPart = parts.find(({type}) => type === "month");
  let dayPart = parts.find(({type}) => type === "day");

  assert.sameValue(monthPart !== undefined, true);
  assert.sameValue(dayPart !== undefined, true);
  
  let month = parseInt(monthPart.value, 10);
  let day = parseInt(dayPart.value, 10);

  // Workaround for <https://bugzilla.mozilla.org/show_bug.cgi?id=1751833>.
  if (!Number.isInteger(month) && calendar === "hebrew") {
    assert.sameValue(Object.hasOwn(hebrewMonths, monthPart.value), true, `month = ${monthPart.value}`);

    let hebrewMonth = hebrewMonths[monthPart.value];
    if (Array.isArray(hebrewMonth)) {
      month = hebrewMonth[Number(date.inLeapYear)];
    } else {
      month = hebrewMonth;
    }
  }

  assert.sameValue(Number.isInteger(month), true, `month = ${monthPart.value}`);
  assert.sameValue(Number.isInteger(day), true, `day = ${dayPart.value}`);

  return {month, day};
}

// All supported calendars have at most 31 days per month.
const maximumDaysPerMonth = 31;

const timeZone = "UTC";

for (let calendar of Intl.supportedValuesOf("calendar")) {
  // Lunar calendars. ICU4C and ICU4X appear to use different algorithms.
  if (calendar === "chinese") continue;
  if (calendar === "dangi") continue;

  // Observational lunar calendars. ICU4C and ICU4X use different algorithms.
  if (calendar === "islamic") continue;
  if (calendar === "islamic-rgsa") continue;
  
  // ICU4C returns 29, but ICU4X correctly returns 30.
  //
  // Temporal.PlainDate.from({year: 2046, month:10, day:30}).withCalendar("hebrew").day
  if (calendar === "hebrew") continue;

  // ICU4C returns 29, but ICU4X returns 30.
  //
  // Temporal.PlainDate.from({year: 2006, month:7, day:25}).withCalendar("islamic-umalqura").day
  if (calendar === "islamic-umalqura") continue;

  // ICU4C returns 30, but ICU4X returns 31.
  //
  // Temporal.PlainDate.from({year: 2025, month:4, day:19}).withCalendar("persian").day
  if (calendar === "persian") continue;

  // Calendars "buddhist", "japanese", and "roc" use Julian calendar arithmetic
  // before the Gregorian change date October 15, 1582.
  // 
  // https://unicode-org.atlassian.net/jira/software/c/projects/ICU/issues/ICU-22412
  //
  // if (calendar === "buddhist") continue;
  // if (calendar === "japanese") continue;
  // if (calendar === "roc") continue;

  let dtf = new Intl.DateTimeFormat("en", {
    calendar,
    timeZone,
    month: "numeric",
    day: "numeric",
  });

  // Test near past and near future.
  for (let y = 2050; y >= 1950; --y) {
    let isoYear = Temporal.PlainDate.from({year: y, month: 1, day: 1});
    let {year, monthsInYear} = isoYear.withCalendar(calendar);

    for (let month = 1; month <= monthsInYear; ++month) {
      let date = Temporal.PlainDate.from({calendar, year, month, day: maximumDaysPerMonth});
      let fields = toFields(dtf, date, calendar, timeZone);

      assert.sameValue(fields.month, date.month, `date = ${date}`);
      assert.sameValue(fields.day, date.day, `date = ${date}`);
    }
  }
}

