// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Instant.toLocaleString()
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

var instant = Temporal.Instant.from("1976-11-18T14:23:30Z");
assert.sameValue(
  `${instant.toLocaleString("en-US", { timeZone: "America/New_York" })}`,
  `11/18/1976, 9:23:30${usDayPeriodSpace}AM`
);
assert.sameValue(`${instant.toLocaleString("de-AT", { timeZone: "Europe/Vienna" })}`, "18.11.1976, 15:23:30");
var fmt = maybeGetWeekdayOnlyFormat();
if (fmt) assert.sameValue(fmt.format(instant), "Thursday");

// outputs timeZoneName if requested
var str = instant.toLocaleString("en-US", {
  timeZone: "America/New_York",
  timeZoneName: "short"
});
assert(str.includes("EST"));
