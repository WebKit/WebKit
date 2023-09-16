// Copyright (C) 2023 Justin Grant. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.from
description: Built-in time zones are compared correctly out of valid strings
features: [Temporal]
---*/

const validsEqual = [
  ["+0330", "+03:30"],
  ["-0650", "-06:50"],
  ["-08", "-08:00"],
  ["\u221201:00", "-01:00"],
  ["\u22120650", "-06:50"],
  ["\u221208", "-08:00"],
  ["1994-11-05T08:15:30-05:00", "-05:00"],
  ["1994-11-05T08:15:30\u221205:00", "-05:00"],
  ["1994-11-05T13:15:30Z", "UTC"]
];

for (const [valid, canonical] of validsEqual) {
  const tzValid = Temporal.TimeZone.from(valid);
  const tzCanonical = Temporal.TimeZone.from(canonical);
  assert.sameValue(tzValid.equals(canonical), true);
  assert.sameValue(tzCanonical.equals(valid), true);
}

const validsNotEqual = [
  ["+0330", "+03:31"],
  ["-0650", "-06:51"],
  ["-08", "-08:01"],
  ["\u221201:00", "-01:01"],
  ["\u22120650", "-06:51"],
  ["\u221208", "-08:01"],
  ["1994-11-05T08:15:30-05:00", "-05:01"],
  ["1994-11-05T08:15:30\u221205:00", "-05:01"]
];

for (const [valid, canonical] of validsNotEqual) {
  const tzValid = Temporal.TimeZone.from(valid);
  const tzCanonical = Temporal.TimeZone.from(canonical);
  assert.sameValue(tzValid.equals(canonical), false);
  assert.sameValue(tzCanonical.equals(valid), false);
}
