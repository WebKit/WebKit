// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone
description: >
  TimeZone constructor canonicalises its input.
features: [Temporal]
---*/

const testCases = {
  "Etc/GMT": "UTC",
  "Etc/GMT+0": "UTC",
  "Etc/GMT-0": "UTC",
  "Etc/GMT0": "UTC",
  "Etc/Greenwich": "UTC",
  "Etc/UCT": "UTC",
  "Etc/UTC": "UTC",
  "Etc/Universal": "UTC",
  "Etc/Zulu": "UTC",
};

for (let [id, canonical] of Object.entries(testCases)) {
  let tz = new Temporal.TimeZone(id);

  assert.sameValue(tz.id, canonical);
}
