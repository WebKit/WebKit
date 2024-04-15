// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.until
description: Tests balancing of days to months at end of month
includes: [temporalHelpers.js]
features: [Temporal]
---*/

// Difference between end of longer month to end of following shorter month
{
  const end = new Temporal.ZonedDateTime(5011200_000_000_000n /* = 1970-02-28T00Z */, "UTC");
  for (const largestUnit of ["years", "months"]) {
    TemporalHelpers.assertDuration(
      new Temporal.ZonedDateTime(2332800_000_000_000n /* = 1970-01-28T00Z */, "UTC").until(end, { largestUnit }),
      0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
      "Jan 28th to Feb 28th is one month"
    );
    TemporalHelpers.assertDuration(
      new Temporal.ZonedDateTime(2419200_000_000_000n /* = 1970-01-29T00Z */, "UTC").until(end, { largestUnit }),
      0, 0, 0, 30, 0, 0, 0, 0, 0, 0,
      "Jan 29th to Feb 28th is 30 days, not one month"
    );
    TemporalHelpers.assertDuration(
      new Temporal.ZonedDateTime(2505600_000_000_000n /* = 1970-01-30T00Z */, "UTC").until(end, { largestUnit }),
      0, 0, 0, 29, 0, 0, 0, 0, 0, 0,
      "Jan 30th to Feb 28th is 29 days, not one month"
    );
    TemporalHelpers.assertDuration(
      new Temporal.ZonedDateTime(2592000_000_000_000n /* = 1970-01-31T00Z */, "UTC").until(end, { largestUnit }),
      0, 0, 0, 28, 0, 0, 0, 0, 0, 0,
      "Jan 31st to Feb 28th is 28 days, not one month"
    );
  }
}

// Difference between end of leap-year January to end of leap-year February
{
  const end = new Temporal.ZonedDateTime(68169600_000_000_000n /* = 1972-02-29T00Z */, "UTC");
  for (const largestUnit of ["years", "months"]) {
    TemporalHelpers.assertDuration(
      new Temporal.ZonedDateTime(65491200_000_000_000n /* = 1972-01-29T00Z */, "UTC").until(end, { largestUnit }),
      0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
      "Jan 29th to Feb 29th is one month"
    );
    TemporalHelpers.assertDuration(
      new Temporal.ZonedDateTime(65577600_000_000_000n /* = 1972-01-30T00Z */, "UTC").until(end, { largestUnit }),
      0, 0, 0, 30, 0, 0, 0, 0, 0, 0,
      "Jan 30th to Feb 29th is 30 days, not one month"
    );
    TemporalHelpers.assertDuration(
      new Temporal.ZonedDateTime(65664000_000_000_000n /* = 1972-01-31T00Z */, "UTC").until(end, { largestUnit }),
      0, 0, 0, 29, 0, 0, 0, 0, 0, 0,
      "Jan 31st to Feb 29th is 29 days, not one month"
    );
  }
}

// Difference between end of longer month to end of not-immediately-following
// shorter month
{
  const end = new Temporal.ZonedDateTime(28771200_000_000_000n /* = 1970-11-30T00Z */, "UTC");
  for (const largestUnit of ["years", "months"]) {
    TemporalHelpers.assertDuration(
      new Temporal.ZonedDateTime(20822400_000_000_000n /* = 1970-08-30T00Z */, "UTC").until(end, { largestUnit }),
      0, 3, 0, 0, 0, 0, 0, 0, 0, 0,
      "Aug 30th to Nov 30th is 3 months"
    );
    TemporalHelpers.assertDuration(
      new Temporal.ZonedDateTime(20908800_000_000_000n /* = 1970-08-31T00Z */, "UTC").until(end, { largestUnit }),
      0, 2, 0, 30, 0, 0, 0, 0, 0, 0,
      "Aug 31st to Nov 30th is 2 months 30 days, not 3 months"
    );
  }
}

// Difference between end of longer month in one year to shorter month in
// later year
{
  const end = new Temporal.ZonedDateTime(104976000_000_000_000n /* = 1973-04-30T00Z */, "UTC");
  TemporalHelpers.assertDuration(
    new Temporal.ZonedDateTime(31363200_000_000_000n /* = 1970-12-30T00Z */, "UTC").until(end, { largestUnit: "months" }),
    0, 28, 0, 0, 0, 0, 0, 0, 0, 0,
    "Dec 30th 1970 to Apr 30th 1973 is 28 months"
  );
  TemporalHelpers.assertDuration(
    new Temporal.ZonedDateTime(31363200_000_000_000n /* = 1970-12-30T00Z */, "UTC").until(end, { largestUnit: "years" }),
    2, 4, 0, 0, 0, 0, 0, 0, 0, 0,
    "Dec 30th 1970 to Apr 30th 1973 is 2 years, 4 months"
  );
  TemporalHelpers.assertDuration(
    new Temporal.ZonedDateTime(31449600_000_000_000n /* = 1970-12-31T00Z */, "UTC").until(end, { largestUnit: "months" }),
    0, 27, 0, 30, 0, 0, 0, 0, 0, 0,
    "Dec 30th 1970 to Apr 30th 1973 is 27 months, 30 days, not 28 months"
  );
  TemporalHelpers.assertDuration(
    new Temporal.ZonedDateTime(31449600_000_000_000n /* = 1970-12-31T00Z */, "UTC").until(end, { largestUnit: "years" }),
    2, 3, 0, 30, 0, 0, 0, 0, 0, 0,
    "Dec 30th 1970 to Apr 30th 1973 is 2 years, 3 months, 30 days, not 2 years 4 months"
  );
}

// Difference where months passes through a month that's the same length or
// shorter than either the start or end month
{
  TemporalHelpers.assertDuration(
    new Temporal.ZonedDateTime(2419200_000_000_000n /* = 1970-01-29T00Z */, "UTC").until(new Temporal.ZonedDateTime(7430400_000_000_000n /* = 1970-03-28T00Z */, "UTC"), { largestUnit: "months" }),
    0, 1, 0, 28, 0, 0, 0, 0, 0, 0,
    "Jan 29th to Mar 28th is 1 month 28 days, not 58 days"
  );
  TemporalHelpers.assertDuration(
    new Temporal.ZonedDateTime(2592000_000_000_000n /* = 1970-01-31T00Z */, "UTC").until(new Temporal.ZonedDateTime(44409600_000_000_000n /* = 1971-05-30T00Z */, "UTC"), { largestUnit: "years" }),
    1, 3, 0, 30, 0, 0, 0, 0, 0, 0,
    "Jan 31st 1970 to May 30th 1971 is 1 year, 3 months, 30 days, not 1 year, 2 months, 60 days"
  );
}
