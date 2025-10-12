// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
features:
  - Temporal
description: |
  pending
esid: pending
---*/

const eras = [
  {
    name: "meiji",
    // Start date is actually October 23, 1868.
    //
    // https://github.com/unicode-org/icu4x/issues/4892
    // https://unicode-org.atlassian.net/browse/CLDR-11375
    start: "1868-09-08",
    end: "1912-07-29",
  },
  {
    name: "taisho",
    start: "1912-07-30",
    end: "1926-12-24",
  },
  {
    name: "showa",
    start: "1926-12-25",
    end: "1989-01-07",
  },
  {
    name: "heisei",
    start: "1989-01-08",
    end: "2019-04-30",
  },
  {
    name: "reiwa",
    start: "2019-05-01",
    end: undefined,
  },
];

for (let {name: era, start, end} of eras) {
  let eraStart = Temporal.PlainDate.from(start);
  let eraEnd = end && Temporal.PlainDate.from(end);

  // Months before start of era.
  for (let month = 1; month < eraStart.month; ++month) {
    for (let day = 1; day <= 32; ++day) {
      let monthCode = "M" + String(month).padStart(2, "0");

      let dateWithMonth = Temporal.PlainDate.from({
        calendar: "japanese",
        era,
        eraYear: 1,
        month,
        day,
      });

      let dateWithMonthCode = Temporal.PlainDate.from({
        calendar: "japanese",
        era,
        eraYear: 1,
        monthCode,
        day,
      });

      let expected = eraStart.with({month, day}).toString();

      assert.sameValue(dateWithMonth.toString({calendarName: "never"}), expected);
      assert.sameValue(dateWithMonthCode.toString({calendarName: "never"}), expected);
    }
  }

  // Days before start of era.
  for (let day = 1; day < eraStart.day; ++day) {
    let dateWithMonth = Temporal.PlainDate.from({
      calendar: "japanese",
      era,
      eraYear: 1,
      month: eraStart.month,
      day,
    });

    let dateWithMonthCode = Temporal.PlainDate.from({
      calendar: "japanese",
      era,
      eraYear: 1,
      monthCode: eraStart.monthCode,
      day,
    });

    let expected = eraStart.with({day}).toString();

    assert.sameValue(dateWithMonth.toString({calendarName: "never"}), expected);
    assert.sameValue(dateWithMonthCode.toString({calendarName: "never"}), expected);
  }

  // After start of era.
  for (let day = eraStart.day; day <= 32; ++day) {
    let dateWithMonth = Temporal.PlainDate.from({
      calendar: "japanese",
      era,
      eraYear: 1,
      month: eraStart.month,
      day,
    });

    let dateWithMonthCode = Temporal.PlainDate.from({
      calendar: "japanese",
      era,
      eraYear: 1,
      monthCode: eraStart.monthCode,
      day,
    });

    let expected = Temporal.PlainDate.from({
      year: eraStart.year,
      monthCode: eraStart.monthCode,
      day,
    }).toString();

    assert.sameValue(dateWithMonth.toString({calendarName: "never"}), expected);
    assert.sameValue(dateWithMonthCode.toString({calendarName: "never"}), expected);
  }

  // Months after start of era.
  for (let month = eraStart.month + 1; month <= 12; ++month) {
    for (let day = 1; day <= 32; ++day) {
      let monthCode = "M" + String(month).padStart(2, "0");

      let dateWithMonth = Temporal.PlainDate.from({
        calendar: "japanese",
        era,
        eraYear: 1,
        month,
        day,
      });

      let dateWithMonthCode = Temporal.PlainDate.from({
        calendar: "japanese",
        era,
        eraYear: 1,
        monthCode,
        day,
      });

      let expected = Temporal.PlainDate.from({
        year: eraStart.year,
        monthCode: monthCode,
        day,
      }).toString();

      assert.sameValue(dateWithMonth.toString({calendarName: "never"}), expected);
      assert.sameValue(dateWithMonthCode.toString({calendarName: "never"}), expected);
    }
  }

  if (end) {
    let lastEraYear = (eraEnd.year - eraStart.year) + 1;

    // After end of era.
    for (let day = 31; day <= 32; ++day) {
      let date = Temporal.PlainDate.from({
        calendar: "japanese",
        era,
        eraYear: 100,
        monthCode: "M12",
        day,
      });

      let expected = eraStart.add({years: 100 - 1})
                             .with({month: 12, day})
                             .toString();

      assert.sameValue(date.toString({calendarName: "never"}), expected);
    }

    // Days after end of era.
    for (let day = eraEnd.day + 1; day <= 32; ++day) {
      let dateWithMonth = Temporal.PlainDate.from({
        calendar: "japanese",
        era,
        eraYear: lastEraYear,
        month: eraEnd.month,
        day,
      });

      let dateWithMonthCode = Temporal.PlainDate.from({
        calendar: "japanese",
        era,
        eraYear: lastEraYear,
        monthCode: eraEnd.monthCode,
        day,
      });

      let expected = eraEnd.with({day}).toString();

      assert.sameValue(dateWithMonth.toString({calendarName: "never"}), expected);
      assert.sameValue(dateWithMonthCode.toString({calendarName: "never"}), expected);
    }

    // Months after end of era.
    for (let month = eraEnd.month + 1; month <= 12; ++month) {
      for (let day = 1; day <= 32; ++day) {
        let monthCode = "M" + String(month).padStart(2, "0");

        let dateWithMonth = Temporal.PlainDate.from({
          calendar: "japanese",
          era,
          eraYear: lastEraYear,
          month,
          day,
        });

        let dateWithMonthCode = Temporal.PlainDate.from({
          calendar: "japanese",
          era,
          eraYear: lastEraYear,
          monthCode,
          day,
        });

        let expected = eraEnd.with({month, day}).toString();

        assert.sameValue(dateWithMonth.toString({calendarName: "never"}), expected);
        assert.sameValue(dateWithMonthCode.toString({calendarName: "never"}), expected);
      }
    }

    // Year after end of era.
    let yearAfterLastEraYear = lastEraYear + 1;
    for (let month = 1; month <= 12; ++month) {
      for (let day = 1; day <= 31; ++day) {
        let monthCode = "M" + String(month).padStart(2, "0");

        let dateWithMonth = Temporal.PlainDate.from({
          calendar: "japanese",
          era,
          eraYear: yearAfterLastEraYear,
          month,
          day,
        });

        let dateWithMonthCode = Temporal.PlainDate.from({
          calendar: "japanese",
          era,
          eraYear: yearAfterLastEraYear,
          monthCode,
          day,
        });

        let expected = eraEnd.add({years: 1})
                             .with({month, day})
                             .toString();

        assert.sameValue(dateWithMonth.toString({calendarName: "never"}), expected);
        assert.sameValue(dateWithMonthCode.toString({calendarName: "never"}), expected);
      }
    }
  }
}

