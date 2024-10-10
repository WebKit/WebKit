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
assert.sameValue(`${ date.toLocaleString("en-US", { timeZone: "America/New_York" }) }`, "11/18/1976");
assert.sameValue(`${ date.toLocaleString("de-AT", { timeZone: "Europe/Vienna" }) }`, "18.11.1976");
var fmt = maybeGetWeekdayOnlyFormat();
if (fmt) assert.sameValue(fmt.format(date), "Thursday");
