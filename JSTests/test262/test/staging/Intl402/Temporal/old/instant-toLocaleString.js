// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Instant.toLocaleString()
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

var instant = Temporal.Instant.from("1976-11-18T14:23:30Z");
assert.sameValue(`${ instant.toLocaleString("en", { timeZone: "America/New_York" }) }`, "11/18/1976, 9:23:30 AM")
assert.sameValue(`${ instant.toLocaleString("de", { timeZone: "Europe/Vienna" }) }`, "18.11.1976, 15:23:30")
var fmt = maybeGetWeekdayOnlyFormat();
if (fmt)
  assert.sameValue(fmt.format(instant), "Thursday");

// outputs timeZoneName if requested
var str = instant.toLocaleString("en", {
  timeZone: "America/New_York",
  timeZoneName: "short"
});
assert(str.includes("EST"));
