// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: zoneddatetime.toLocaleString()
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

var zdt = Temporal.ZonedDateTime.from("1976-11-18T15:23:30+01:00[Europe/Vienna]");
assert.sameValue(zdt.toLocaleString("en"), "11/18/1976, 3:23:30 PM GMT+1")
assert.sameValue(zdt.toLocaleString("de"), "18.11.1976, 15:23:30 MEZ")

const fmt = maybeGetWeekdayOnlyFormat();
//uses only the options in resolvedOptions 
if (fmt) assert.sameValue(fmt.format(zdt), 'Thursday');
// can override the style of the time zone name
assert.sameValue(zdt.toLocaleString("en", { timeZoneName: "long" }), "11/18/1976, 3:23:30 PM Central European Standard Time");

// works if the time zone given in options agrees with the object's time zone
assert.sameValue(zdt.toLocaleString("en", { timeZone: "Europe/Vienna" }), "11/18/1976, 3:23:30 PM GMT+1");

// throws if the time zone given in options disagrees with the object's time zone
assert.throws(RangeError, () => zdt.toLocaleString("en", { timeZone: "America/New_York" }));

// works when the object's calendar is the same as the locale's calendar
var zdt = new Temporal.ZonedDateTime(0n, "UTC", "japanese");
var result = zdt.toLocaleString("en-US-u-ca-japanese");
assert(result === "1/1/45, 12:00:00 AM UTC" || result === "1/1/45 S, 12:00:00 AM UTC");

// adopts the locale's calendar when the object's calendar is ISO
var zdt = Temporal.ZonedDateTime.from("1976-11-18T15:23:30+00:00[UTC]");
var result = zdt.toLocaleString("en-US-u-ca-japanese");
assert(result === "11/18/51, 3:23:30 PM UTC" || result === "11/18/51 S, 3:23:30 PM UTC");

// throws when the calendars are different and not ISO
var zdt = new Temporal.ZonedDateTime(0n, "UTC", "gregory");
assert.throws(RangeError, () => zdt.toLocaleString("en-US-u-ca-japanese"));
