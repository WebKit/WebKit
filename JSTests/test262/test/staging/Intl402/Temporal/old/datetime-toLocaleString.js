// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: datetime.toLocaleString()
features: [Temporal]
---*/

// Tolerate implementation variance by expecting consistency without being prescriptive.
// TODO: can we change tests to be less reliant on CLDR formats while still testing that
// Temporal and Intl are behaving as expected?
const usDayPeriodSpace =
  new Intl.DateTimeFormat("en-US", { timeStyle: "short" })
    .formatToParts(0)
    .find((part, i, parts) => part.type === "literal" && parts[i + 1].type === "dayPeriod")?.value || "";

function maybeGetWeekdayOnlyFormat() {
  const fmt = new Intl.DateTimeFormat("en-US", { weekday: "long", timeZone: "Europe/Vienna" });
  if (
    ["era", "year", "month", "day", "hour", "minute", "second", "timeZoneName"].some(
      (prop) => prop in fmt.resolvedOptions()
    )
  ) {
    // no weekday-only format available
    return null;
  }
  return fmt;
}

var datetime = Temporal.PlainDateTime.from("1976-11-18T15:23:30");
assert.sameValue(
  `${datetime.toLocaleString("en-US", { timeZone: "America/New_York" })}`,
  `11/18/1976, 3:23:30${usDayPeriodSpace}PM`
);
assert.sameValue(`${datetime.toLocaleString("de-AT", { timeZone: "Europe/Vienna" })}`, "18.11.1976, 15:23:30");
var fmt = maybeGetWeekdayOnlyFormat();
if (fmt) assert.sameValue(fmt.format(datetime), "Thursday");

// should use compatible disambiguation option
var dstStart = new Temporal.PlainDateTime(2020, 3, 8, 2, 30);
assert.sameValue(
  `${dstStart.toLocaleString("en-US", { timeZone: "America/Los_Angeles" })}`,
  `3/8/2020, 3:30:00${usDayPeriodSpace}AM`
);
