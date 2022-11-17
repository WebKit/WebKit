// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthsinyear
description: Various forms of time zone annotation; critical flag has no effect
features: [Temporal]
---*/

const tests = [
  ["2000-05-02[Asia/Kolkata]", "named, with no time and no offset"],
  ["2000-05-02[!Europe/Vienna]", "named, with !, no time, and no offset"],
  ["2000-05-02[+00:00]", "numeric, with no time and no offset"],
  ["2000-05-02[!-02:30]", "numeric, with !, no time, and no offset"],
  ["2000-05-02+00:00[UTC]", "named, with offset and no time"],
  ["2000-05-02+00:00[!Africa/Abidjan]", "named, with offset, !, and no time"],
  ["2000-05-02+00:00[-08:00]", "numeric, with offset and no time"],
  ["2000-05-02+00:00[!+01:00]", "numeric, with offset, !, and no time"],
  ["2000-05-02T15:23[America/Sao_Paulo]", "named, with no offset"],
  ["2000-05-02T15:23[!Asia/Tokyo]", "named, with ! and no offset"],
  ["2000-05-02T15:23[-02:30]", "numeric, with no offset"],
  ["2000-05-02T15:23[!+00:00]", "numeric, with ! and no offset"],
  ["2000-05-02T15:23+00:00[America/New_York]", "named, with offset"],
  ["2000-05-02T15:23+00:00[!UTC]", "named, with offset and !"],
  ["2000-05-02T15:23+00:00[+01:00]", "numeric, with offset"],
  ["2000-05-02T15:23+00:00[!-08:00]", "numeric, with offset and !"],
];

const instance = new Temporal.Calendar("iso8601");

tests.forEach(([arg, description]) => {
  const result = instance.monthsInYear(arg);

  assert.sameValue(
    result,
    12,
    `time zone annotation (${description})`
  );
});
