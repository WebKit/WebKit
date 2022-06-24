// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Time separator in string argument can vary
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const date = new Temporal.PlainDate(2000, 5, 3);
const tests = [
  ["2000-05-02T15:23", "uppercase T"],
  ["2000-05-02t15:23", "lowercase T"],
  ["2000-05-02 15:23", "space between date and time"],
];

const instance = new Temporal.Calendar("iso8601");

tests.forEach(([arg, description]) => {
  TemporalHelpers.assertDuration(
    instance.dateUntil(arg, date),
    0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
    `variant time separators (${description}), first argument`
  );

  TemporalHelpers.assertDuration(
    instance.dateUntil(date, arg),
    0, 0, 0, -1, 0, 0, 0, 0, 0, 0,
    `variant time separators (${description}), second argument`
  );
});
