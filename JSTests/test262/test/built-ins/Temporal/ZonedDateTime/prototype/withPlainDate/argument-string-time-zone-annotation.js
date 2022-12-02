// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withplaindate
description: Various forms of time zone annotation; critical flag has no effect
features: [Temporal]
---*/

const tests = [
  ["2000-05-02[Asia/Kolkata]", "named, with no time"],
  ["2000-05-02[!Europe/Vienna]", "named, with ! and no time"],
  ["2000-05-02[+00:00]", "numeric, with no time"],
  ["2000-05-02[!-02:30]", "numeric, with ! and no time"],
  ["2000-05-02T15:23[America/Sao_Paulo]", "named, with no offset"],
  ["2000-05-02T15:23[!Asia/Tokyo]", "named, with ! and no offset"],
  ["2000-05-02T15:23[-02:30]", "numeric, with no offset"],
  ["2000-05-02T15:23[!+00:00]", "numeric, with ! and no offset"],
  ["2000-05-02T15:23+00:00[America/New_York]", "named, with offset"],
  ["2000-05-02T15:23+00:00[!UTC]", "named, with offset and !"],
  ["2000-05-02T15:23+00:00[+01:00]", "numeric, with offset"],
  ["2000-05-02T15:23+00:00[!-08:00]", "numeric, with offset and !"],
];

const timeZone = new Temporal.TimeZone("UTC");
const instance = new Temporal.ZonedDateTime(0n, timeZone);

tests.forEach(([arg, description]) => {
  const result = instance.withPlainDate(arg);

  assert.sameValue(
    result.epochNanoseconds,
    957_225_600_000_000_000n,
    `time zone annotation (${description})`
  );
});
