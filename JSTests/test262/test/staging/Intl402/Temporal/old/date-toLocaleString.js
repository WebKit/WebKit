// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: date.toLocaleString()
features: [Temporal]
---*/

function maybeGetWeekdayOnlyFormat() {
  const fmt = new Intl.DateTimeFormat('en', { weekday: 'long', timeZone: 'Europe/Vienna' });
  if (
    ['era', 'year', 'month', 'day', 'hour', 'minute', 'second', 'timeZoneName'].some(
      (prop) => prop in fmt.resolvedOptions()
    )
  ) {
   //no weekday-only format available 
    return null;
  }
  return fmt;
}

var date = Temporal.PlainDate.from("1976-11-18T15:23:30");
assert.sameValue(`${ date.toLocaleString("en", { timeZone: "America/New_York" }) }`, "11/18/1976")
assert.sameValue(`${ date.toLocaleString("de", { timeZone: "Europe/Vienna" }) }`, "18.11.1976")
var fmt = maybeGetWeekdayOnlyFormat();
if (fmt) assert.sameValue(fmt.format(date), "Thursday");

// should ignore units not in the data type
assert.sameValue(date.toLocaleString("en", { timeZoneName: "long" }), "11/18/1976");
assert.sameValue(date.toLocaleString("en", { hour: "numeric" }), "11/18/1976");
assert.sameValue(date.toLocaleString("en", { minute: "numeric" }), "11/18/1976");
assert.sameValue(date.toLocaleString("en", { second: "numeric" }), "11/18/1976");

// works when the object's calendar is the same as the locale's calendar
var d = Temporal.PlainDate.from({
  era: "showa",
  eraYear: 51,
  month: 11,
  day: 18,
  calendar: "japanese"
});
var result = d.toLocaleString("en-US-u-ca-japanese");
assert(result === "11/18/51" || result === "11/18/51 S");

// adopts the locale's calendar when the object's calendar is ISO
var d = Temporal.PlainDate.from("1976-11-18");
var result = d.toLocaleString("en-US-u-ca-japanese");
assert(result === "11/18/51" || result === "11/18/51 S");

// throws when the calendars are different and not ISO
var d = Temporal.PlainDate.from({
  year: 1976,
  month: 11,
  day: 18,
  calendar: "gregory"
});
assert.throws(RangeError, () => d.toLocaleString("en-US-u-ca-japanese"));
