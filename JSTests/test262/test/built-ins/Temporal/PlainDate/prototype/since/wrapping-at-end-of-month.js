// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.since
description: Tests balancing of days to months at end of month
includes: [temporalHelpers.js]
features: [Temporal]
---*/

// Difference between end of longer month to end of following shorter month
{
  const end = new Temporal.PlainDate(1970, 2, 28);
  for (const largestUnit of ["years", "months"]) {
    TemporalHelpers.assertDuration(
      new Temporal.PlainDate(1970, 1, 28).since(end, { largestUnit }),
      0, -1, 0, 0, 0, 0, 0, 0, 0, 0,
      "Jan 28th to Feb 28th is one month"
    );
    TemporalHelpers.assertDuration(
      new Temporal.PlainDate(1970, 1, 29).since(end, { largestUnit }),
      0, 0, 0, -30, 0, 0, 0, 0, 0, 0,
      "Jan 29th to Feb 28th is 30 days, not one month"
    );
    TemporalHelpers.assertDuration(
      new Temporal.PlainDate(1970, 1, 30).since(end, { largestUnit }),
      0, 0, 0, -29, 0, 0, 0, 0, 0, 0,
      "Jan 30th to Feb 28th is 29 days, not one month"
    );
    TemporalHelpers.assertDuration(
      new Temporal.PlainDate(1970, 1, 31).since(end, { largestUnit }),
      0, 0, 0, -28, 0, 0, 0, 0, 0, 0,
      "Jan 31st to Feb 28th is 28 days, not one month"
    );
  }
}

// Difference between end of leap-year January to end of leap-year February
{
  const end = new Temporal.PlainDate(1972, 2, 29);
  for (const largestUnit of ["years", "months"]) {
    TemporalHelpers.assertDuration(
      new Temporal.PlainDate(1972, 1, 29).since(end, { largestUnit }),
      0, -1, 0, 0, 0, 0, 0, 0, 0, 0,
      "Jan 29th to Feb 29th is one month"
    );
    TemporalHelpers.assertDuration(
      new Temporal.PlainDate(1972, 1, 30).since(end, { largestUnit }),
      0, 0, 0, -30, 0, 0, 0, 0, 0, 0,
      "Jan 30th to Feb 29th is 30 days, not one month"
    );
    TemporalHelpers.assertDuration(
      new Temporal.PlainDate(1972, 1, 31).since(end, { largestUnit }),
      0, 0, 0, -29, 0, 0, 0, 0, 0, 0,
      "Jan 31st to Feb 29th is 29 days, not one month"
    );
  }
}

// Difference between end of longer month to end of not-immediately-following
// shorter month
{
  const end = new Temporal.PlainDate(1970, 11, 30);
  for (const largestUnit of ["years", "months"]) {
    TemporalHelpers.assertDuration(
      new Temporal.PlainDate(1970, 8, 30).since(end, { largestUnit }),
      0, -3, 0, 0, 0, 0, 0, 0, 0, 0,
      "Aug 30th to Nov 30th is 3 months"
    );
    TemporalHelpers.assertDuration(
      new Temporal.PlainDate(1970, 8, 31).since(end, { largestUnit }),
      0, -2, 0, -30, 0, 0, 0, 0, 0, 0,
      "Aug 31st to Nov 30th is 2 months 30 days, not 3 months"
    );
  }
}

// Difference between end of longer month in one year to shorter month in
// later year
{
  const end = new Temporal.PlainDate(1973, 4, 30);
  TemporalHelpers.assertDuration(
    new Temporal.PlainDate(1970, 12, 30).since(end, { largestUnit: "months" }),
    0, -28, 0, 0, 0, 0, 0, 0, 0, 0,
    "Dec 30th 1970 to Apr 30th 1973 is 28 months"
  );
  TemporalHelpers.assertDuration(
    new Temporal.PlainDate(1970, 12, 30).since(end, { largestUnit: "years" }),
    -2, -4, 0, 0, 0, 0, 0, 0, 0, 0,
    "Dec 30th 1970 to Apr 30th 1973 is 2 years, 4 months"
  );
  TemporalHelpers.assertDuration(
    new Temporal.PlainDate(1970, 12, 31).since(end, { largestUnit: "months" }),
    0, -27, 0, -30, 0, 0, 0, 0, 0, 0,
    "Dec 30th 1970 to Apr 30th 1973 is 27 months, 30 days, not 28 months"
  );
  TemporalHelpers.assertDuration(
    new Temporal.PlainDate(1970, 12, 31).since(end, { largestUnit: "years" }),
    -2, -3, 0, -30, 0, 0, 0, 0, 0, 0,
    "Dec 30th 1970 to Apr 30th 1973 is 2 years, 3 months, 30 days, not 2 years 4 months"
  );
}

// Difference where months passes through a month that's the same length or
// shorter than either the start or end month
{
  TemporalHelpers.assertDuration(
    new Temporal.PlainDate(1970, 1, 29).since(new Temporal.PlainDate(1970, 3, 28), { largestUnit: "months" }),
    0, -1, 0, -28, 0, 0, 0, 0, 0, 0,
    "Jan 29th to Mar 28th is 1 month 28 days, not 58 days"
  );
  TemporalHelpers.assertDuration(
    new Temporal.PlainDate(1970, 1, 31).since(new Temporal.PlainDate(1971, 5, 30), { largestUnit: "years" }),
    -1, -3, 0, -30, 0, 0, 0, 0, 0, 0,
    "Jan 31st 1970 to May 30th 1971 is 1 year, 3 months, 30 days, not 1 year, 2 months, 60 days"
  );
}
