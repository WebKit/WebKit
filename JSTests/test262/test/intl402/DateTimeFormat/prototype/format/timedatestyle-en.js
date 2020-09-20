// Copyright 2019 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date-time-style-pattern
description: Checks basic handling of timeStyle and dateStyle.
features: [Intl.DateTimeFormat-datetimestyle]
locale: [en-US]
---*/

const date = new Date("1886-05-01T14:12:47Z");
const dateOptions = [
  ["full", "Saturday, May 1, 1886"],
  ["long", "May 1, 1886"],
  ["medium", "May 1, 1886"],
  ["short", "5/1/86"],
];

const timeOptions = [
  ["full", "2:12:47 PM Coordinated Universal Time", "14:12:47 Coordinated Universal Time"],
  ["long", "2:12:47 PM UTC", "14:12:47 UTC"],
  ["medium", "2:12:47 PM", "14:12:47"],
  ["short", "2:12 PM", "14:12"],
];

const options12 = [
  { "hour12": true },
  { "hourCycle": "h11" },
  { "hourCycle": "h12" },
  { "hourCycle": "h23", "hour12": true },
  { "hourCycle": "h24", "hour12": true },
];

const options24 = [
  { "hour12": false },
  { "hourCycle": "h23" },
  { "hourCycle": "h24" },
  { "hourCycle": "h11", "hour12": false },
  { "hourCycle": "h12", "hour12": false },
];

for (const [dateStyle, expected] of dateOptions) {
  const dtf = new Intl.DateTimeFormat("en-US", {
    dateStyle,
    timeZone: "UTC",
  });

  const dateString = dtf.format(date);
  assert.sameValue(dateString, expected, `Result for ${dateStyle}`);
}

for (const [timeStyle, expected12, expected24] of timeOptions) {
  const check = (locale, options, expected) => {
    const dtf = new Intl.DateTimeFormat(locale, {
      timeStyle,
      timeZone: "UTC",
      ...options
    });

    const dateString = dtf.format(date);
    assert.sameValue(dateString, expected, `Result for ${timeStyle} with ${JSON.stringify(options)}`);
  };

  check("en-US", {}, expected12);
  check("en-US-u-hc-h11", {}, expected12);
  check("en-US-u-hc-h12", {}, expected12);
  check("en-US-u-hc-h23", {}, expected24);
  check("en-US-u-hc-h24", {}, expected24);

  for (const hourOptions of options12) {
    check("en-US", hourOptions, expected12);
    check("en-US-u-hc-h11", hourOptions, expected12);
    check("en-US-u-hc-h12", hourOptions, expected12);
    check("en-US-u-hc-h23", hourOptions, expected12);
    check("en-US-u-hc-h24", hourOptions, expected12);
  }

  for (const hourOptions of options24) {
    check("en-US", hourOptions, expected24);
    check("en-US-u-hc-h11", hourOptions, expected24);
    check("en-US-u-hc-h12", hourOptions, expected24);
    check("en-US-u-hc-h23", hourOptions, expected24);
    check("en-US-u-hc-h24", hourOptions, expected24);
  }
}

for (const [dateStyle, expectedDate] of dateOptions) {
  for (const [timeStyle, expectedTime] of timeOptions) {
    const dtf = new Intl.DateTimeFormat("en-US", {
      dateStyle,
      timeStyle,
      timeZone: "UTC",
    });
    const result1 = [expectedDate, ", ", expectedTime].join("");
    const result2 = [expectedDate, " at ", expectedTime].join("");

    const dateString = dtf.format(date);
    assert.sameValue(
      [result1, result2].includes(dateString),
      true,
      `Result for date=${dateStyle} and time=${timeStyle}`
    );
  }
}
