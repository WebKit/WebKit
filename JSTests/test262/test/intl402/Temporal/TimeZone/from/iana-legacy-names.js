// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone
description: IANA legacy names must be supported
features: [Temporal]
---*/

const legacyNames = [
  ["Etc/GMT0", "UTC"],
  ["GMT0", "UTC"],
  ["GMT-0", "UTC"],
  ["GMT+0", "UTC"],
  ["EST5EDT", "EST5EDT"],
  ["CST6CDT", "CST6CDT"],
  ["MST7MDT", "MST7MDT"],
  ["PST8PDT", "PST8PDT"],
];

legacyNames.forEach(([arg, expectedID]) => {
  const tz = Temporal.TimeZone.from(arg);
  assert.sameValue(tz.toString(), expectedID, `"${arg}" is a supported name for the "${expectedID}" time zone`);
});
